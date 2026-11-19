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

## Architecture, Business Rules, and Engineering Targets

Review the domain behavioral contracts in [docs/architecture/business-rules.md](docs/architecture/business-rules.md) before modifying core services, media playback, dual-screen presentation, or integrations.

- **Memory footprint target:** Under 45 MB RAM at idle and under 75 MB RAM during 1080p playback.
- **Startup target:** Under 400 ms cold startup backed by SQLite WAL persistence and disk-backed thumbnail caching.
- **CPU and thermal efficiency:** Zero polling loops in steady state; use event-driven OS APIs and `hwdec=auto-safe` via `libmpv`.
- **Clean separation:** Keep all persistence, parsing, networking, and process monitoring inside `kmedia_core` without coupling to QML.

## UI Design System

KMedia provides dual **Studio Carbon (Dark)** and **Studio Daylight (Light)** palettes calibrated for dimly lit A/V booths or daylight rehearsal setups (see [resources/qml/Theme.qml](resources/qml/Theme.qml)), built with shared `Studio*` controls ([resources/qml/controls/](resources/qml/controls)), bundled `Inter` typography, and `MultiEffect`-tinted Lucide vector icons:

| Role | Dark Hex | Light Hex | Token |
| :--- | :--- | :--- | :--- |
| Window Base | `#0D0F12` | `#F1F3F6` | `windowBase` |
| Panel Surface | `#13161C` | `#FFFFFF` | `panelSurface` |
| Card / Row Surface | `#181C23` | `#F8FAFC` | `cardSurface` |
| Elevated Hover | `#242A35` | `#ECEFF4` | `cardHover` |
| Subtle Border | `#262C36` | `#E2E6ED` | `borderHairline` |
| Primary Accent | `#FF6B00` | `#FF6B00` | `accentPrimary` |
| Primary Text | `#F3F4F6` | `#111827` | `textPrimary` |
| Secondary Text | `#9CA3AF` | `#4B5563` | `textSecondary` |

Layout adapts smoothly across three responsive breakpoints (`Wide >= 1080 px` three-column workstation, `Medium 680-1079 px` collapsed icon rail, and `Compact 360-679 px` single-column strip for side-by-side operation with Zoom).

## Editor and AI Assistant Configuration

The repository ships synchronized setups across common IDEs and coding agents so every contributor starts with consistent tooling:

- **Universal Editors:** [`.editorconfig`](.editorconfig) and [CMakePresets.json](CMakePresets.json) (VS Code, CLion, Qt Creator, Visual Studio, Neovim).
- **VS Code & Zed:** [`.vscode/`](.vscode/) (`settings.json`, `extensions.json`, `tasks.json`, `launch.json`) and [`.zed/settings.json`](.zed/settings.json).
- **AI Assistants:** [`AGENTS.md`](AGENTS.md), [`CLAUDE.md`](CLAUDE.md), [`.cursor/rules/`](.cursor/rules/), [`.github/copilot-instructions.md`](.github/copilot-instructions.md), [`.windsurfrules`](.windsurfrules), [`.aider.conf.yml`](.aider.conf.yml), and [`.ai/system-prompt.md`](.ai/system-prompt.md) (for Grok, DeepSeek, or external models).

## Releases

Versioning and changelogs are automated via [`release-please-config.json`](release-please-config.json) (updating `CMakeLists.txt`, `vcpkg.json`, `docs/package.json`, and `CHANGELOG.md`). Publishing a release or pushing a `v*` tag triggers [`.github/workflows/release.yml`](.github/workflows/release.yml) to build and attach signed SHA-256 packages for Windows, macOS, and Linux.

## Building from Source

### Prerequisites

- **CMake** 3.25 or newer and **Ninja**
- **C++20 compiler:** MSVC 2022+, MinGW-w64 GCC 13+, Clang 16+, or Apple Clang
- **Qt 6.8+** with modules: `Core`, `Gui`, `LinguistTools`, `Network`, `OpenGL`, `Pdf`, `Quick`, `QuickControls2`, `Sql`, `Svg`, `Test`, and `WebSockets`
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

1. Check open issues and milestones before starting larger features, or open an issue to track non-trivial changes first.
2. Create a focused branch from `main` and reference the related issue in commits and PR descriptions (`Refs #<number>` or `Closes #<number>` whenever an issue exists, or note `No-Issue: <reason>` / `skip-issue` for trivial standalone chores).
3. Keep user manuals (`docs/src/en/`, `docs/src/pt/`), [docs/architecture/business-rules.md](docs/architecture/business-rules.md), and `CHANGELOG.md` in sync whenever adding features or changing settings/UI behavior. CI enforces this via the `Docs Sync` check (use the `skip-docs` PR label only for purely internal refactors or bugfixes without user-visible behavior changes).
4. Ensure formatting, `kmedia_qmllint`, and all Catch2/Qt tests pass locally.
5. Submit a pull request using the repository template with clear verification notes.
