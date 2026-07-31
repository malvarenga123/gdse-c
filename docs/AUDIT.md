# Repository audit and remediation backlog

## Audit record

- **Original audit:** 2026-07-30 at Rust commit `b0a9042cbba84d3681cb993b3015e645c17ff4f3`.
- **C89 remediation:** branch `c89`, based on `0e8706c0bb453767c585fe9f9596f4836f839ee4`.
- **Upstream reference:** `origin` is configured, and the pre-fork Rust implementation remains available on the `mainline` branch. Comparing C behavior against that reference is the expected way to validate port parity.
- **Scope:** The Rust/Cargo implementation was replaced by an ISO C89 application, focused tests, Make build, vendored LZ4/utf8proc, and updated durable guidance.
- **Validated since:** English end-to-end output on a licensed installation of game version 1.3.0 — see *Full Rainbow parity* below. The ARZ and ARC readers, the inference pass, and the rewriter all ran against real version-3 archives to produce it.
- **Unvalidated:** Windows runtime, non-English archives, crash injection during publication, representative performance measurements, and a byte-for-byte comparison against the pre-fork Rust executable.

## Remediation validation

| Command | Result |
| --- | --- |
| `make clean && make check` | Exit 0; all project sources compile with strict C89 diagnostics and all focused tests pass. |
| `./gdse --help` | Exit 0; documents the supported CLI surface. |
| `./gdse --version` | Exit 0; reports `gdse 0.1.0-c89`. |
| `./gdse` outside a Grim Dawn installation | Exit 1 as designed; the current directory is assumed and its missing mandatory database is diagnosed. |
| `./gdse /nonexistent` | Exit 1 as designed; invalid install paths are diagnosed. |
| `make clean && make CC=i686-w64-mingw32-gcc EXE=.exe all` | Exit 0; produces a 32-bit Windows executable with strict C89 diagnostics on project sources. |
| `make OS=Windows_NT SHELL=/bin/sh clean build` | Exit 0; the MSYS2-style environment selects POSIX recipes while retaining `.exe` targets. |

## AUD-001 — Core transformation and inference behavior has no automated tests

- **Status:** Resolved
- **Classification:** Observed
- **Original severity:** High
- **Resolution:** `tests/test_rules.c` adds table-driven coverage for all color-placement branches, replacement/placeholder ordering, property exclusions and palette selection, Unicode letters, CRLF normalization and no-final-newline preservation, conversion behavior, rarity ties, affixability, and name-part coloring. The Make `check` target compiles and runs the suite.
- **Evidence:** `src/rules.c`, `tests/test_rules.c`, `Makefile`; `make clean && make check` exits 0.
- **Remaining limitation:** No proprietary fixture was vendored. ARZ/ARC parsing and complete game output still require a licensed manual smoke test.

## AUD-002 — Input archive and database failures are silently treated as optional content

- **Status:** Resolved
- **Classification:** Observed
- **Original severity:** High
- **Resolution:** The base database and requested base language archive are mandatory. DLC inputs are optional only when absent. Every existing input open, header, metadata, decompression, and record error is contextual and fatal before publication; the CLI returns nonzero.
- **Evidence:** `src/main.c::load_database`, `process_archive`; checked readers in `src/archive.c`.
- **Compatibility note:** Damaged installations that formerly produced incomplete output now fail explicitly, as approved for this rewrite.
- **Remaining limitation:** Which future DLC inputs exist is still encoded in the current four-entry list and requires maintenance if the game adds another expansion.

## AUD-003 — Output publication is non-atomic and does not reconcile stale files

- **Status:** Resolved with platform limitation
- **Classification:** Observed
- **Original severity:** Medium
- **Resolution:** Generation writes a complete sibling staging tree before publication. Unsafe record paths fail validation, while duplicate destinations follow archive overlay order and replace the earlier staged file. `.gdse-manifest` records ownership and stale cleanup deletes only previously owned paths. Publication backs up existing destinations, uses renames, and restores backups after an observed publication failure.
- **Evidence:** `src/main.c::process_archive`, `publish`; `src/util.c::gd_safe_record_path`.
- **Remaining limitation:** Portable C89 cannot guarantee a transactional multi-file directory swap while coexisting with unrelated files. Individual renames and rollback are best-effort across crashes and filesystem/platform semantics. Crash-injection and Windows tests remain required before claiming stronger atomicity.

## AUD-004 — Record filtering eagerly collects every raw database record

- **Status:** Resolved; real-data performance remains unmeasured
- **Classification:** Inferred
- **Original severity:** Medium
- **Resolution:** The C reader stores only fixed-size record offsets and the format-required string table. Item payloads are decompressed, folded into inference state, and freed one at a time. ARC payloads are likewise processed one at a time. The former simultaneous raw-record and resolved-record vectors no longer exist. Dynamically resized hash indexes now make tag insertion, inference joins, and localization lookup amortized constant-time, and identical part/base relationships are stored once rather than once per record.
- **Evidence:** `src/archive.c::gd_arz_open`, `gd_arz_record`; `src/rules.c::gd_infer_database`, `gd_inference_ensure_tag`, `gd_inference_add_part`, `gd_tag_color`; `tests/test_rules.c::index_cases`; `make clean && make check` exits 0 with 20,000 distinct synthetic tags, 20,000 lookups, and 20,000 duplicate relationships.
- **Remaining limitation:** Representative peak-RSS and wall-time measurements cannot be collected without licensed databases. Inference state naturally still scales with unique relevant tags and unique relationships, and real game archives remain a manual performance-validation requirement.

