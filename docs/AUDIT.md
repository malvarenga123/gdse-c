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

**Evidence provenance:** the numerical results below come from that recorded
licensed-installation run, not from the repository's synthetic fixtures. A
normal checkout contains neither Grim Dawn's proprietary archives nor Full
Rainbow's distributed localization file, so a later source review can verify
that the implementation follows the documented inference rules, but cannot
independently regenerate the output. The two captured `tags_items.txt` files and
plain-text record exports are now under `records/`, so source-level rules can be
checked against the captured comparison; full revalidation still requires
running the generator against the proprietary archives.

### Measurement

| Build | Differing lines |
| --- | --- |
| gdse's own scheme | 1,224 |
| `--full-rainbow` | 273 |

No line differs in text. Every difference in either run is a color marker or Full Rainbow's `(S) ` set marker; no tag is added, dropped, reworded, or reordered. All 209 set names Full Rainbow marks are reproduced exactly.

### Classification of the remaining 273 lines

| Cause | Lines | Actionable |
| --- | --- | --- |
| Monster Infrequent coloring | 148 | Yes — implemented after this measurement; 133 of them now colored |
| Tags absent from Full Rainbow's list | 103 | No — gdse colors ordinary gear it has no entry for |
| Tags with no item record at all | 15 | No |
| Enemy-only gear | 5 | No |
| `Empowered` / `Mythical` unique styles | 2 | No |

### Findings

- **Monster Infrequents are distinguishable, but not from `records/items/` alone.** *(Superseded — see the note below.)* Full Rainbow paints them `{^L}`, and `{^Z}`/`{^F}` at Epic/Legendary tier. The distinction tracks whether a named creature drops the item, which lives in creature and loot-table records outside the `records/items/` scope gdse reads. Base rarity does not separate them: `Bloodsworn Repeater` is `{^L}` while `Hand Mortar`, `Shrapnel Pistol` and `Francis' Gun` are `{^G}`, and gdse classifies all four identically as Rare bases.

- **Tags with no item record at all account for 15 lines.** Confirmed absent from the database: `tagHeadA010`, `tagShieldA011`, `tagQualityWeaponWood06` through `11`, `tagQuestItemSlithRing`, `tagShoulderF005`, `tagShoulderF010`, `tagTorsoF005`, `tagTorsoF010`. `tagQuestItemBrothersAmulet` and `tagItemTest` are presumed the same but were not separately confirmed. Full Rainbow colors text the game never displays; inference has nothing to work from. `tagQualityWeaponWood05`, which does have six records, is colored correctly, so the mechanism is sound.

- **Enemy-only gear accounts for 5 lines** — `tagWeaponArcaneA001/A003/A005/A007` and `tagTorsoM001`. These name records under `records/items/enemygear/` and equivalent monster-gear paths, carrying meshes such as `creatures/enemies/groble/equipment/groble_shamanstaff01.msh`. `m01_groblestaff001.dbr` is classified `Broken`; `m01_torso001.dbr` has no `itemClassification` field at all and sets `cannotPickUp,1`. Either way the tag has no modeled rarity and stays uncolored.

- **Modeling the `Broken` tier would be a net loss.** Full Rainbow colors `tagWeaponArcaneA001/3/5/7` but leaves `A002/4/6` plain, and all seven are the same kind of enemygear record, so treating `Broken` as colorable would fix four lines and break three in that family alone, before counting the rest of the game. These are items the player cannot obtain, so the names never reach an inventory tooltip. Note that the pre-fork Rust's claim in `src/keywords.rs` — that "only 2 tags in the game have a Broken record and neither is ever colored" — is not accurate; there are more, and Full Rainbow colors several. The behavior of ignoring `Broken` is still correct, but for the reason given here rather than the one stated there.

- **The `Empowered` / `Mythical` unique styles cannot be separated from `Polarized`.** An implementation coloring a style word by the tier of the bases it reaches was built and measured. It colored `tagStyleUniqueTier2` `{^A}` and `tagStyleUniqueTier3` `{^P}` correctly, but also colored `tagStyleUniqueInverted` (`Polarized`), which Full Rainbow leaves plain. `Polarized` is a unique style on non-faction gear reaching Legendary bases — identical to `Mythical` in every field gdse reads. Two correct lines were not worth one visibly wrong one, so the category was dropped.

- **A tag's records can disagree with each other.** One `itemNameTag` is shared by an item and its upgrade tiers, and those tiers are separate records that can differ. `tagLegsC005` ("Soiled Trousers") has four: a level-18 Epic base, a level-75 Empowered tier, an upgraded tier, and a level-94 awakened tier added in GDX3 — and only the awakened one carries an `itemSetName`. Any per-tag property derived from records must therefore be resolved across all of them rather than latched from the first hit; set membership uses a strict majority for this reason. Expansion databases matter here: a base-game-only search for `tagLegsC005` misses the record that mattered.

### Monster Infrequent inference (2026-07-31, after the measurement above)

