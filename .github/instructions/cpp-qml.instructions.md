---
applyTo: "src/**,tests/**,resources/qml/**"
---

# C++20 and QML Implementation Guidelines

- Keep business logic headless in `kmedia_core`; expose reactive state cleanly through `ApplicationController` and `PlaylistModel`.
- Follow `.clang-format` and `.clang-tidy` naming (`m_` fields, `camelCase` methods, `CamelCase` types) with zero conversion/sign warnings.
- In QML, use `pragma ComponentBehavior: Bound`, `required property` delegates, unversioned Qt 6 imports, `Font.DemiBold`, `Layout.preferredWidth`/`Height` inside layouts, `Studio*` design system controls, and `Theme` tokens for Dark and Light mode.
- Always run `--target kmedia_qmllint` and `ctest --output-on-failure` after edits.
