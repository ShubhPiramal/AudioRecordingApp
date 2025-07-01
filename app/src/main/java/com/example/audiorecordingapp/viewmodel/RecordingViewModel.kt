package com.example.audiorecordingapp.viewmodel

import android.app.Application
import android.content.Context
import android.media.MediaMetadataRetriever
import android.util.Log
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.example.audiorecordingapp.AudioRecorderNative
import com.example.audiorecordingapp.data.Recording
import com.example.audiorecordingapp.data.RecordingDatabase
import com.example.audiorecordingapp.repository.RecordingRepository
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import java.io.File
import java.text.SimpleDateFormat
import java.util.*

class RecordingViewModel(application: Application) : AndroidViewModel(application) {
    
    private val repository: RecordingRepository
    private val audioRecorder = AudioRecorderNative()
    
    private val _isRecording = MutableStateFlow(false)
    val isRecording: StateFlow<Boolean> = _isRecording.asStateFlow()
    
    private val _recordings = MutableStateFlow<List<Recording>>(emptyList())
    val recordings: StateFlow<List<Recording>> = _recordings.asStateFlow()
    
    private val _isPlaying = MutableStateFlow(false)
    val isPlaying: StateFlow<Boolean> = _isPlaying.asStateFlow()
    
    private val _currentPlayingId = MutableStateFlow<Long?>(null)
    val currentPlayingId: StateFlow<Long?> = _currentPlayingId.asStateFlow()
    
    private val _recordingTime = MutableStateFlow(0L)
    val recordingTime: StateFlow<Long> = _recordingTime.asStateFlow()
    
    private var recordingStartTime = 0L
    private var currentRecordingPath: String? = null
    
    init {
        val database = RecordingDatabase.getDatabase(application)
        repository = RecordingRepository(database.recordingDao())
        
        // Initialize native recorder and player
        audioRecorder.initializeRecorder()
        audioRecorder.initializePlayer()
        
        // Load existing recordings
        loadRecordings()
    }
    
    private fun loadRecordings() {
        viewModelScope.launch {
            repository.getAllRecordings().collect { recordingList ->
                _recordings.value = recordingList
            }
        }
    }
    