The 148 Monster Infrequent lines were subsequently found to be derivable, and `--full-rainbow` now colors them. A monster record names loot tables in its `loot*Item*` fields; excluding the shared `loottables/mastertables/` pools leaves the tables attached to that monster in particular, and their Rare-and-above contents are Monster Infrequents.

Two wrong cuts were measured before that shape settled, and both are worth not repeating:

| Rule | Differing lines | False positives |
| --- | --- | --- |
| `lootMisc<N>Item<M>` only | 205 | 3 |
| any `loot*Item*` | 259 | 117 |
| any `loot*Item*`, Rare and above | 149 | 7 |
| plus boss-owned mastertables | **148** | 7 |
| plus `LevelTable` arrays | 377 | 246 |
| plus `LevelTable` arrays, boss chains only | 377 | 246 |

- **Slot name carries no information.** The first cut assumed `lootMisc<N>Item<M>` held a monster's own drops while `loot<Slot>Item<M>` held the gear it wields. That recovered only 71 of 149 lines, and the missed half was almost entirely wearable. The troll that drops Gollus' Ring names it in `lootFinger1Item1` with nothing but master tables in its misc slots — the exact mirror of the yeti, whose Infrequent is in `lootMisc3Item1`. Excluding mastertables was doing all the discriminating work by itself.
- **Rarity is the discriminator the slot name is not.** Widening to every `loot*Item*` field made the total worse, not better: 117 false positives, of which 110 were Common items painted olive — `Sabre`, `Gladius`, `Club`, `Mace`, `Tower Shield`, `Pauldrons`, `Shotgun`. A monster's loot slots hold both its Infrequent and the plain gear it wields. Full Rainbow's `{^L}`, `{^Z}` and `{^F}` only ever land on Rare, Epic and Legendary, so gating the mark on Rare-and-above separates the two without any per-item knowledge.

Confirmed against real records: Yeti Horn, Gollus' Ring and Gutworm's Mark each resolve through a monster's drop slot, while Honed Longsword and Battle Shield reach only crafting blueprints and Francis' Gun only a lore-chest table. The near-miss worth remembering is the Sabre, an ordinary white base reachable from the Necromancer's summoned skeleton — pets are `Class,Pet`, carry `dropItems,0`, and live under `records/skills/`, so they never enter a scan scoped to `records/creatures/`. Values naming an item record rather than a table, such as the troll's `craft_ancientheart` reference, land as an empty table and mark nothing.

- **`mastertables/` is the shared pooling layer, but not uniformly.** A few records filed there are one boss's own table: `mt_gearweaponsmelee2h_d02_alkamos` sits beside `mt_accessories_rings_d01`, same directory and same tier letter, and nothing in the name or the family separates them. Counting the distinct creature records that name each one does. Measured on version 1.3.0 the populations do not come close to touching — `mt_accessories_rings_a01` and `_d01` at 50 creatures each, `mt_beast_large_a01` at 82, `mt_gearweaponsshield` at 90, against 1 for each of Alkamos' two. `GD_SHARED_TABLE_REFS` cuts inside that gap.
- **That count does not generalize outside `mastertables/`.** Replacing the path rule with a pure reference count cost 26 correct lines and gained 2: a monster family has one creature record per variant and difficulty, so dozens of yetis name the single yeti table and it reads exactly like a world pool. The directory carries real information and the count only rescues the exceptions inside it.

This costs about 5,200 extra record decompressions and only when the flag is set.

### Measurement of the shipped rule

The supplied outputs contain **149 differing tags**, down from 273 before the
Monster Infrequent category existed. Still no text-level difference of any
kind. The original residue decomposes exactly:

| Bucket | Lines | Cause |
| --- | --- | --- |
| `- → W` / `- → Y` | 103 | Full Rainbow's hand list has no entry; gdse colors ordinary gear |
| `W → -`, `G → -`, `S → -` | 19 | Tags with no item record, and enemy-only gear |
| `F → I`, `Z → B`, `L → G` | 17 | Monster Infrequents gdse still misses |
| `B → Z`, `G → L` | 7 | Monster Infrequents Full Rainbow does not color |
| `A → -`, `P → -`, `F → -` | 3 | The two unique styles, and `tagItemTest` |

In that table the left side is Full Rainbow and the right side is gdse; `-`
means no explicit color marker. Thus the large `- → W` / `- → Y` block
does not indicate failed rarity inference. It is gdse successfully finding
records which the mod's list does not color. The block is exactly 36
`tagCraftRandom*` names and 67 Loyalist illusion equipment names
(`tagDLCA01` through `tagDLCA43`, plus the populated `tagDLCB*` range). Those
two coherent families account for all 103 lines. Suppressing them would make a
comparison against that release smaller, but would replace the database rule
with knowledge of omissions in one hand-maintained file.

### Can exact parity be inferred without per-item fixes?

No, not from the fields available in the game database. There are two useful
levels of "closer" which should not be confused:

1. A pair of **category-level compatibility rules** now suppress the
   `tagCraftRandom*` and Loyalist illusion families. That needs no list of 103
   individual items and removes 103 lines from the supplied comparison. The
   rules also follow the semantic distinction visible in
   the localization categories: generated crafting-result labels and cosmetic
   unlock equipment are not ordinary dropped item names. The projection still
   needs remeasurement on a licensed installation.
