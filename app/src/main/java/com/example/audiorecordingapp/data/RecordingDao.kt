package com.example.audiorecordingapp.data

import androidx.room.*
import kotlinx.coroutines.flow.Flow

@Dao
interface RecordingDao {
    
    @Query("SELECT * FROM recordings ORDER BY dateCreated DESC")
    fun getAllRecordings(): Flow<List<Recording>>
    
    @Query("SELECT * FROM recordings WHERE id = :id")
    suspend fun getRecordingById(id: Long): Recording?
    
    @Insert
    suspend fun insertRecording(recording: Recording): Long
    
    @Update
    suspend fun updateRecording(recording: Recording)
    
    @Delete
    suspend fun deleteRecording(recording: Recording)
    
    @Query("DELETE FROM recordings WHERE id = :id")
    suspend fun deleteRecordingById(id: Long)
    
    @Query("SELECT COUNT(*) FROM recordings")
    suspend fun getRecordingCount(): Int
}
