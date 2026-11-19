# GitHub Copilot Instructions for KMedia

Read [AGENTS.md](../AGENTS.md) and [docs/architecture/business-rules.md](../docs/architecture/business-rules.md) before proposing changes.

- **Build & Language:** Native C++20, Qt 6.8+ Qt Quick (`KMedia.UI`), `libmpv`, `Qt6::Pdf`, SQLite3 WAL, `minizip-ng`, Catch2, VitePress docs.
- **Checks Required:** `clang-format --dry-run --Werror`, Release build without warnings, `--target kmedia_qmllint`, `QT_QPA_PLATFORM=offscreen ctest --output-on-failure`, and `docs:typecheck` + `docs:build` when docs change.
- **Rules:** Docs-as-code for user-visible changes, EN/PT `.ts` localization updates, zero em/en-dashes anywhere, zero polling in steady state, Conventional Commits with `Refs #<id>` or `Closes #<id>`.
