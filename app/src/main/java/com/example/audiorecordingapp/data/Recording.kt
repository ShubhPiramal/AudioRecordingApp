package com.example.audiorecordingapp.data

import androidx.room.Entity
import androidx.room.PrimaryKey
import java.util.Date

@Entity(tableName = "recordings")
data class Recording(
    @PrimaryKey(autoGenerate = true)
    val id: Long = 0,
    val name: String,
    val filePath: String,
    val duration: Long, // Duration in milliseconds
    val dateCreated: Date,
    val fileSize: Long // File size in bytes
)
