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
  its rarity, style/quality words are painted silver rather than taking the
  base name's white, and Monster Infrequents take their own colors. Every
  category is derived from the game database; none uses a hand-maintained list.
  The original database-only rule differed on 149 of roughly 4,500 lines
  against the supplied game-version-1.3.0 snapshot. Generic category and
  specialized loot-chain rules reduce the latest snapshots to 12 base, 9 GDX1,
  and 8 GDX2 color-only differences without a per-item table.
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

- Fixed `--full-rainbow` coloring random-crafting result labels and Loyalist
  illusion equipment names that Full Rainbow intentionally leaves uncolored.
- Fixed `--full-rainbow` leaving unique tier style words and quest-item names
  uncolored when no item record owns their localization tags.
- Fixed missing-record members of the wooden-quality, base head/monster torso,
  and faction shoulder/torso families remaining uncolored.
- Fixed `Broken` enemy weapons remaining uncolored in Full Rainbow mode without
  making them affixable in gdse's default scheme.
- Fixed specialized `LevelTable` chains for named monster and boss families
  being skipped while generic tier wrappers remain excluded.
- Fixed crafted Rare bases taking Rare green instead of the Magical color of
  the blueprint result, and named boss-chest and descriptor-less nemesis/tomb
  loot chains being skipped.
- Restricted the crafted-result override to expansion-tagged results,
  descriptor-less loot parents to named nemesis/tomb families, and boss-chest
  MI colors to GDX1 tags, avoiding base-game regressions.
- Fixed invisible GDX1 illusion equipment receiving Common white.
- Fixed item, affix, style, and quality tag inference so shared tags, unknown
  item classifications, rarity ties, faction items, and affixable name parts
  receive the intended colors.
- Fixed ARZ record decoding dropping every string field that holds an array
  rather than a single value. Arrays now decode as one field per element.
- Fixed color insertion for bracket placeholders, numeric pipe placeholders,
  existing color markers, and non-ASCII text, including bounds and allocation
  error handling for large values.
