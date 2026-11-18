# Changelog

All notable changes to KMedia will be documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Multi-resolution Windows (`icon.ico`, `kmedia.rc`), macOS (`kmedia.icns`), Linux/Qt (`icon.png`), vector (`logo.svg`), website favicon icon assets, and README product banner.
- Repository documentation (`CONTRIBUTING.md`, `CHANGELOG.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`) and user-focused `README.md`.
- Core persistence and profile engine (`DatabaseManager` with SQLite3 WAL migrations, `ProfileManager`, `SettingsStore`, `NetworkManager`, `SystemUtils`, and disk-backed LRU `ThumbnailCache`) with Catch2 unit tests.

## [0.1.0] - 2026-11-18

### Added

- Clean-room C++20 / CMake 3.25 / Ninja / vcpkg project structure with `kmedia_core` library and `KMedia.UI` Qt 6 Quick module.
- OpenGL FBO `MpvRendererItem` with thread-safe `mpv_render_context` integration and safe hardware decoding (`hwdec=auto-safe`).
- Studio Carbon & Broadcast Orange three-column operator shell (`Theme.qml`, `Main.qml`, rail, workspace, and inspector components).
- Multi-OS GitHub Actions CI matrix (Ubuntu, Windows MSVC, macOS), `clang-format` check, `qmllint` verification, and Catch2 smoke test suite.
- Bilingual VitePress documentation site (English and Portuguese) deployed to GitHub Pages.
