# Sound Game Project Progress

## Version 1.0 - Feature Complete Prototype

This document summarizes the development progress of the Sound Game project.
The project has successfully implemented a feature-rich prototype of a rhythm game.

### Core Gameplay Features:
- **6-Lane Rhythm Game:** The fundamental gameplay mechanic is a 6-lane rhythm game.
- **Note Spawning:** Notes scroll from the top to the bottom of the screen.
- **Judgment System:** Implemented a hit judgment system with "Perfect", "Great", and "Miss" ratings based on timing accuracy.
- **Scoring System:** Players receive score points based on their performance. A combo counter tracks consecutive successful hits.

### Music and Chart Features:
- **MIDI-based Chart Loading:** The game can read and parse `.mid` files to generate note charts dynamically.
- **BGM Synchronization:** Note scrolling is precisely synchronized with the BGM playback time.
- **External Song Management:** All song data, including titles, audio file paths, and chart file paths, is managed externally via a `songs.json` file, allowing for easy addition of new songs without recompiling.

### User Interface and Experience (UI/UX):
- **Complete Game Flow:** A full game cycle is implemented:
  1.  **Title Screen:** The game starts with a title screen.
  2.  **Song Selection:** Players can choose a song from a list loaded from `songs.json`.
  3.  **Difficulty Selection:** For each song, players can select a difficulty level (EASY, NORMAL, HARD).
  4.  **Gameplay Screen:** The main game screen where notes are played.
  5.  **Pause Menu:** Players can pause the game to access "Resume", "Retry", and "Back to Select" options.
  6.  **Results Screen:** After a song is completed, a results screen displays the final score, max combo, and a breakdown of Perfect/Great/Miss counts.
- **Visual Enhancements:** The game includes background images for both the gameplay and results screens, and uses multiple fonts to improve visual clarity.
- **Audio Feedback:** A tap sound effect is played upon hitting a note, enhancing the gameplay experience.

### Technical Details:
- **Language:** C++
- **Library:** SFML (Simple and Fast Multimedia Library) for graphics, audio, and windowing.
- **External Libraries:**
  - **Midifile:** For parsing MIDI chart data.
  - **nlohmann/json:** For parsing the `songs.json` configuration file.
- **Development Environment:** The project was developed in a hybrid environment, with code editing on WSL and compilation/execution on Windows (using MinGW-w64) to ensure native performance for audio and graphics.