package com.example.audiorecordingapp.repository

import com.example.audiorecordingapp.data.Recording
import com.example.audiorecordingapp.data.RecordingDao
import kotlinx.coroutines.flow.Flow

class RecordingRepository(private val recordingDao: RecordingDao) {
    
    fun getAllRecordings(): Flow<List<Recording>> = recordingDao.getAllRecordings()
    
    suspend fun getRecordingById(id: Long): Recording? = recordingDao.getRecordingById(id)
    
    suspend fun insertRecording(recording: Recording): Long = recordingDao.insertRecording(recording)
    
    suspend fun updateRecording(recording: Recording) = recordingDao.updateRecording(recording)
    
    suspend fun deleteRecording(recording: Recording) = recordingDao.deleteRecording(recording)
    
    suspend fun deleteRecordingById(id: Long) = recordingDao.deleteRecordingById(id)
    
    suspend fun getRecordingCount(): Int = recordingDao.getRecordingCount()
}
