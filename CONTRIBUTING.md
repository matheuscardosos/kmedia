# Contributing to KMedia

Thank you for helping improve **KMedia**. I appreciate bug reports, feature suggestions, translations, documentation improvements, and pull requests.

## Repository Structure

```text
kmedia/
├── cmake/                  # CMake find modules and platform workarounds
├── docs/                   # VitePress bilingual documentation website (EN / PT)
├── resources/
│   ├── icons/              # Application icons (.svg, .png, .ico, .icns, .rc)
│   └── qml/                # Qt Quick (QML) interface and reusable components
├── src/
│   ├── app/                # QML-exposed application controllers
│   ├── core/               # Pure C++/Qt non-UI services (kmedia_core)
│   └── media/              # libmpv OpenGL renderer and media playback components
└── tests/                  # Catch2 + Qt Test unit and integration suite
```

## Architecture and Engineering Targets

- **Memory footprint target:** Under 45 MB RAM at idle and under 75 MB RAM during 1080p playback.
- **Startup target:** Under 400 ms cold startup backed by SQLite WAL persistence and disk-backed thumbnail caching.
- **CPU and thermal efficiency:** Zero polling loops in steady state; use event-driven OS APIs and `hwdec=auto-safe` via `libmpv`.
- **Clean separation:** Keep all persistence, parsing, networking, and process monitoring inside `kmedia_core` without coupling to QML.

## UI Design System

KMedia uses a dark **Studio Carbon & Broadcast Orange** palette calibrated to eliminate screen glare inside dimly lit A/V booths:

| Role | Token | Hex |
| :--- | :--- | :--- |
| Window Base | `windowBg` | `#0D0F12` |
| Panel Surface | `surfaceBg` | `#14171C` |
| Card / Row Surface | `cardBg` | `#1B1F26` |
| Elevated Hover | `hoverBg` | `#222730` |
| Subtle Border | `borderColor` | `#262B34` |
| Primary Accent | `accent` | `#FF6B00` |
| Primary Text | `textPrimary` | `#F0F2F5` |
| Secondary Text | `textSecondary` | `#8E95A2` |

Layout follows a fixed three-column workstation grid: a compact left navigation rail, a dense center playlist/media workspace, and a right-hand live preview and inspector panel.

## Building from Source

### Prerequisites

- **CMake** 3.25 or newer and **Ninja**
- **C++20 compiler:** MSVC 2022+, MinGW-w64 GCC 13+, Clang 16+, or Apple Clang
- **Qt 6.8+** with modules: `Core`, `Gui`, `LinguistTools`, `Network`, `OpenGL`, `Pdf`, `Quick`, `QuickControls2`, `Sql`, `Test`, and `WebSockets`
- **libmpv** development headers and library (`MPV_ROOT` on Windows, `libmpv-dev` on Debian/Ubuntu, or `mpv` via Homebrew on macOS)
- **vcpkg** with `VCPKG_ROOT` set in your environment (`catch2` and `minizip-ng` are managed in manifest mode via `vcpkg.json`)

### Configure, Build, and Test

```bash
cmake --preset <preset>
cmake --build --preset <preset>
cmake --build --preset <preset> --target kmedia_qmllint
QT_QPA_PLATFORM=offscreen ctest --preset <preset> --output-on-failure
```

Available presets in `CMakePresets.json`:

- `windows-msvc-release` / `windows-msvc-debug`
- `windows-mingw-release` / `windows-mingw-debug`
- `linux-release` / `linux-debug`
- `macos-release`

### Code Style

- C++ sources follow `.clang-format` and `.clang-tidy`. Run `clang-format --dry-run --Werror` on changed `.h` and `.cpp` files before submitting a PR.
- Keep user-facing English text clear, concise, and free of em-dashes or en-dashes.

## Pull Request Process

1. Check open issues and milestones before starting larger features.
2. Create a focused branch from `main` referencing the related issue.
3. Ensure formatting, `kmedia_qmllint`, and all Catch2/Qt tests pass locally.
4. Submit a pull request using the repository template with clear verification notes.
