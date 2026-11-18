# KMedia

**KMedia** is a native, cross-platform desktop application written from scratch in **C++20**, **Qt 6**, and **libmpv** to download, organize, and present audio, video, images, and PDF documents for congregation meetings.

I built KMedia to replace heavyweight browser-wrapped media presenters with a cold, quiet, low-memory workstation engineered for Kingdom Hall laptops and hybrid A/V booths.

- **Documentation & Website:** [matheuscardosos.github.io/kmedia](https://matheuscardosos.github.io/kmedia/)
- **License:** [GNU Affero General Public License v3.0 (AGPL-3.0)](LICENSE)

## Engineering Goals

- **Low Memory Footprint:** Under 45 MB RAM at idle and under 75 MB RAM during 1080p video playback.
- **Thermal & CPU Efficiency:** Near-zero CPU usage at idle and mandatory GPU hardware-accelerated video decoding (`hwdec=auto-safe` via `libmpv`).
- **Instant Startup:** Sub-400 ms cold start backed by SQLite WAL persistence and disk-backed WebP/JPEG thumbnail caching.
- **Clean-Room Native Stack:** 100% idiomatic C++20 and Qt Quick (QML), with native `.jwpub`, `.jwlplaylist`, `Qt6::Pdf`, OBS Studio WebSocket v5, and Zoom integration.

## Visual Identity

KMedia uses a **Studio Carbon & Broadcast Orange** dark interface calibrated for zero screen glare in dimly lit auditoriums:

- Window Base: `#0D0F12`
- Panel Surface: `#14171C`
- Card / Row Surface: `#1B1F26`
- Primary Accent: `#FF6B00`

## Tech Stack

- **Language:** C++20
- **UI Framework:** Qt 6.8+ (`Qt6::Quick`, `Qt6::QuickControls2`, `Qt6::Pdf`, `Qt6::Sql`, `Qt6::Network`, `Qt6::WebSockets`)
- **Video & Audio Engine:** `libmpv` (`mpv_render_context` OpenGL FBO interoperability)
- **Build & Dependencies:** CMake 3.25+, Ninja, `vcpkg` manifest mode (`catch2`, `minizip-ng`)

## Building from Source

### Prerequisites

- CMake 3.25+ and Ninja
- C++20 compiler (MSVC 2022, MinGW-w64 GCC 13+, Clang 16+, or Apple Clang)
- Qt 6.8+ with `Quick`, `QuickControls2`, `OpenGL`, `Sql`, `Network`, `WebSockets`, `Pdf`, and `LinguistTools`
- `libmpv` development headers and library (`MPV_ROOT` on Windows, `libmpv-dev` on Ubuntu/Debian, `mpv` via Homebrew on macOS)
- `vcpkg` (`VCPKG_ROOT` environment variable set)

### Configure, Build, and Test

```bash
cmake --preset <preset>
cmake --build --preset <preset>
ctest --preset <preset> --output-on-failure
```

Available presets in `CMakePresets.json`:

- `windows-msvc-release` / `windows-msvc-debug`
- `windows-mingw-release` / `windows-mingw-debug`
- `linux-release` / `linux-debug`
- `macos-release`

## Repository Roadmap

1. **Phase 1 (Current):** Repository foundation, CMake/vcpkg presets, CI/CD, VitePress site, and compiling Qt 6 + `libmpv` Studio Carbon shell.
2. **Phase 2:** Core persistence and profile engine (`DatabaseManager`, `ProfileManager`, `SettingsStore`, `ThumbnailCache`).
3. **Phase 3:** Media engine and dual-screen presentation (`MpvRendererItem`, `PlaybackController`, `BackgroundMusicEngine`, `DisplayManager`).
4. **Phase 4:** Native `.jwpub` / `.jwlplaylist` parsers, watched-folder synchronization, and media download service.
5. **Phase 5:** Process monitoring, OBS WebSocket v5 client, Zoom bridge, translations, and polished three-column operator UI.