    fun startRecording() {
        if (_isRecording.value) return
        
        try {
            val timestamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault()).format(Date())
            val fileName = "recording_$timestamp.wav"
            val recordingsDir = File(getApplication<Application>().filesDir, "recordings")
            if (!recordingsDir.exists()) {
                recordingsDir.mkdirs()
            }
            
            val filePath = File(recordingsDir, fileName).absolutePath
            currentRecordingPath = filePath
            
            if (audioRecorder.startRecording(filePath)) {
                _isRecording.value = true
                recordingStartTime = System.currentTimeMillis()
                startRecordingTimer()
                Log.d("RecordingViewModel", "Recording started: $filePath")
            } else {
                Log.e("RecordingViewModel", "Failed to start recording")
            }
        } catch (e: Exception) {
            Log.e("RecordingViewModel", "Error starting recording", e)
        }
    }
    
    fun stopRecording() {
        if (!_isRecording.value) return
        
        try {
            if (audioRecorder.stopRecording()) {
                _isRecording.value = false
                _recordingTime.value = 0L
                
                currentRecordingPath?.let { path ->
                    saveRecordingToDatabase(path)
                }
                
                Log.d("RecordingViewModel", "Recording stopped")
            } else {
                Log.e("RecordingViewModel", "Failed to stop recording")
            }
        } catch (e: Exception) {
            Log.e("RecordingViewModel", "Error stopping recording", e)
        }
    }
    
    private fun startRecordingTimer() {
        viewModelScope.launch {
            while (_isRecording.value) {
                _recordingTime.value = System.currentTimeMillis() - recordingStartTime
                kotlinx.coroutines.delay(100)
            }
        }
    }
    
    private fun saveRecordingToDatabase(filePath: String) {
        viewModelScope.launch {
            try {
                val file = File(filePath)
                if (file.exists()) {
                    val duration = getAudioDuration(filePath)
                    val fileName = file.nameWithoutExtension
                    
                    val recording = Recording(
                        name = fileName,
                        filePath = filePath,
                        duration = duration,
                        dateCreated = Date(),
                        fileSize = file.length()
                    )
                    
                    repository.insertRecording(recording)
                    Log.d("RecordingViewModel", "Recording saved to database: $fileName")
                } else {
                    Log.e("RecordingViewModel", "Recording file not found: $filePath")
                }
            } catch (e: Exception) {
                Log.e("RecordingViewModel", "Error saving recording to database", e)
            }
        }
    }
    
    private fun getAudioDuration(filePath: String): Long {
        return try {
            val retriever = MediaMetadataRetriever()
            retriever.setDataSource(filePath)
            val duration = retriever.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION)
            retriever.release()
            duration?.toLongOrNull() ?: 0L
        } catch (e: Exception) {
            Log.e("RecordingViewModel", "Error getting audio duration", e)
            0L
        }
    }
    
    fun deleteRecording(recording: Recording) {
        viewModelScope.launch {
            try {
                // Delete file from storage
                val file = File(recording.filePath)
                if (file.exists()) {
                    file.delete()
                }
                
                // Delete from database
                repository.deleteRecording(recording)
                Log.d("RecordingViewModel", "Recording deleted: ${recording.name}")
            } catch (e: Exception) {
                Log.e("RecordingViewModel", "Error deleting recording", e)
            }
        }
    }
    
    fun formatDuration(duration: Long): String {
        val seconds = (duration / 1000) % 60
        val minutes = (duration / (1000 * 60)) % 60
        val hours = (duration / (1000 * 60 * 60)) % 24
        
        return if (hours > 0) {
            String.format("%02d:%02d:%02d", hours, minutes, seconds)
        } else {
            String.format("%02d:%02d", minutes, seconds)
        }
    }
    
    fun playRecording(recording: Recording) {
        viewModelScope.launch {
            try {
                // Stop current playback if any
                if (_isPlaying.value) {
                    stopPlayback()
                }
                
                if (audioRecorder.loadAudioFile(recording.filePath)) {
                    if (audioRecorder.startPlayback()) {
                        _isPlaying.value = true
                        _currentPlayingId.value = recording.id
                        Log.d("RecordingViewModel", "Started playing: ${recording.name}")
                        
                        // Monitor playback status
                        monitorPlayback()
                    } else {
                        Log.e("RecordingViewModel", "Failed to start playback")
                    }
                } else {
                    Log.e("RecordingViewModel", "Failed to load audio file: ${recording.filePath}")
                }
            } catch (e: Exception) {
                Log.e("RecordingViewModel", "Error playing recording", e)
            }
        }
    }
    
    fun stopPlayback() {
        try {
            if (audioRecorder.stopPlayback()) {
                _isPlaying.value = false
                _currentPlayingId.value = null
                Log.d("RecordingViewModel", "Playback stopped")
            }
        } catch (e: Exception) {
            Log.e("RecordingViewModel", "Error stopping playback", e)
        }
    }
    
    private fun monitorPlayback() {
        viewModelScope.launch {
            while (_isPlaying.value) {
                if (!audioRecorder.isPlaying()) {
                    _isPlaying.value = false
                    _currentPlayingId.value = null
                    Log.d("RecordingViewModel", "Playback completed")
                    break
                }
                kotlinx.coroutines.delay(100)
            }
        }
    }
    
    fun formatFileSize(bytes: Long): String {
        val kb = bytes / 1024.0
        val mb = kb / 1024.0
        
        return when {
            mb >= 1 -> String.format("%.1f MB", mb)
            kb >= 1 -> String.format("%.1f KB", kb)
            else -> "$bytes B"
        }
    }
    
    override fun onCleared() {
        super.onCleared()
        audioRecorder.cleanup()
    }
}
