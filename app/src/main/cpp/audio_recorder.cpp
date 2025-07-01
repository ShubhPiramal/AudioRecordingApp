#include <jni.h>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <string>
#include <vector>
#include <fstream>
#include <thread>
#include <atomic>
#include <mutex>

#define LOG_TAG "AudioRecorder"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

class AudioRecorder {
private:
    SLObjectItf engineObject = nullptr;
    SLEngineItf engineEngine = nullptr;
    SLObjectItf recorderObject = nullptr;
    SLRecordItf recorderRecord = nullptr;
    SLAndroidSimpleBufferQueueItf recorderBufferQueue = nullptr;
    
    std::atomic<bool> isRecording{false};
    std::vector<short> audioBuffer;
    std::mutex bufferMutex;
    std::string outputFilePath;
    
    static const int SAMPLE_RATE = 44100;
    static const int CHANNELS = 1;
    static const int BITS_PER_SAMPLE = 16;
    static const int BUFFER_SIZE = 4096;
    
    short recordBuffer[BUFFER_SIZE];
    
public:
    AudioRecorder() = default;
    ~AudioRecorder() {
        cleanup();
    }
    
    bool initialize() {
        SLresult result;
        
        // Create engine
        result = slCreateEngine(&engineObject, 0, nullptr, 0, nullptr, nullptr);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to create engine");
            return false;
        }
        
        // Realize engine
        result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to realize engine");
            return false;
        }
        
        // Get engine interface
        result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to get engine interface");
            return false;
        }
        
        return true;
    }
    
    bool startRecording(const std::string& filePath) {
        if (isRecording) {
            LOGE("Already recording");
            return false;
        }
        
        outputFilePath = filePath;
        audioBuffer.clear();
        
        SLresult result;
        
        // Configure audio source
        SLDataLocator_IODevice loc_dev = {SL_DATALOCATOR_IODEVICE, SL_IODEVICE_AUDIOINPUT,
                                          SL_DEFAULTDEVICEID_AUDIOINPUT, nullptr};
        SLDataSource audioSrc = {&loc_dev, nullptr};
        
        // Configure audio sink
        SLDataLocator_AndroidSimpleBufferQueue loc_bq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
        SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, CHANNELS, SL_SAMPLINGRATE_44_1,
                                       SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                       SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
        SLDataSink audioSnk = {&loc_bq, &format_pcm};
        
        // Create audio recorder
        const SLInterfaceID id[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[1] = {SL_BOOLEAN_TRUE};
        result = (*engineEngine)->CreateAudioRecorder(engineEngine, &recorderObject, &audioSrc,
                                                       &audioSnk, 1, id, req);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to create audio recorder");
            return false;
        }
        
        // Realize the audio recorder
        result = (*recorderObject)->Realize(recorderObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to realize audio recorder");
            return false;
        }
        
        // Get the record interface
        result = (*recorderObject)->GetInterface(recorderObject, SL_IID_RECORD, &recorderRecord);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to get record interface");
            return false;
        }
        
        // Get the buffer queue interface
        result = (*recorderObject)->GetInterface(recorderObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                                                  &recorderBufferQueue);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to get buffer queue interface");
            return false;
        }
        
        // Register callback
        result = (*recorderBufferQueue)->RegisterCallback(recorderBufferQueue, bqRecorderCallback, this);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to register callback");
            return false;
        }
        
        // Enqueue initial buffer
        result = (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recordBuffer, BUFFER_SIZE * sizeof(short));
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to enqueue buffer");
            return false;
        }
        
        // Start recording
        result = (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_RECORDING);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to start recording");
            return false;
        }
        
        isRecording = true;
        LOGI("Recording started");
        return true;
    }
    
    bool stopRecording() {
        if (!isRecording) {
            LOGE("Not recording");
            return false;
        }
        
        isRecording = false;
        
        if (recorderRecord) {
            (*recorderRecord)->SetRecordState(recorderRecord, SL_RECORDSTATE_STOPPED);
        }
        
        // Save audio data to file
        saveToFile();
        
        // Cleanup recorder
        if (recorderObject) {
            (*recorderObject)->Destroy(recorderObject);
            recorderObject = nullptr;
            recorderRecord = nullptr;
            recorderBufferQueue = nullptr;
        }
        
        LOGI("Recording stopped");
        return true;
    }
    
    bool isCurrentlyRecording() const {
        return isRecording;
    }
    
