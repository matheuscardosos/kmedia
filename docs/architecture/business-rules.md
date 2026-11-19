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
- Midweek (Monday-Friday) and weekend (Saturday-Sunday) congregation meeting weekdays and start times (`meeting/midweek_day`, `meeting/midweek_time`, `meeting/weekend_day`, `meeting/weekend_time`) are modeled in `MeetingCalendarModel`, persisted per profile, and bound automatically into `BackgroundMusicEngine::configureSchedules(...)` so ambient music initiates a smooth fade-out (default `5000 ms`) tuned to conclude at the scheduled start timestamp.
- Operator pause/fade overrides take immediate precedence over automated scheduling for the remainder of the current session.

## 6. Network Politeness and Archive Safety

- All catalog and meeting media requests identify `KMedia/<version>` in `User-Agent`, enable HTTP/2, limit concurrent transfers to at most `3` by default (never above `6`), honor `ETag` (`If-None-Match`) and `Last-Modified` (`If-Modified-Since`) with disk-cached catalog JSON payloads so HTTP `304` responses preserve full language and publication metadata across sessions, stream payloads to `.part` files before atomic rename, verify SHA-256 checksums, and use exponential backoff only on transient statuses (`408`, `429`, `5xx`).
- Weekly meeting auto-sync (`MeetingCatalogService`) resolves exact workbook weeks from `.jwpub` `DatedText` records, probes candidate *Watchtower* issues across `6`, `8`, `10`, and `12` weeks back (`Class = 40` study articles), reads `DocumentParagraph`, `Question.TargetParagraphNumberLabel`, `DocumentExtract`, `Extract`, `RefPublication`, `CategoryType`, `SuppressZoom`, and `LinkMultimediaId`, resolves external video/audio assets via JW Mediator `v1/media-items` with `GETPUBMEDIALINKS` and secondary language fallback, treats media exceeding `2100 s` (`35` minutes) as stream-only (`streamUrl`), supports congregation schedule/language import via `hub.jw.org`, and honors per-week special event rules: shifting Circuit Overseer (`co_visit`) midweek meetings to Tuesday (auto-hiding CBS extracts and closing songs), suppressing regular meetings on Memorial (`memorial`) weeks while supporting `mi<YY>` + Songs `18` and `25`, skipping regular weekly sync during assemblies/conventions, and applying temporary weekly schedule overrides (`meeting/temp_schedule_changes_json`).
- Date-keyed workspaces (`DatabaseManager` Schema V2 `playlists` + `playlist_items`) isolate each congregation profile's daily playlists (`midweek`, `weekend`, `memorial`, `custom`), storing extract groups (`group_key`), colored visual dividers (`item_kind`, `divider_color`), hidden state (`hidden`), editable item tags (`tag_type`, `tag_value`), slide intervals (`slide_interval_ms`), paired background audio (`paired_audio_path`), playback rates (`playback_rate`), and restorable sync sort order (`original_section`, `sort_order_original`).
- Archive extractors (`.jwpub`, `.jwlplaylist`, `S-34 / S-34mp`) must validate every archive path and entry size cap against directory traversal (Zip-Slip) and decompression bombs before reading or writing data, and convert JW Library 100-nanosecond ticks into non-destructive millisecond trim bounds (`10000 ticks/ms`).

## 7. Cloud Folder Synchronization Rules

- Local Dropbox, OneDrive, Google Drive, and Nextcloud directories are watched recursively through OS filesystem notifications (`QFileSystemWatcher`) with zero disk-scanning timers in steady state.
- Filesystem modification notifications are debounced (`500 ms`) per file and checked for size/timestamp stability and readable archive integrity before importing so partially synced cloud files are never read prematurely.

## 8. Responsive Operator Console Breakpoints

- The operator main window (`Main.qml`) must support resizing down to a compact side-by-side layout (`minimumWidth: 360`, `minimumHeight: 520`) so operators can run KMedia side by side with Zoom cleanly on `1280x720` and `1366x768` booth displays:
  - **Wide (`width >= 1080 px`):** Three-column desktop workstation (`234 px` left navigation rail, flexible center workspace, `344 px` right live preview and inspector card stack).
  - **Medium (`680 px <= width < 1080 px`):** Narrow icon-only navigation rail (`64 px`), flexible center workspace, and collapsible inspector drawer for 50/50 split screens.
  - **Compact (`width < 680 px`, down to `360x520`):** Single-column stacked workspace with a `54 px` compact bottom tab switcher, icon-only floating pill action strip (`ActionPillBar`), persistent transport + `Standby/Blackout` strip, and toggleable preview drawer.

## 9. Hybrid Booth Integrations (OBS Studio and Zoom)

- `ProcessMonitor` queries running processes (`OBS Studio` and `Zoom`) on a gentle `3 s` interval using lightweight native OS APIs without spawning child shell processes.
- `ObsWebSocketClient` connects asynchronously via `Qt6::WebSockets` (`OpCode 0 Hello` -> SHA-256 challenge auth -> `OpCode 1 Identify` -> `OpCode 2 Identified`) only when OBS is detected or explicitly requested. Presenting static image or PDF media maps to the active play scene rather than the paused scene while Standby remains inactive.
- `ZoomBridge` launches meetings via `zoommtg://` and configures dedicated `libmpv` audio output selection across both presentation playback and ambient background music engines so Kingdom Hall speakers never echo back into Zoom.
- Direct workspace keyboard shortcuts (`Space`, `B`, `Esc`, `Delete`, `Ctrl+M`, `1`-`6`) automatically yield when any text input field or modal palette has focus, and demo-mode configuration edits stay isolated from persisted congregation settings.

## 10. UI Design System, Theme Persistence, and Meeting Section Grouping

- All QML controls inherit semantic tokens from `Theme.qml` supporting both **Studio Carbon (Dark)** and **Studio Daylight (Light)** modes persisted per profile via `ui/dark_theme` (and overridable at launch with `--theme dark|light`).
- Meeting playlist entries (`PlaylistModel`) group visually under workbook section headers (*Treasures From God's Word*, *Apply Yourself to the Field Ministry*, *Living as Christians*, *Public Talk*, *Watchtower Study*, *Circuit Overseer*, or custom user-created sections) via `ListView.section` while persisting custom section names, hex colors, and section ordering (`playlist/sections_<id>`) in SQLite and updating section assignment cleanly when dragging items across section boundaries or assigning via context menu.
- Primary library views (`MediaLibraryView`, `CalendarView`) prioritize visual 16:9 thumbnail grids, type filter pills, local cache readiness badges, and icon-first controls over multi-line descriptive prose.
- Settings use a searchable two-pane layout (`SettingsView`) with optional per-profile salted SHA-256 Booth PIN protection (`BoothSecurity`, `security/booth_pin_hash`, `security/booth_pin_salt`, gated by `SettingsPinGate`) and startup tab selection (`--tab`).
- Section dividers map custom clean-room monochrome vector glyphs in operator mode while falling back to generic Lucide icons in `--demo` mode, alongside unobtrusive pre/post-meeting readiness checklists (`BoothChecklistPopup`).
- Quick booth actions stay one click away through the enlarged floating `ActionPillBar` (with anchored popovers for display selection, background music playback/fade, live `MeetingTimer` schedule variance tracking, OBS scene/recording control, Zoom booth status, subtitle visibility toggle, and download queue adjustments) and `Ctrl+K` `CommandPalette` using bundled `Inter` typography and `MultiEffect`-tinted vector SVGs.
