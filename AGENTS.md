# Engineering Rules for KMedia Contributors and AI Agents

## 1. Architecture and Clean-Room Discipline
- Keep all database persistence (`SQLite3` WAL), media parsers (`.jwpub`, `.jwlplaylist`, `S-34`), networking, catalog fetching, process monitoring, and booth integrations inside `src/core/`, `src/media/`, `src/models/`, `src/network/`, `src/parsers/`, and `src/integrations/` (`kmedia_core`) without any QML dependency.
- Keep `src/app/` and `resources/qml/` strictly presentation-focused using the custom `Studio*` primitives (`resources/qml/controls/`), semantic `Theme` tokens (`resources/qml/Theme.qml`), Lucide vector icons (`resources/icons/lucide/`), and the bundled `Inter` font family.
- Maintain strict clean-room development under `AGPL-3.0-or-later`. Never copy source files verbatim from third-party GPL repositories; implement KMedia's native C++20 / Qt 6 architecture independently and document third-party assets in `THIRD-PARTY-NOTICES.md`.
- Maintain zero polling loops in steady state; prefer Qt signals/slots, `QFileSystemWatcher`, and event-driven OS notifications (`ProcessMonitor` uses a gentle `3 s` native OS snapshot only while active).

## 2. Docs-as-Code and Localization
- Whenever adding or changing user-facing features, settings, UI layouts, parsers, CLI flags, or integrations, update `docs/src/en/`, `docs/src/pt/`, `docs/architecture/business-rules.md`, and `CHANGELOG.md` alongside the C++/QML changes.
- Update Qt Linguist catalogs (`resources/translations/kmedia_en.ts` and `resources/translations/kmedia_pt.ts`) using `lupdate` whenever QML or C++ `qsTr()` / `tr()` strings change, keeping unfinished entries at zero.

## 3. Formatting, Style, and Language
- Keep all code, comments, CMake files, workflows, documentation, and commits in English (use first-person singular `"I"` in repository prose/commits where personal voice applies), with zero AI attribution or unfinished stub placeholders (`TODO`, `FIXME`).
- Never add em-dashes or en-dashes anywhere in code, comments, UI strings, docs, or commit messages. Use `:`, `,`, or standard ASCII `-` instead.
- C++20 code follows `.clang-format` and `.clang-tidy` (`CamelCase` types, `camelCase` functions/locals, `m_` member prefix).
- QML files use unversioned Qt 6 imports, `pragma ComponentBehavior: Bound`, `required property` on delegates, explicit `id` qualification, `Layout.preferredWidth` / `Layout.preferredHeight` instead of raw `width` / `height` inside layouts, and plain string literals inside `qsTr()`.

## 4. Repository Hygiene and Local Exclusions
- Keep personal scratch briefings and local agent session caches (`project_engineering_briefing.md`, `.agents/`, `.agent/`, `.claude/`, `.gemini/`, raw logo master files) ignored via `.git/info/exclude`. Verify `git status` before every commit.
- Keep repository-wide contributor configs (`AGENTS.md`, `CLAUDE.md`, `.cursor/rules/`, `.github/copilot-instructions.md`, `.windsurfrules`, `.ai/`, `.vscode/`, `.zed/`, `.editorconfig`) tracked and synchronized.

## 5. Mandatory Pre-Commit Verification
Run and verify all applicable checks cleanly before committing:
- `clang-format --dry-run --Werror` on changed `.h`, `.hpp`, and `.cpp` files
- `cmake --build --preset <preset>` (Release build with zero warnings)
- `cmake --build --preset <preset> --target kmedia_qmllint` (zero QML warnings/errors)
- `QT_QPA_PLATFORM=offscreen ctest --preset <preset> --output-on-failure`
- Inside `docs/`: `npm run docs:typecheck` and `npm run docs:build` whenever `docs/` changes

## 6. Issue Traceability and Conventional Commits
- Follow Conventional Commits (`feat:`, `fix:`, `docs:`, `refactor:`, `perf:`, `test:`, `build:`, `ci:`, `chore:`) on all PR titles and commits.
- Always reference the related GitHub issue (`Refs #<id>` or `Closes #<id>`) in commit messages and pull requests whenever an issue exists (or open an issue beforehand for tracked feature/bug work), or provide `No-Issue: <reason>` for standalone chores.