2. **Exact parity** requires exceptions or an external copy of Full Rainbow's
   tag decisions. Some localization tags have no corresponding item record;
   seven real named-monster drops satisfy the same database rule as other MIs
   but are not painted as MIs by Full Rainbow; and two tags for the same helm
   need different colors despite resolving to the same inferred metadata. No
   further traversal or classifier can recover distinctions which are absent
   from its input.

The uploaded records expose the missing discriminator: named families retain a
descriptor after their rarity token in both the `LevelTable` and its dynamic
children (`d02_alkamos`, `c03_sharzul`, `ghostly`), while generic tier wrappers
end at `c01` or `d101`. Following only specialized parent/child paths recovers
15 missing MI colors. It introduces one known tier collision,
`tagHeadC034B`, rather than the 239 false positives caused by following every
`records` array. Together with the two unique-style and two quest-item category
colors, that first implementation projected **28 differing tags** on the
supplied snapshot.

The next generated output confirmed **29 differences**. The one-line delta
from the projection is `Scythe of Tenebris`: it is a genuine specialized boss
drop and therefore satisfies the same generic rule, but Full Rainbow leaves it
at ordinary Epic blue. The remaining uncolored entries reveal five coherent
missing-record families. Numeric-only fallbacks now cover wooden quality words,
base head and shield names, monster torsos, and faction shoulders/torsos without
touching their description tags. Those rules remove 13 more lines, leaving a
projected **16 differences**.

The original 7 false positives are irreducible. `Gutworm's Bloody Seal`, `Razorback's Spined Mantle`, `Bernard's Slightly-Chewed Buckler`, `Leander Greene's Hand Cannon`, `Bloodreaper's Cleaver`, `Reddan Memento Ring` and `Skinner's Torch` are all genuine named-monster drops that Full Rainbow's hand-maintained list happens not to paint. `Scythe of Tenebris` is the same kind of omission exposed by the specialized traversal. Nothing in the database distinguishes them from the Infrequents Full Rainbow does paint.

The tier collision is also irreducible: `tagHeadC034` and `tagHeadC034B` are the same helm at two tiers, Full Rainbow paints them `{^Z}` and `{^B}`, and gdse reads no field that separates them.

### Specialized `LevelTable` traversal

The specialized traversal recovers Shar'Zul (`Furnace`, `Incinerator`,
`Worldeater`), Mogdrogen (`Spaulders`, `Mantle`), Alkamos (`Soulrend`, both
`Touch of` rings), the Mad Queen, `Outcast's Secret`, two Triumvirate helm tags,
and the Rare Spectral weapon family. `Manticore Longsword` has no loot-table
reference in the supplied exports, while Loghorrean's shoulders are reached
through boss-chest records rather than a creature-owned table; those two remain
ordinary Rare colors.

Alkamos' chain, read end to end from the records themselves:

```
ghost_stepsoftorment_03.dbr   lootRightHandItem2  Class,Monster
  mt_gearweaponsmelee2h_d02_alkamos.dbr   lootName1     Class,LootMasterTable
    lt_melee2h_d02_alkamos.dbr            records[2]    Class,LevelTable
      tdyn_melee2h_d02_alkamos.dbr        lootName1/2   Class,LootItemTable_DynWeight
        d012_axe2h.dbr                                  Soulrend
```

A `LevelTable` selects among whole tables by character level and lists them in a
`records` **string array**. Reading every array connects generic tier wrappers
as well as boss chains and produced 377 differences: 184 Epic and 55 Legendary
bases read as somebody's Infrequent. The specialized-path rule instead records
only named parent/child pairs and refuses to expand from a specialized table
into a generic nested table.

### A reader defect found along the way

`parse_fields` kept a string field only when its value count was 1, silently discarding every array-valued string field in the database — for every code path, flag or no flag. It was found while chasing the chain above and is fixed independently: arrays now arrive as one field per element, and `gd_record_field` still answers with the first, so single-valued readers are unaffected. The specialized loot-table scanner now consumes the relevant `records` elements individually.

The lesson worth keeping is about the encoding. A `.dbr` text export writes an array as one semicolon-joined string; the ARZ stores one string index per element. Code written against a text export will not fire on real data, and the first attempt at `records` parsing did exactly that and produced three rounds of unexplained byte-identical results.

## Residual risks and follow-up

- Compare generated output byte-for-byte with the pre-fork Rust executable. The C readers have now been exercised against real version-3 game data (see *Full Rainbow parity*), but the two implementations have never been diffed against each other on the same installation.
- Exercise publication rollback with injected rename/write failures on Linux and Windows.
- Validate non-English archives and invalid-byte behavior.
- Confirm archive record identifiers' documented contract upstream; containment is enforced defensively regardless.
- Run the cross-compiled binary and tests on native Windows or Wine; this checkout validates MinGW32 compilation but has no Windows runtime.
