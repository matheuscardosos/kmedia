# Claude Code Guide for KMedia

Read and follow [AGENTS.md](AGENTS.md) and [docs/architecture/business-rules.md](docs/architecture/business-rules.md) as the canonical project rules.

## Project Stack
- **Language / Standard:** C++20 (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion`)
- **UI Framework:** Qt 6.8+ Qt Quick / QML (`KMedia.UI` module) with custom `Studio*` controls, dual Dark/Light `Theme.qml`, Lucide SVG icons, and `Inter` typography
- **Media Engine:** OpenGL FBO `libmpv` (`MpvRendererItem`) with non-destructive trim/zoom and `Qt6::Pdf`
- **Build & Packages:** CMake 3.25+ + Ninja + vcpkg (`Catch2`, `minizip-ng`) via `CMakePresets.json`
- **Docs:** Bilingual EN/PT VitePress site under `docs/`

## Daily Verification Commands
```bash
cmake --preset windows-mingw-release # or linux-release / macos-release / windows-msvc-release
cmake --build --preset <preset>
cmake --build --preset <preset> --target kmedia_qmllint
QT_QPA_PLATFORM=offscreen ctest --preset <preset> --output-on-failure
```

## Non-Negotiable Constraints
- Keep `kmedia_core` decoupled from QML (`src/core`, `src/media`, `src/models`, `src/parsers`, `src/network`, `src/integrations`).
- Update `docs/src/en/`, `docs/src/pt/`, `docs/architecture/business-rules.md`, and `CHANGELOG.md` whenever user-facing behavior or UI changes.
- Update `resources/translations/kmedia_{en,pt}.ts` with zero unfinished translations when UI strings change.
- Never use em-dashes or en-dashes in code, UI strings, docs, or commits.
- Reference GitHub issues (`Refs #<id>` or `Closes #<id>`) in every tracked commit/PR.
