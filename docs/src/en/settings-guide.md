# Settings Guide

Each congregation profile in KMedia keeps its own isolated settings database and media folder.

## Congregation Profiles

- Create separate profiles for different congregations or language groups sharing the same computer.
- Configure each profile's media storage directory, preferred catalog language, and weekly midweek/weekend meeting schedule.

## Display & Standby

- **Presentation Display:** Choose automatic secondary-display assignment or select a specific output screen manually (override falls back automatically if the selected display disconnects).
- **Hardware Video Decoding:** Uses safe GPU decoding (`auto-safe`) via `libmpv` to keep laptop thermals and CPU usage minimal.
- **Standby Image:** Configure a custom congregation standby image shown when playback is idle or in standby (`Standby / Blackout`).

## Audio & Background Music

- **Output Device:** Select a dedicated audio device from `libmpv` to avoid feedback loops between hall speakers and Zoom.
- **Scheduled Background Music:** Set midweek and weekend meeting days and start times plus fade-out duration (default `5000 ms`) so ambient background music automatically performs a smooth fade-out and stops on schedule.

## OBS Studio & Zoom

- **OBS WebSocket v5:** Set host, port (default `4455`), and optional password, then map playback start, pause, stop, and standby events to specific OBS scenes.
- **Zoom Bridge:** Save the congregation Zoom meeting ID and passcode for one-click launch via `zoommtg://`, and optionally toggle screen-share automation when playback starts.
