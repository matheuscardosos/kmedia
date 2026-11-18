# User Guide

This guide covers the core KMedia workflow for preparing and presenting meeting media.

## Three-Column Operator Console

1. **Left Navigation Rail:** Switch between active congregation profiles, navigate between *Meeting*, *Media*, *Playlists*, *Downloads*, and *Settings*, and monitor live status for OBS Studio, Zoom, secondary displays, and disk storage.
2. **Center Workspace:** Manage the active meeting playlist, reorder items via drag-and-drop, add local files or `.jwpub`/`.jwlplaylist` packages, and control playback from the bottom transport bar.
3. **Right Live Preview & Inspector:** Monitor the current presentation output, toggle instant *Standby / Blackout*, inspect media metadata, set non-destructive start/end trim timestamps, or adjust image/PDF zoom, pan, and page selection.

## Playback, Trim, Zoom, and PDF Presentation

- **Non-Destructive Video and Audio Trims:** Start (`trimStartMs`) and end (`trimEndMs`) markers are enforced directly by `libmpv` without modifying or re-encoding original files. Setting an end time earlier than or equal to the start time disables the upper bound cleanly.
- **Image and Video Viewport Controls:** Adjust zoom (`1.0x` to `8.0x`) and pan coordinates (`-0.5` to `+0.5`) smoothly across video, image, and PDF presentation output, or reset the viewport with one click.
- **Multi-Page PDF Documents:** Navigate pages (`Previous`, `Next`, or direct page selection) rendered natively through `Qt6::Pdf` at display resolution.

## Background Music and Meeting Schedules

- Background ambient music runs on a dedicated audio-only `libmpv` pipeline independent from presentation video playback.
- Configure midweek and weekend meeting start times so background music initiates an automatic smooth fade-out (default `5000 ms`) scheduled to finish right when the meeting starts. Manual pause or play overrides remain active for that session.

## Importing Media

- **Drag and Drop:** Drop video (`MP4`, `MKV`, `WebM`), audio (`MP3`, `AAC`, `OGG`, `FLAC`), image (`JPEG`, `PNG`, `WebP`), or `PDF` files directly onto the playlist.
- **Publication Packages:** Import `.jwpub` and `.jwlplaylist` files directly without external unzipping tools.
- **Watched Cloud Folders:** Configure a local folder synced with Dropbox, OneDrive, Google Drive, or Nextcloud so newly added items appear automatically in the active meeting.

## Presenting on a Secondary Display

When a secondary monitor or projector is connected, KMedia automatically targets it for full-screen borderless presentation (`"KMedia - Presentation"`) while keeping the operator console on the primary screen. Use the *Standby / Blackout* control to switch smoothly between active media and a black screen or custom congregation standby image without losing playlist state.
