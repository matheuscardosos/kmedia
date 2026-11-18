# KMedia Domain and Business Rules

This document defines the behavioral contracts and architectural invariants that apply across all modules in **KMedia**. Every C++ core service, media pipeline, and QML screen must respect these rules.

## 1. Congregation Profile Isolation and Switching

- Each congregation or language group profile owns an isolated SQLite3 database (`profile.db`), a dedicated media directory (`storagePath`), independent meeting schedules, and independent OBS/Zoom configuration.
- Registry metadata (`profiles.json`) must always be written atomically using `QSaveFile`.
- Deleting the last remaining congregation profile is rejected so the workspace always retains a valid active profile.
- Switching between profiles closes the active database cleanly, binds the target database and `SettingsStore`, and emits workspace reload signals in under `100 ms`.

## 2. Database and Disk Persistence Invariants

- SQLite3 opens in `WAL` mode (`synchronous = NORMAL`, `foreign_keys = ON`, `busy_timeout = 5000`, `mmap_size = 64 MB`) with versioned migrations tracked in `schema_migrations`.
- Multi-step playlist or catalog updates must execute inside explicit transactions with automatic rollback on failure.
- Empty optional text fields bind as empty strings rather than SQL `NULL` when stored in `TEXT NOT NULL` columns.
- Thumbnails are downscaled asynchronously to a bounded width (default `320 px`) on a capped worker pool (`<= 2` threads) and evicted strictly by LRU byte budget.

## 3. Dual-Monitor Presentation and Standby Rules

- Display monitoring reacts exclusively to `QGuiApplication::screens()` events (`screenAdded` / `screenRemoved`), with zero timer polling.
- When a secondary monitor is detected, the presentation output targets that display in borderless fullscreen unless a manual screen override is active.
- The external output window carries a consistent title (`"KMedia - Presentation"`) and composited backing so OBS Window Capture and Zoom Window Share capture cleanly without black frames.
- **Standby / Blackout Priority:** Activating Standby or Blackout takes immediate visual precedence over video, audio artwork, images, and PDF output without tearing down the active playback queue.

## 4. Non-Destructive Trim and Viewport Synchronization

- Media trimming uses `trim_start_ms` and `trim_end_ms` metadata passed directly to `libmpv` (`start` and `end` options) without re-encoding files on disk.
- If `trim_end_ms > 0` and `trim_end_ms <= trim_start_ms`, the invalid trim bound is ignored cleanly.
- Image zoom, normalized pan coordinates (`pan_x`, `pan_y`), and PDF page numbers synchronize in real time between the operator preview and the secondary presentation window.

## 5. Meeting Schedule and Background Music Fade-Out

- Ambient background music runs on a dedicated audio-only `libmpv` pipeline isolated from presentation video output.
- Before a scheduled midweek or weekend meeting start time, background music automatically initiates a smooth fade-out (default `5000 ms`) tuned to conclude at the scheduled start timestamp.
- Operator pause/fade overrides take immediate precedence over automated scheduling for the remainder of the current session.

## 6. Network Politeness and Archive Safety

- All catalog and meeting media requests identify `KMedia/<version>` in `User-Agent`, enable HTTP/2, limit concurrent transfers to at most `3` by default (never above `6`), honor `ETag` (`If-None-Match`) and `Last-Modified` (`If-Modified-Since`), and use exponential backoff only on transient statuses (`408`, `429`, `5xx`).
- Archive extractors (`.jwpub`, `.jwlplaylist`, `S-34`) must validate every archive path against directory traversal (Zip-Slip) before reading or writing data.

## 7. Cloud Folder Synchronization Rules

- Local Dropbox, OneDrive, Google Drive, and Nextcloud directories are watched through OS filesystem notifications (`QFileSystemWatcher`) with zero disk-scanning timers in steady state.
- Filesystem modification notifications are debounced (`500 ms`) before importing so partially synced cloud files are never read prematurely.

## 8. Responsive Operator Console Breakpoints

- The operator main window (`Main.qml`) must support resizing down to a compact side-by-side layout (`minimumWidth: 360`, `minimumHeight: 520`) so operators can run KMedia side by side with Zoom cleanly on `1280x720` and `1366x768` booth displays:
  - **Wide (`width >= 1080 px`):** Three-column desktop workstation (`210 px` left navigation rail, flexible center workspace, `340 px` right live preview and inspector).
  - **Medium (`680 px <= width < 1080 px`):** Narrow icon-only navigation rail (`56 px`), full center workspace, and collapsible inspector drawer/tab for 50/50 split screens.
  - **Compact (`width < 680 px`, down to `360 px`):** Mobile-style stacked workspace with compact bottom or top tab switcher, single-column playlist/library view, persistent quick transport + `Standby/Blackout` strip, and toggleable mini preview.

## 9. Hybrid Booth Integrations (OBS Studio and Zoom)

- `ProcessMonitor` queries running processes (`OBS Studio` and `Zoom`) on a gentle `3 s` interval using lightweight native OS APIs without spawning child shell processes.
- `ObsWebSocketClient` connects asynchronously via `Qt6::WebSockets` (`OpCode 0 Hello` -> SHA-256 challenge auth -> `OpCode 1 Identify` -> `OpCode 2 Identified`) only when OBS is detected or explicitly requested.
- `ZoomBridge` launches meetings via `zoommtg://` and configures dedicated `libmpv` audio output selection so Kingdom Hall speakers never echo back into Zoom.
