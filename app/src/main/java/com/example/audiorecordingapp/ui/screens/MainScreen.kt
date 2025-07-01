package com.example.audiorecordingapp.ui.screens

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Mic
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.example.audiorecordingapp.data.Recording
import com.example.audiorecordingapp.ui.components.RecordingItem
import com.example.audiorecordingapp.viewmodel.RecordingViewModel
import com.google.accompanist.permissions.ExperimentalPermissionsApi
import com.google.accompanist.permissions.MultiplePermissionsState
import com.google.accompanist.permissions.isGranted

@OptIn(ExperimentalPermissionsApi::class)
@Composable
fun MainScreen(
    viewModel: RecordingViewModel,
    permissionsState: MultiplePermissionsState,
    modifier: Modifier = Modifier
) {
    val isRecording by viewModel.isRecording.collectAsState()
    val recordings by viewModel.recordings.collectAsState()
    val recordingTime by viewModel.recordingTime.collectAsState()
    val isPlaying by viewModel.isPlaying.collectAsState()
    val currentPlayingId by viewModel.currentPlayingId.collectAsState()
    
    LaunchedEffect(Unit) {
        if (!permissionsState.allPermissionsGranted) {
            permissionsState.launchMultiplePermissionRequest()
        }
    }
    
    Column(
        modifier = modifier
            .fillMaxSize()
            .padding(16.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        // Title
        Text(
            text = "Audio Recording App",
            fontSize = 24.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(bottom = 32.dp)
        )
        
        if (!permissionsState.allPermissionsGranted) {
            PermissionRequestCard(
                onRequestPermissions = {
                    permissionsState.launchMultiplePermissionRequest()
                }
            )
        } else {
            // Recording Controls
            RecordingControls(
                isRecording = isRecording,
                recordingTime = recordingTime,
                onStartRecording = { viewModel.startRecording() },
                onStopRecording = { viewModel.stopRecording() },
                modifier = Modifier.padding(bottom = 32.dp)
            )
            
            // Recordings List
            RecordingsList(
                recordings = recordings,
                isPlaying = isPlaying,
                currentPlayingId = currentPlayingId,
                onPlayRecording = { recording -> viewModel.playRecording(recording) },
                onStopPlayback = { viewModel.stopPlayback() },
                onDeleteRecording = { recording -> viewModel.deleteRecording(recording) },
                formatDuration = { duration -> viewModel.formatDuration(duration) },
                formatFileSize = { size -> viewModel.formatFileSize(size) }
            )
        }
    }
}

@Composable
fun PermissionRequestCard(
    onRequestPermissions: () -> Unit
) {
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .padding(16.dp),
        elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text(
                text = "Permissions Required",
                fontSize = 18.sp,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(bottom = 8.dp)
            )
            Text(
                text = "This app needs microphone and storage permissions to record and save audio files.",
                textAlign = TextAlign.Center,
                modifier = Modifier.padding(bottom = 16.dp)
            )
            Button(
                onClick = onRequestPermissions,
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("Grant Permissions")
            }
        }
    }
}

@Composable
fun RecordingControls(
    isRecording: Boolean,
    recordingTime: Long,
    onStartRecording: () -> Unit,
    onStopRecording: () -> Unit,
    modifier: Modifier = Modifier
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        elevation = CardDefaults.cardElevation(defaultElevation = 4.dp)
    ) {
        Column(
            modifier = Modifier.padding(24.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            // Recording Time Display
            if (isRecording) {
                Text(
                    text = formatRecordingTime(recordingTime),
                    fontSize = 32.sp,
                    fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.primary,
                    modifier = Modifier.padding(bottom = 16.dp)
                )
            }
            
            // Record Button
            FloatingActionButton(
                onClick = {
                    if (isRecording) {
                        onStopRecording()
                    } else {
                        onStartRecording()
                    }
                },
                containerColor = if (isRecording) Color.Red else MaterialTheme.colorScheme.primary,
                modifier = Modifier.size(80.dp)
            ) {
                Icon(
                    imageVector = if (isRecording) Icons.Default.Stop else Icons.Default.Mic,
                    contentDescription = if (isRecording) "Stop Recording" else "Start Recording",
                    modifier = Modifier.size(40.dp),
                    tint = Color.White
                )
            }
            
            Text(
                text = if (isRecording) "Recording..." else "Tap to Record",
                fontSize = 16.sp,
                modifier = Modifier.padding(top = 16.dp)
            )
        }
    }
}

@Composable
fun RecordingsList(
    recordings: List<Recording>,
    isPlaying: Boolean,
    currentPlayingId: Long?,
    onPlayRecording: (Recording) -> Unit,
    onStopPlayback: () -> Unit,
    onDeleteRecording: (Recording) -> Unit,
    formatDuration: (Long) -> String,
    formatFileSize: (Long) -> String
) {
    Column {
        Text(
            text = "Recordings (${recordings.size})",
            fontSize = 20.sp,
            fontWeight = FontWeight.Bold,
            modifier = Modifier.padding(bottom = 16.dp)
        )
        
        if (recordings.isEmpty()) {
            Card(
                modifier = Modifier.fillMaxWidth(),
                elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
            ) {
                Text(
                    text = "No recordings yet.\nTap the microphone button to start recording.",
                    textAlign = TextAlign.Center,
                    modifier = Modifier.padding(32.dp),
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        } else {
            LazyColumn(
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                items(recordings) { recording ->
                    RecordingItem(
                        recording = recording,
                        isPlaying = isPlaying && currentPlayingId == recording.id,
                        onPlayClick = {
                            if (isPlaying && currentPlayingId == recording.id) {
                                onStopPlayback()
                            } else {
                                onPlayRecording(recording)
                            }
                        },
                        onDeleteClick = { onDeleteRecording(recording) },
                        formatDuration = formatDuration,
                        formatFileSize = formatFileSize
                    )
                }
            }
        }
    }
}

private fun formatRecordingTime(timeMs: Long): String {
    val seconds = (timeMs / 1000) % 60
    val minutes = (timeMs / (1000 * 60)) % 60
    return String.format("%02d:%02d", minutes, seconds)
}
