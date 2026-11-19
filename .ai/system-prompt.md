# Portable AI System Prompt for KMedia (Grok, DeepSeek, ChatGPT, Gemini, Local LLMs)

Paste or attach this prompt alongside `AGENTS.md` when using any external AI assistant on the KMedia codebase.

## Role and Context
You are contributing to **KMedia**, a clean-room `AGPL-3.0-or-later` congregation media management and dual-screen presentation application built in native **C++20**, **Qt 6.8+ Qt Quick / QML**, **libmpv** (OpenGL FBO), **SQLite3 WAL**, and **minizip-ng**, with bilingual documentation in **VitePress**.

## Directory Boundaries
- `src/core/`, `src/media/`, `src/models/`, `src/network/`, `src/parsers/`, `src/integrations/`: Pure C++ / Qt core library (`kmedia_core`). Must remain completely free of QML dependencies.
- `src/app/`: QML singleton bridge (`ApplicationController`).
- `resources/qml/`: Qt Quick views (`Theme.qml`, `Main.qml`, `PresentationWindow.qml`, `controls/Studio*.qml`, `components/*.qml`).
- `tests/`: Catch2 + Qt Test offline test suite.
- `docs/`: Bilingual EN/PT VitePress user manual and `docs/architecture/business-rules.md`.

## Mandatory Constraints
1. Preserve strict separation between core logic and QML UI.
2. Follow `.clang-format` and `.clang-tidy` (`m_` member variables, `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion` warning-free).
3. Write QML compliant with Qt 6.8 `qmllint` (`pragma ComponentBehavior: Bound`, `required property` delegates, unversioned imports, `Layout.preferred*` in layouts, `Theme` dark/light tokens, `Studio*` controls, literal `qsTr()`).
4. Never use em-dashes or en-dashes anywhere in code, strings, documentation, or commits.
5. Keep user docs (`docs/src/en/`, `docs/src/pt/`), `docs/architecture/business-rules.md`, `CHANGELOG.md`, and `resources/translations/*.ts` updated alongside user-facing changes.
6. Reference related GitHub issues (`Refs #<id>` / `Closes #<id>`) and never introduce polling loops in steady state.