private:
    static void bqRecorderCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
        AudioRecorder* recorder = static_cast<AudioRecorder*>(context);
        recorder->processAudioData();
    }
    
    void processAudioData() {
        if (!isRecording) return;
        
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            audioBuffer.insert(audioBuffer.end(), recordBuffer, recordBuffer + BUFFER_SIZE);
        }
        
        // Re-enqueue the buffer
        if (recorderBufferQueue && isRecording) {
            (*recorderBufferQueue)->Enqueue(recorderBufferQueue, recordBuffer, BUFFER_SIZE * sizeof(short));
        }
    }
    
    void saveToFile() {
        std::lock_guard<std::mutex> lock(bufferMutex);
        
        if (audioBuffer.empty()) {
            LOGE("No audio data to save");
            return;
        }
        
        std::ofstream file(outputFilePath, std::ios::binary);
        if (!file.is_open()) {
            LOGE("Failed to open output file: %s", outputFilePath.c_str());
            return;
        }
        
        // Write WAV header
        writeWavHeader(file, audioBuffer.size());
        
        // Write audio data
        file.write(reinterpret_cast<const char*>(audioBuffer.data()), 
                   audioBuffer.size() * sizeof(short));
        
        file.close();
        LOGI("Audio saved to: %s", outputFilePath.c_str());
    }
    
    void writeWavHeader(std::ofstream& file, size_t dataSize) {
        uint32_t fileSize = 36 + dataSize * sizeof(short);
        uint32_t byteRate = SAMPLE_RATE * CHANNELS * BITS_PER_SAMPLE / 8;
        uint16_t blockAlign = CHANNELS * BITS_PER_SAMPLE / 8;
        uint32_t subchunk2Size = dataSize * sizeof(short);
        
        // RIFF header
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char*>(&fileSize), 4);
        file.write("WAVE", 4);
        
        // fmt subchunk
        file.write("fmt ", 4);
        uint32_t subchunk1Size = 16;
        file.write(reinterpret_cast<const char*>(&subchunk1Size), 4);
        uint16_t audioFormat = 1; // PCM
        file.write(reinterpret_cast<const char*>(&audioFormat), 2);
        uint16_t numChannels = CHANNELS;
        file.write(reinterpret_cast<const char*>(&numChannels), 2);
        uint32_t sampleRate = SAMPLE_RATE;
        file.write(reinterpret_cast<const char*>(&sampleRate), 4);
        file.write(reinterpret_cast<const char*>(&byteRate), 4);
        file.write(reinterpret_cast<const char*>(&blockAlign), 2);
        uint16_t bitsPerSample = BITS_PER_SAMPLE;
        file.write(reinterpret_cast<const char*>(&bitsPerSample), 2);
        
        // data subchunk
        file.write("data", 4);
        file.write(reinterpret_cast<const char*>(&subchunk2Size), 4);
    }
    
    void cleanup() {
        if (isRecording) {
            stopRecording();
        }
        
        if (recorderObject) {
            (*recorderObject)->Destroy(recorderObject);
            recorderObject = nullptr;
        }
        
        if (engineObject) {
            (*engineObject)->Destroy(engineObject);
            engineObject = nullptr;
        }
    }
};

class AudioPlayer {
private:
    SLObjectItf engineObject = nullptr;
    SLEngineItf engineEngine = nullptr;
    SLObjectItf playerObject = nullptr;
    SLPlayItf playerPlay = nullptr;
    SLAndroidSimpleBufferQueueItf playerBufferQueue = nullptr;
    
    std::atomic<bool> isPlaying{false};
    std::vector<short> audioData;
    std::mutex playerMutex;
    size_t currentPosition = 0;
    
    static const int SAMPLE_RATE = 44100;
    static const int CHANNELS = 1;
    static const int BUFFER_SIZE = 4096;
    
public:
    AudioPlayer() = default;
    ~AudioPlayer() {
        cleanup();
    }
    
    bool initialize() {
        SLresult result;
        
        // Create engine
        result = slCreateEngine(&engineObject, 0, nullptr, 0, nullptr, nullptr);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to create player engine");
            return false;
        }
        
