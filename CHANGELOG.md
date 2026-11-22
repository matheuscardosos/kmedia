# Changelog

All notable changes to KMedia will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Shared folder and media export, inspired by the Meeting Media Manager folder watcher and auto-export (`WatchedFolderMedia`, `MediaAutoExport`): media dropped into the `YYYY-MM-DD` folders of a shared folder shows up in that day, placed by `.section-order.json`, exported names, or the day's main section, with moves written back and folders prepared for the coming meetings; every day with media is exported into numbered date folders after each sync and each change, optionally converted to MP4.
- JW.org videos in the Add media menu (`JwVideoCatalog`), inspired by the Meeting Media Manager video picker: the latest videos first, then the whole video library in the media language, newest first and searchable by title; a picked video is downloaded at or below the preferred resolution with its preview picture.
- Publication media by document (`PublicationMediaPicker`), inspired by the Meeting Media Manager publication media dialog: open a JW.org publication by symbol and issue, or a `.jwpub` from the computer, then pick the document whose media you need; it is added in document order with paragraph tags, and linked videos download like synced media. A `.jwpub` picked or dropped no longer adds every picture of the whole publication.
- Public talk media from the S-34mp (`PublicTalkMedia`), inspired by the Meeting Media Manager picker: pick the congregation's `.jwpub` once, search the talks by number or title, and the picked talk's media lands in *Public Talk* in outline order with the talk title as the section subtitle; media the outline only links to is downloaded like synced media.
- Simplified the navigation to two pages, *Meeting* and *Settings*, with the same header on each (icon, title, congregation). The congregation lookup, meeting days and times, media languages, and media filters moved to *Settings > Meetings*; special weeks and temporary schedules are set from the *Meeting* page menu for the week of the open day; the download queue with all its actions lives in the pill bar popover; the preview panel can be put away on wide windows too; and *Settings* is only created when first opened. The calendar, media library, playlists, and downloads pages are removed.
- Rebuilt the *Meeting* page around the selected day, inspired by the Meeting Media Manager media calendar (`MeetingMediaPage`): a header with the meeting and congregation, previous and next day with media, a date picker whose dots show each day's sync status, and an **Add media** menu grouped by source; one card per section with its colored icon, translated name, and *Watchtower* article title; media rows with a 16:9 thumbnail, duration, song or paragraph tag, play button, and a menu for everything else; notices for empty sections and hidden media, placeholder rows while a day syncs, and an error banner with retry. Songs can be added by number (`MeetingSyncEngine::addSong`), including the *Public Talk* song from its section header, and the page opens on the first day from today on with a meeting or media. Menus now follow the dark and light themes (`StudioMenu`).
- Every media row now shows a real thumbnail, decoded once on a worker thread and cached: pictures and PDF pages from the file, synced videos from the preview picture JW.org publishes (database schema V4 keeps its path), and other videos and audio covers from a frame read by a headless mpv instance. A new `MeetingDayModel` lays out each day the way the meeting media page will show it (meeting sections in meeting order, notices for empty sections and hidden media) and applies changes as row inserts and moves, so the list keeps its place while a day syncs or reloads. The sync status of each day now also marks days that hold added media.
- Meeting media now syncs on its own (`MeetingSyncEngine`, `PublicationStore`): the meetings of the next five weeks are kept ready in the background, two days at a time and closest meeting first, each in its own date workspace with a per-day status. Publications are downloaded and unpacked once on a worker thread and reused for every week they cover, extracts that point at other publications bring their pictures too, and re-syncing a day keeps the operator's edits and added media.
- Added the publication layer for the per-day meeting sync: each `.jwpub` is extracted once and reused for every week it covers (`JwpubParser::extractContents`), `PublicationDatabase` reads the week's document by `DatedText`, its media with paragraph labels, the extracts that point at other publications, and the song positions, and `MeetingMediaBuilder` turns them into ordered day items with sections by paragraph position, Congregation Bible Study detection, paragraph and song tags, extract groups, and the Circuit Overseer week rules.
- Meeting sections are stored by stable id (`tgw`, `ayfm`, `lac`, `pt`, `wt`, `circuit-overseer`, `imported-media`, `custom-<n>`), so their names follow the interface language and their default colors match the meeting workbook. Database schema V3 migrates existing days in place and adds a per-day sync status plus synced and added media tracking for the upcoming background sync.
- Implemented date-keyed daily meeting workspaces (`DatabaseManager` V2 schema + `PlaylistModel`) isolating `midweek`, `weekend`, `memorial`, and custom non-meeting days per congregation profile with collapsible extract groups, colored dividers, editable media tags, hidden item toggles, per-section repeat, slide intervals, search filter (`Ctrl+F`), and sort-order reset.
- Added `YYYY-MM-DD` day folder support shared by the watched folder and the media auto-export (`DatedFolderLayout`, `PlaylistExporter`): the date comes from the parent folder, placements live in a per-folder `.section-order.json`, exports use numbered `NN Section - NN Title` names that stay in sync between runs, day folders are removed once their week is over, images and audio can become MP4 clips through FFmpeg, and `CloudSyncWatcher` gains an opt-in mode that only watches recent day folders. `PdfViewerController` can also export a page range as PNG images.
- Upgraded `JwpubParser`, `CatalogClient`, `MeetingCalendarModel`, and `MeetingCatalogService` with `DatedText` week matching, `[6, 8, 10, 12]`-week *Watchtower* `Class = 40` resolution, `DocumentExtract` song/reference parsing, `SuppressZoom` / `LinkMultimediaId` filtering, Mediator `v1/media-items` + `GETPUBMEDIALINKS` + secondary language fallback, `2100 s` stream-only threshold, CO Tuesday shift + hidden CBS/closing song rules, Memorial suppression + `mi<YY>` support, temporary schedule overrides, extraction filter toggles, and `hub.jw.org` Congregation Lookup UI.

