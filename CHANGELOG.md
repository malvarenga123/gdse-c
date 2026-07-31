# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
unless the repository documents a different versioning policy.

## [Unreleased]

### Added

- Added a `--full-rainbow` option that widens the item color scheme toward the
  Rainbow Filter mod: Epic and Legendary names are colored instead of left to
  the engine, set-item names are marked with `(S) `, faction gear is colored by
  its rarity, and style/quality words are painted silver rather than taking the
  base name's white, and Monster Infrequents take their own colors, identified
  from monster drop-slot references to loot tables rather than from any
  hand-maintained list.
- Added ISO C89 builds with POSIX `make`, including vendored LZ4 and utf8proc
  dependencies so builds do not need to download third-party packages.
- Added native MinGW32 and MSYS2 build support for Windows.
- Added staged output publication, an ownership manifest for removing stale
  generated files without affecting unrelated files, and best-effort rollback
  of existing generated files when publication fails.

### Changed

- Replaced the Rust/Cargo implementation and build workflow with the ISO C89
  application and Make workflow.
- Made the optional Grim Dawn installation path a positional argument instead
  of reading `GRIM_DAWN_INSTALL_PATH` from the environment, defaulting to the
  current directory when omitted.
- Enabled Rainbow Filter damage colors by default and replaced the opt-in flag
  with `--no-rainbow-filter-damage-colors` for disabling them. English remains
  the default language when `--language` is omitted.
- Made missing base-game data and existing unreadable or corrupt archives fail
  with contextual errors instead of producing incomplete output; absent DLC
  archives remain optional.
- Archive record paths are now validated before filesystem use, and unsafe
  paths stop generation.
- Improved processing performance by indexing localization tags and inferred
  item relationships instead of repeatedly scanning them.
- Changed duplicate localization records from later expansion archives to
  override earlier records according to the game's archive overlay order.
- Generated localization files and `.gdse-manifest` now use Windows CRLF line
  endings while preserving whether source localization files have a final
  newline.

### Fixed

- Fixed item, affix, style, and quality tag inference so shared tags, unknown
  item classifications, rarity ties, faction items, and affixable name parts
  receive the intended colors.
- Fixed color insertion for bracket placeholders, numeric pipe placeholders,
  existing color markers, and non-ASCII text, including bounds and allocation
  error handling for large values.
