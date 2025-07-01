package com.example.audiorecordingapp

class AudioRecorderNative {
    
    companion object {
        init {
            System.loadLibrary("audiorecordingapp")
        }
    }
    
    // Recording functions
    external fun initializeRecorder(): Boolean
    external fun startRecording(filePath: String): Boolean
    external fun stopRecording(): Boolean
    external fun isRecording(): Boolean
    
    // Playback functions
    external fun initializePlayer(): Boolean
    external fun loadAudioFile(filePath: String): Boolean
    external fun startPlayback(): Boolean
    external fun stopPlayback(): Boolean
    external fun isPlaying(): Boolean
    
    external fun cleanup()
}