        // Realize engine
        result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to realize player engine");
            return false;
        }
        
        // Get engine interface
        result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to get player engine interface");
            return false;
        }
        
        return true;
    }
    
    bool loadAudioFile(const std::string& filePath) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            LOGE("Failed to open audio file: %s", filePath.c_str());
            return false;
        }
        
        // Skip WAV header (44 bytes)
        file.seekg(44);
        
        // Read audio data
        audioData.clear();
        short sample;
        while (file.read(reinterpret_cast<char*>(&sample), sizeof(short))) {
            audioData.push_back(sample);
        }
        
        file.close();
        currentPosition = 0;
        
        LOGI("Loaded audio file: %s, samples: %zu", filePath.c_str(), audioData.size());
        return !audioData.empty();
    }
    
    bool startPlayback() {
        if (isPlaying || audioData.empty()) {
            return false;
        }
        
        SLresult result;
        
        // Configure audio source
        SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
        SLDataFormat_PCM format_pcm = {SL_DATAFORMAT_PCM, CHANNELS, SL_SAMPLINGRATE_44_1,
                                       SL_PCMSAMPLEFORMAT_FIXED_16, SL_PCMSAMPLEFORMAT_FIXED_16,
                                       SL_SPEAKER_FRONT_CENTER, SL_BYTEORDER_LITTLEENDIAN};
        SLDataSource audioSrc = {&loc_bufq, &format_pcm};
        
        // Configure audio sink
        SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, nullptr};
        SLDataSink audioSnk = {&loc_outmix, nullptr};
        
        // Create output mix
        SLObjectItf outputMixObject;
        result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, nullptr, nullptr);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to create output mix");
            return false;
        }
        
        result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to realize output mix");
            return false;
        }
        
        loc_outmix.outputMix = outputMixObject;
        
        // Create audio player
        const SLInterfaceID ids[1] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req[1] = {SL_BOOLEAN_TRUE};
        result = (*engineEngine)->CreateAudioPlayer(engineEngine, &playerObject, &audioSrc, &audioSnk, 1, ids, req);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to create audio player");
            return false;
        }
        
        // Realize the player
        result = (*playerObject)->Realize(playerObject, SL_BOOLEAN_FALSE);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to realize audio player");
            return false;
        }
        
        // Get the play interface
        result = (*playerObject)->GetInterface(playerObject, SL_IID_PLAY, &playerPlay);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to get play interface");
            return false;
        }
        
        // Get the buffer queue interface
        result = (*playerObject)->GetInterface(playerObject, SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &playerBufferQueue);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to get player buffer queue interface");
            return false;
        }
        
        // Register callback
        result = (*playerBufferQueue)->RegisterCallback(playerBufferQueue, bqPlayerCallback, this);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to register player callback");
            return false;
        }
        
        // Start playback
        result = (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_PLAYING);
        if (SL_RESULT_SUCCESS != result) {
            LOGE("Failed to start playback");
            return false;
        }
        
        isPlaying = true;
        currentPosition = 0;
        
        // Enqueue first buffer
        enqueueBuffer();
        
        LOGI("Playback started");
        return true;
    }
    
    bool stopPlayback() {
        if (!isPlaying) return false;
        
        isPlaying = false;
        
        if (playerPlay) {
            (*playerPlay)->SetPlayState(playerPlay, SL_PLAYSTATE_STOPPED);
        }
        
        if (playerObject) {
            (*playerObject)->Destroy(playerObject);
            playerObject = nullptr;
            playerPlay = nullptr;
            playerBufferQueue = nullptr;
        }
        
        LOGI("Playback stopped");
        return true;
    }
    
    bool isCurrentlyPlaying() const {
        return isPlaying;
    }
    
