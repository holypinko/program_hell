# Mehrkanaltonspektrumsreaktionsvisualisierungskomponente

A real-time OpenGL + WASAPI/Media Foundation demo that visualizes audio-reactive spirals with morphing geometry. (for a project)

## Features
- Decodes and plays back audio (MP3 etc) via Media Foundation
- Real-time WASAPI float PCM playback with fade-out
- Instanced spiral + helix geometry - Audio-reactive, bobbing and transparency
- **Cinematic camera system** with:
  - Wide orbital sweeps
  - Close-up dolly shots
  - Dramatic top-down sweeps
- Logging to `dev.log` and `error.log`

## How to use it (For Dummies)
1. On launch, a file dialog lets you select an audio file.
2. The visualization reacts in real-time to the audio envelope.
3. Press `ESC` to exit.

## Cinematic Camera
The camera cycles around every 60 seconds:
- **0–20s**: Wide orbit around the scene
- **20–40s**: Close-up
- **40–60s**: Sweeping shot  
Then the sequence repeats.