## Full Rainbow parity (2026-07-31)

`--full-rainbow` was developed against a diff of gdse's generated `tags_items.txt` and the `tags_items.txt` distributed with the Full Rainbow mod, taken on a licensed installation of game version 1.3.0. This section records what that comparison established so the analysis does not have to be repeated.

### Measurement

| Build | Differing lines |
| --- | --- |
| gdse's own scheme | 1,224 |
| `--full-rainbow` | 273 |

No line differs in text. Every difference in either run is a color marker or Full Rainbow's `(S) ` set marker; no tag is added, dropped, reworded, or reordered. All 209 set names Full Rainbow marks are reproduced exactly.

### Classification of the remaining 273 lines

| Cause | Lines | Actionable |
| --- | --- | --- |
| Monster Infrequent coloring | 148 | Only with a new inference pass over creature loot tables |
| Tags absent from Full Rainbow's list | 103 | No — gdse colors ordinary gear it has no entry for |
| Tags with no item record at all | 15 | No |
| Enemy-only gear | 5 | No |
| `Empowered` / `Mythical` unique styles | 2 | No |

### Findings

- **Monster Infrequents are not distinguishable from `records/items/` fields.** Full Rainbow paints them `{^L}`, and `{^Z}`/`{^F}` at Epic/Legendary tier. The distinction tracks whether a named creature drops the item, which lives in creature and loot-table records outside the `records/items/` scope gdse reads. Base rarity does not separate them: `Bloodsworn Repeater` is `{^L}` while `Hand Mortar`, `Shrapnel Pistol` and `Francis' Gun` are `{^G}`, and gdse classifies all four identically as Rare bases.

- **Tags with no item record at all account for 15 lines.** Confirmed absent from the database: `tagHeadA010`, `tagShieldA011`, `tagQualityWeaponWood06` through `11`, `tagQuestItemSlithRing`, `tagShoulderF005`, `tagShoulderF010`, `tagTorsoF005`, `tagTorsoF010`. `tagQuestItemBrothersAmulet` and `tagItemTest` are presumed the same but were not separately confirmed. Full Rainbow colors text the game never displays; inference has nothing to work from. `tagQualityWeaponWood05`, which does have six records, is colored correctly, so the mechanism is sound.

- **Enemy-only gear accounts for 5 lines** — `tagWeaponArcaneA001/A003/A005/A007` and `tagTorsoM001`. These name records under `records/items/enemygear/` and equivalent monster-gear paths, carrying meshes such as `creatures/enemies/groble/equipment/groble_shamanstaff01.msh`. `m01_groblestaff001.dbr` is classified `Broken`; `m01_torso001.dbr` has no `itemClassification` field at all and sets `cannotPickUp,1`. Either way the tag has no modeled rarity and stays uncolored.

- **Modeling the `Broken` tier would be a net loss.** Full Rainbow colors `tagWeaponArcaneA001/3/5/7` but leaves `A002/4/6` plain, and all seven are the same kind of enemygear record, so treating `Broken` as colorable would fix four lines and break three in that family alone, before counting the rest of the game. These are items the player cannot obtain, so the names never reach an inventory tooltip. Note that the pre-fork Rust's claim in `src/keywords.rs` — that "only 2 tags in the game have a Broken record and neither is ever colored" — is not accurate; there are more, and Full Rainbow colors several. The behavior of ignoring `Broken` is still correct, but for the reason given here rather than the one stated there.

- **The `Empowered` / `Mythical` unique styles cannot be separated from `Polarized`.** An implementation coloring a style word by the tier of the bases it reaches was built and measured. It colored `tagStyleUniqueTier2` `{^A}` and `tagStyleUniqueTier3` `{^P}` correctly, but also colored `tagStyleUniqueInverted` (`Polarized`), which Full Rainbow leaves plain. `Polarized` is a unique style on non-faction gear reaching Legendary bases — identical to `Mythical` in every field gdse reads. Two correct lines were not worth one visibly wrong one, so the category was dropped.

- **A tag's records can disagree with each other.** One `itemNameTag` is shared by an item and its upgrade tiers, and those tiers are separate records that can differ. `tagLegsC005` ("Soiled Trousers") has four: a level-18 Epic base, a level-75 Empowered tier, an upgraded tier, and a level-94 awakened tier added in GDX3 — and only the awakened one carries an `itemSetName`. Any per-tag property derived from records must therefore be resolved across all of them rather than latched from the first hit; set membership uses a strict majority for this reason. Expansion databases matter here: a base-game-only search for `tagLegsC005` misses the record that mattered.

## Residual risks and follow-up

- Compare generated output byte-for-byte with the pre-fork Rust executable. The C readers have now been exercised against real version-3 game data (see *Full Rainbow parity*), but the two implementations have never been diffed against each other on the same installation.
- Decide whether Monster Infrequent coloring is worth an inference pass over creature and loot-table records; it is the only remaining Full Rainbow category that is derivable at all, and the largest single block of remaining differences.
- Exercise publication rollback with injected rename/write failures on Linux and Windows.
- Validate non-English archives and invalid-byte behavior.
- Confirm archive record identifiers' documented contract upstream; containment is enforced defensively regardless.
- Run the cross-compiled binary and tests on native Windows or Wine; this checkout validates MinGW32 compilation but has no Windows runtime.
