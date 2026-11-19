# Changelog

All notable changes to KMedia will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.1](https://github.com/matheuscardosos/kmedia/compare/v0.1.0...v0.1.1) (2026-11-19)


### Added

* **integrations:** add ProcessMonitor, OBS WebSocket v5 client, and ZoomBridge ([3c5792b](https://github.com/matheuscardosos/kmedia/commit/3c5792be393d89841b3ee15a1188bd7e790038ef)), closes [#18](https://github.com/matheuscardosos/kmedia/issues/18) [#19](https://github.com/matheuscardosos/kmedia/issues/19)
* **ui:** rebuild QML Studio workstation with section dividers, themes, and ActionPillBar ([10c219c](https://github.com/matheuscardosos/kmedia/commit/10c219c9bb5cc783c54472bd952c87788d112705)), closes [#20](https://github.com/matheuscardosos/kmedia/issues/20) [#21](https://github.com/matheuscardosos/kmedia/issues/21) [#22](https://github.com/matheuscardosos/kmedia/issues/22)


### Fixed

* **ci:** fix install-qt-action module list and pin zizmor-action v0.8.1 ([8625525](https://github.com/matheuscardosos/kmedia/commit/862552502c9f6a65a38adf41c50cfd1a2f7aa2cc)), closes [#20](https://github.com/matheuscardosos/kmedia/issues/20) [#21](https://github.com/matheuscardosos/kmedia/issues/21)

## [Unreleased]

### Added

- Automatic weekly meeting fetching and download orchestration (`MeetingCatalogService`, disk-cached `CatalogClient` JSON responses, QML-bound `DownloadService` transfer queue with progress/pause/resume/retry/cancel, searchable congregation media language selector, `CatalogBrowserCard`, `DownloadQueueCard`, and one-click `Sync Week` header action).
- Enlarged floating `ActionPillBar` with authentic OBS Studio and Zoom custom brand SVGs, status LED indicators, anchored quick popovers (Display selector, Background Music playback/volume/fade, live `MeetingTimer`, OBS scene switcher and recording toggle, Zoom launch/audio readout, and Downloads queue), and `libmpv` subtitle toggle.
- Native Windows Inno Setup installer (`kmedia-windows-x64-setup.exe`) with per-user non-admin default installation, bilingual EN/PT wizard, bundled MSVC CRT runtime DLLs, and aligned download documentation links.

- Multi-resolution Windows (`icon.ico`, `kmedia.rc`), macOS (`kmedia.icns`), Linux/Qt (`icon.png`), vector (`logo.svg`), website favicon icon assets, and README product banner.
- Repository documentation (`CONTRIBUTING.md`, `CHANGELOG.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`) and user-focused `README.md`.
- Core persistence and profile engine (`DatabaseManager` with SQLite3 WAL migrations, `ProfileManager`, `SettingsStore`, `NetworkManager`, `SystemUtils`, and disk-backed LRU `ThumbnailCache`) with Catch2 unit tests.
- Repository automation (`.github/dependabot.yml`, `.github/labeler.yml`, PR Conventional Commit title and issue traceability check, `Docs Sync` documentation enforcement, and CodeQL scanning) plus domain business rules in `docs/architecture/business-rules.md`.
- Media engine and dual-screen presentation modules (`MpvRendererItem` transport/trims/tracks/zoom-pan, `PlaybackController`, `BackgroundMusicEngine` with scheduled 5-second fade-out, `DisplayManager` with `PresentationWindow`, and `PdfViewerController` with `PdfPageImageProvider`) plus Catch2 unit tests.
- Native content parsers and cloud sync pipeline (`ZipArchiveReader` with minizip-ng Zip-Slip guards, `JwpubParser`, `S34Parser`, `JwlPlaylistParser`, `CatalogClient`, `DownloadService` with ETag caching and SHA-256 verification, and event-driven `CloudSyncWatcher`) plus Catch2 integration tests.
- Native booth integrations (`ProcessMonitor`, `ObsWebSocketClient` for OBS WebSocket v5 SHA-256 auth and scene automation, and `ZoomBridge` `zoommtg://` launcher).
- Redesigned QML Studio design system (`resources/qml/controls/Studio*.qml`) with dual **Dark (`Studio Carbon`)** and **Light (`Studio Daylight`)** themes (`ui/dark_theme` + `--theme`), bundled `Inter` font family, `MultiEffect`-tinted Lucide vector icons, custom `.jwpub` / `.jwlplaylist` SVG badges, **Meeting Section Dividers** (*Treasures From God's Word*, *Apply Yourself to the Field Ministry*, *Living as Christians*), **Floating Pill Action Bar (`ActionPillBar`)**, **Command Palette (`Ctrl+K`)**, toast notifications, playlist drag-and-drop, non-destructive trim/zoom inspector, media/playlists/downloads/settings views, and responsive scaling down to `360x520`.
- Complete EN/PT Qt Linguist catalogs (`resources/translations/kmedia_en.ts`, `kmedia_pt.ts`), `--demo` / `--screenshot` automated screenshot workflow (`.github/workflows/screenshot-refresh.yml`), release automation (`release-please-config.json`, `.github/workflows/release-please.yml`, `.github/workflows/release.yml` with SHA-256 sums), workflow security auditing (`.github/workflows/zizmor.yml`), multi-IDE & multi-AI contributor configurations (`AGENTS.md`, `CLAUDE.md`, `.cursor/rules/`, `.github/copilot-instructions.md`, `.windsurfrules`, `.ai/system-prompt.md`, `.vscode/`, `.zed/`, `.editorconfig`), and `THIRD-PARTY-NOTICES.md`.

## [0.1.0] - 2026-11-18

### Added

- Clean-room C++20 / CMake 3.25 / Ninja / vcpkg project structure with `kmedia_core` library and `KMedia.UI` Qt 6 Quick module.
- OpenGL FBO `MpvRendererItem` with thread-safe `mpv_render_context` integration and safe hardware decoding (`hwdec=auto-safe`).
- Studio Carbon & Broadcast Orange three-column operator shell (`Theme.qml`, `Main.qml`, rail, workspace, and inspector components).
- Multi-OS GitHub Actions CI matrix (Ubuntu, Windows MSVC, macOS), `clang-format` check, `qmllint` verification, and Catch2 smoke test suite.
- Bilingual VitePress documentation site (English and Portuguese) deployed to GitHub Pages.