private:
    static void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
        AudioPlayer* player = static_cast<AudioPlayer*>(context);
        player->enqueueBuffer();
    }
    
    void enqueueBuffer() {
        if (!isPlaying || currentPosition >= audioData.size()) {
            isPlaying = false;
            return;
        }
        
        std::lock_guard<std::mutex> lock(playerMutex);
        
        size_t remainingSamples = audioData.size() - currentPosition;
        size_t samplesToPlay = std::min(static_cast<size_t>(BUFFER_SIZE), remainingSamples);
        
        if (samplesToPlay > 0 && playerBufferQueue) {
            (*playerBufferQueue)->Enqueue(playerBufferQueue, 
                                          &audioData[currentPosition], 
                                          samplesToPlay * sizeof(short));
            currentPosition += samplesToPlay;
        }
        
        if (currentPosition >= audioData.size()) {
            isPlaying = false;
        }
    }
    
    void cleanup() {
        if (isPlaying) {
            stopPlayback();
        }
        
        if (playerObject) {
            (*playerObject)->Destroy(playerObject);
            playerObject = nullptr;
        }
        
        if (engineObject) {
            (*engineObject)->Destroy(engineObject);
            engineObject = nullptr;
        }
    }
};

// Global instances
static AudioRecorder* g_recorder = nullptr;
static AudioPlayer* g_player = nullptr;

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_example_audiorecordingapp_AudioRecorderNative_initializeRecorder(JNIEnv *env, jobject thiz) {
    if (g_recorder == nullptr) {
        g_recorder = new AudioRecorder();
    }
    return g_recorder->initialize();
}

JNIEXPORT jboolean JNICALL
Java_com_example_audiorecordingapp_AudioRecorderNative_startRecording(JNIEnv *env, jobject thiz, jstring filePath) {
    if (g_recorder == nullptr) {
        LOGE("Recorder not initialized");
        return false;
    }
    
    const char* path = env->GetStringUTFChars(filePath, nullptr);
    bool result = g_recorder->startRecording(std::string(path));
    env->ReleaseStringUTFChars(filePath, path);
    
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_example_audiorecordingapp_AudioRecorderNative_stopRecording(JNIEnv *env, jobject thiz) {
    if (g_recorder == nullptr) {
        LOGE("Recorder not initialized");
        return false;
    }
    
    return g_recorder->stopRecording();
}

JNIEXPORT jboolean JNICALL
Java_com_example_audiorecordingapp_AudioRecorderNative_isRecording(JNIEnv *env, jobject thiz) {
    if (g_recorder == nullptr) {
        return false;
    }
    
    return g_recorder->isCurrentlyRecording();
}

JNIEXPORT jboolean JNICALL
Java_com_example_audiorecordingapp_AudioRecorderNative_initializePlayer(JNIEnv *env, jobject thiz) {
    if (g_player == nullptr) {
        g_player = new AudioPlayer();
    }
    return g_player->initialize();
}

JNIEXPORT jboolean JNICALL
Java_com_example_audiorecordingapp_AudioRecorderNative_loadAudioFile(JNIEnv *env, jobject thiz, jstring filePath) {
    if (g_player == nullptr) {
        LOGE("Player not initialized");
        return false;
    }
    
    const char* path = env->GetStringUTFChars(filePath, nullptr);
    bool result = g_player->loadAudioFile(std::string(path));
    env->ReleaseStringUTFChars(filePath, path);
    
    return result;
}

JNIEXPORT jboolean JNICALL
Java_com_example_audiorecordingapp_AudioRecorderNative_startPlayback(JNIEnv *env, jobject thiz) {
    if (g_player == nullptr) {
        LOGE("Player not initialized");
        return false;
    }
    
    return g_player->startPlayback();
}

JNIEXPORT jboolean JNICALL
Java_com_example_audiorecordingapp_AudioRecorderNative_stopPlayback(JNIEnv *env, jobject thiz) {
    if (g_player == nullptr) {
        LOGE("Player not initialized");
        return false;
    }
    
    return g_player->stopPlayback();
}

JNIEXPORT jboolean JNICALL
Java_com_example_audiorecordingapp_AudioRecorderNative_isPlaying(JNIEnv *env, jobject thiz) {
    if (g_player == nullptr) {
        return false;
    }
    
    return g_player->isCurrentlyPlaying();
}

JNIEXPORT void JNICALL
Java_com_example_audiorecordingapp_AudioRecorderNative_cleanup(JNIEnv *env, jobject thiz) {
    if (g_recorder != nullptr) {
        delete g_recorder;
        g_recorder = nullptr;
    }
    
    if (g_player != nullptr) {
        delete g_player;
        g_player = nullptr;
    }
}

}