### Changed

- Icons are drawn by an image provider that recolors each SVG once and lets Qt cache it, instead of a shader effect with its own layer per icon, so a busy meeting page uses far fewer GPU layers; icons now also show in software rendering and in `--screenshot` captures.

### Documentation

- Rewrote the user guide, settings guide, and FAQ in English and Portuguese around the day by day *Meeting* page: first launch, choosing a day and its status dots, how meetings sync, sections and item menus, every way to add media, presenting and keyboard shortcuts, the shared folder and media export, one section per *Settings* category, and new troubleshooting and privacy answers, with fresh screenshots. The README, home, and About pages no longer describe features that are not in the app, and they credit Meeting Media Manager as the inspiration for the meeting workflow.

### Fixed

- Media from the watched cloud folder no longer lands in whatever day is open: it now goes to the day of its date folder.
- Removed the old weekly sync that `MeetingSyncEngine` replaced: `MeetingCatalogService` now only keeps the congregation lookup, the language catalog, and the media filters, and a `.jwpub` added by hand no longer guesses meeting sections from English or Portuguese words in its titles.
- Sign language congregations now get their songs from the sign language songbook (`sjj`) instead of `sjjm`, in the meeting sync and when adding a song.
- Text fields and other customized controls now draw with the KMedia theme on Windows and macOS: the native styles ignored their custom backgrounds, so the app now builds on the Qt Quick Controls *Basic* style everywhere.
- Fixed second screen changes (resolution or position) going unnoticed because the screen geometry connection was rejected, and the focus ring of the pill bar buttons that never showed.
- Fixed the weekly sync putting media into whatever day was open when a download finished, duplicating every item on each re-sync, appending late media at the end instead of its paragraph position, never retrying incomplete days, downloading the audio recordings of up to four Watchtower issues, mixing study articles from several Watchtower issues, assigning midweek sections by English or Portuguese keywords, and leaving the weekend closing song visible during a Circuit Overseer week.
- Fixed JW.org downloads always failing checksum verification: GETPUBMEDIALINKS and the Mediator publish MD5 checksums, which `DownloadService` compared against a SHA-256 hash and then deleted the file. Downloads now verify with the algorithm the expected checksum belongs to (MD5 or SHA-256).
- Opening a day no longer reads every image from disk or queries each media item separately, and removing a custom section keeps its media under *Added Media* instead of refusing.
- Fixed OBS/Zoom actions in `SettingsView` (`connectToObs()` and `joinMeeting()`), kept static Image/PDF presentations mapped to the OBS play scene instead of `pauseScene`, routed `ZoomBridge::dedicatedAudioDevice` through `PlaybackController` and `BackgroundMusicEngine`, isolated `--demo` writes in `saveCloudFolder()` and `saveObsAndZoomSettings()`, and wired focus-safe local keyboard shortcuts (`Space`, `B`, `Ctrl+M`, `Escape`, `Delete`, `1`-`6`) in `Main.qml`.

