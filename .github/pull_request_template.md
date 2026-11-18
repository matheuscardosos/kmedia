## Summary

Describe the changes in this pull request and reference the related issue (`Closes #...`).

## Documentation

- [ ] Updated user guides in `docs/src/en/` and `docs/src/pt/` if user-facing behavior or settings changed
- [ ] Updated `docs/architecture/business-rules.md` if domain behavior or invariants changed
- [ ] Updated `CHANGELOG.md` under `[Unreleased]` (or applied `skip-docs` label for purely internal changes)

## Testing

- [ ] Formatted C++ sources (`clang-format`)
- [ ] Built cleanly in Release mode with zero new warnings (`cmake --build`)
- [ ] Passed QML linting (`--target kmedia_qmllint`) and all tests (`ctest --output-on-failure`)