## [0.1.2](https://github.com/matheuscardosos/kmedia/compare/v0.1.1...v0.1.2) (2026-11-19)


### Added

* **calendar,playlist:** add CO visit overrides and custom section builder ([#51](https://github.com/matheuscardosos/kmedia/issues/51)) ([d426631](https://github.com/matheuscardosos/kmedia/commit/d42663185c7105c16ebc90230dc800b201ba319f)), closes [#32](https://github.com/matheuscardosos/kmedia/issues/32) [#33](https://github.com/matheuscardosos/kmedia/issues/33)
* **network:** wire automatic weekly meeting sync and catalog download UI ([#49](https://github.com/matheuscardosos/kmedia/issues/49)) ([006569e](https://github.com/matheuscardosos/kmedia/commit/006569eed97db4102571a275407e7fb97eb61dd6)), closes [#46](https://github.com/matheuscardosos/kmedia/issues/46)
* **packaging:** add native Windows Inno Setup installer ([#44](https://github.com/matheuscardosos/kmedia/issues/44)) ([d017b14](https://github.com/matheuscardosos/kmedia/commit/d017b14c630e5f7f2f79575cb9e913dca7f78d52)), closes [#43](https://github.com/matheuscardosos/kmedia/issues/43)
* **settings:** add two-pane PIN lock, section glyphs, and checklists ([#52](https://github.com/matheuscardosos/kmedia/issues/52)) ([8ad5488](https://github.com/matheuscardosos/kmedia/commit/8ad548815bd9dae281a6e1845801c4f1d32cb724)), closes [#34](https://github.com/matheuscardosos/kmedia/issues/34) [#36](https://github.com/matheuscardosos/kmedia/issues/36)
* **ui:** add meeting calendar and visual media grid (Closes [#31](https://github.com/matheuscardosos/kmedia/issues/31), [#47](https://github.com/matheuscardosos/kmedia/issues/47)) ([#50](https://github.com/matheuscardosos/kmedia/issues/50)) ([2dc04f4](https://github.com/matheuscardosos/kmedia/commit/2dc04f4afd7cf7eec29d513a5d0e3596a6f08344))
* **ui:** upgrade ActionPillBar with brand icons, popovers, timer, and subtitles ([#48](https://github.com/matheuscardosos/kmedia/issues/48)) ([213709d](https://github.com/matheuscardosos/kmedia/commit/213709d67a29e81d0b2477899e926358e459c4c0)), closes [#35](https://github.com/matheuscardosos/kmedia/issues/35)
* Searchable two-pane settings console (`SettingsView`) across six categories (*General*, *Schedule*, *Display*, *Audio*, *Integrations*, *Security*) with per-profile salted SHA-256 Booth PIN protection (`BoothSecurity`, `SettingsPinGate`, `SettingsSecurityCard`) and CLI `--tab` screenshot/launch targeting.
* Clean-room JW meeting section vector glyphs (`section-treasures`, `section-ministry`, `section-living`, `section-public-talk`, `section-watchtower`, `section-overseer` in operator mode with generic fallback in `--demo`) and pre/post-meeting booth readiness checklists (`BoothChecklistPopup`).
* Circuit Overseer visit, Memorial, Circuit Assembly, and Regional Convention schedule overrides (`MeetingCalendarModel`, `MeetingCatalogService`, `CalendarEventPopup`) with per-profile JSON persistence, automatic *Circuit Overseer* section injection, and smart sync handling on special-event weeks.
* Custom meeting section builder and color customization (`PlaylistModel`, `DatabaseManager`, `CustomSectionPopup`, `MeetingPlaylistView`) supporting custom section creation, renaming, palette swatch coloring, section reordering, item-to-section assignment, and dragging across section boundaries with SQLite persistence.
* Monthly and 4-week meeting calendar navigation (`MeetingCalendarModel`, `CalendarView`, `CalendarSchedulePopup`) with configurable per-profile congregation midweek/weekend days and times, local catalog readiness indicators (`MWB` / `W`), direct week opening/syncing, visual 16:9 thumbnail grid & compact list modes with live search/type filters in `MediaLibraryView`, and streamlined icon-driven workspace cards.
* Automatic weekly meeting fetching and download orchestration (`MeetingCatalogService`, disk-cached `CatalogClient` JSON responses, QML-bound `DownloadService` transfer queue with progress/pause/resume/retry/cancel, searchable congregation media language selector, `CatalogBrowserCard`, `DownloadQueueCard`, and one-click `Sync Week` header action).
* Enlarged floating `ActionPillBar` with authentic OBS Studio and Zoom custom brand SVGs, status LED indicators, anchored quick popovers (Display selector, Background Music playback/volume/fade, live `MeetingTimer`, OBS scene switcher and recording toggle, Zoom launch/audio readout, and Downloads queue), and `libmpv` subtitle toggle.
* Native Windows Inno Setup installer (`kmedia-windows-x64-setup.exe`) with per-user non-admin default installation, bilingual EN/PT wizard, bundled MSVC CRT runtime DLLs, and aligned download documentation links.


### Fixed

* **ui:** center StudioButton icons and fit compact header actions ([#54](https://github.com/matheuscardosos/kmedia/issues/54)) ([341c1a3](https://github.com/matheuscardosos/kmedia/commit/341c1a358cb7a1b3c09eb8d0068ec4540f015286))
* Centered icon-only `StudioButton` glyphs reliably across toolbars/calendar cells and tightened compact header action bar spacing in `MeetingPlaylistView` so all actions fit comfortably down to `360 px` window widths.

## [0.1.1](https://github.com/matheuscardosos/kmedia/compare/v0.1.0...v0.1.1) (2026-11-19)


### Added

* **integrations:** add ProcessMonitor, OBS WebSocket v5 client, and ZoomBridge ([3c5792b](https://github.com/matheuscardosos/kmedia/commit/3c5792be393d89841b3ee15a1188bd7e790038ef)), closes [#18](https://github.com/matheuscardosos/kmedia/issues/18) [#19](https://github.com/matheuscardosos/kmedia/issues/19)
* **ui:** rebuild QML Studio workstation with section dividers, themes, and ActionPillBar ([10c219c](https://github.com/matheuscardosos/kmedia/commit/10c219c9bb5cc783c54472bd952c87788d112705)), closes [#20](https://github.com/matheuscardosos/kmedia/issues/20) [#21](https://github.com/matheuscardosos/kmedia/issues/21) [#22](https://github.com/matheuscardosos/kmedia/issues/22)
* Multi-resolution Windows (`icon.ico`, `kmedia.rc`), macOS (`kmedia.icns`), Linux/Qt (`icon.png`), vector (`logo.svg`), website favicon icon assets, and README product banner.
* Repository documentation (`CONTRIBUTING.md`, `CHANGELOG.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`) and user-focused `README.md`.
* Core persistence and profile engine (`DatabaseManager` with SQLite3 WAL migrations, `ProfileManager`, `SettingsStore`, `NetworkManager`, `SystemUtils`, and disk-backed LRU `ThumbnailCache`) with Catch2 unit tests.
* Repository automation (`.github/dependabot.yml`, `.github/labeler.yml`, PR Conventional Commit title and issue traceability check, `Docs Sync` documentation enforcement, and CodeQL scanning) plus domain business rules in `docs/architecture/business-rules.md`.
* Media engine and dual-screen presentation modules (`MpvRendererItem` transport/trims/tracks/zoom-pan, `PlaybackController`, `BackgroundMusicEngine` with scheduled 5-second fade-out, `DisplayManager` with `PresentationWindow`, and `PdfViewerController` with `PdfPageImageProvider`) plus Catch2 unit tests.
* Native content parsers and cloud sync pipeline (`ZipArchiveReader` with minizip-ng Zip-Slip guards, `JwpubParser`, `S34Parser`, `JwlPlaylistParser`, `CatalogClient`, `DownloadService` with ETag caching and SHA-256 verification, and event-driven `CloudSyncWatcher`) plus Catch2 integration tests.
* Native booth integrations (`ProcessMonitor`, `ObsWebSocketClient` for OBS WebSocket v5 SHA-256 auth and scene automation, and `ZoomBridge` `zoommtg://` launcher).
* Redesigned QML Studio design system (`resources/qml/controls/Studio*.qml`) with dual **Dark (`Studio Carbon`)** and **Light (`Studio Daylight`)** themes (`ui/dark_theme` + `--theme`), bundled `Inter` font family, `MultiEffect`-tinted Lucide vector icons, custom `.jwpub` / `.jwlplaylist` SVG badges, **Meeting Section Dividers** (*Treasures From God's Word*, *Apply Yourself to the Field Ministry*, *Living as Christians*), **Floating Pill Action Bar (`ActionPillBar`)**, **Command Palette (`Ctrl+K`)**, toast notifications, playlist drag-and-drop, non-destructive trim/zoom inspector, media/playlists/downloads/settings views, and responsive scaling down to `360x520`.
* Complete EN/PT Qt Linguist catalogs (`resources/translations/kmedia_en.ts`, `kmedia_pt.ts`), `--demo` / `--screenshot` automated screenshot workflow (`.github/workflows/screenshot-refresh.yml`), release automation (`release-please-config.json`, `.github/workflows/release-please.yml`, `.github/workflows/release.yml` with SHA-256 sums), workflow security auditing (`.github/workflows/zizmor.yml`), multi-IDE & multi-AI contributor configurations (`AGENTS.md`, `CLAUDE.md`, `.cursor/rules/`, `.github/copilot-instructions.md`, `.windsurfrules`, `.ai/system-prompt.md`, `.vscode/`, `.zed/`, `.editorconfig`), and `THIRD-PARTY-NOTICES.md`.


### Fixed

* **ci:** fix install-qt-action module list and pin zizmor-action v0.8.1 ([8625525](https://github.com/matheuscardosos/kmedia/commit/862552502c9f6a65a38adf41c50cfd1a2f7aa2cc)), closes [#20](https://github.com/matheuscardosos/kmedia/issues/20) [#21](https://github.com/matheuscardosos/kmedia/issues/21)

## [0.1.0] - 2026-11-18

### Added

- Clean-room C++20 / CMake 3.25 / Ninja / vcpkg project structure with `kmedia_core` library and `KMedia.UI` Qt 6 Quick module.
- OpenGL FBO `MpvRendererItem` with thread-safe `mpv_render_context` integration and safe hardware decoding (`hwdec=auto-safe`).
- Studio Carbon & Broadcast Orange three-column operator shell (`Theme.qml`, `Main.qml`, rail, workspace, and inspector components).
- Multi-OS GitHub Actions CI matrix (Ubuntu, Windows MSVC, macOS), `clang-format` check, `qmllint` verification, and Catch2 smoke test suite.
- Bilingual VitePress documentation site (English and Portuguese) deployed to GitHub Pages.
