# Repository audit and remediation backlog

## Audit record

- **Original audit:** 2026-07-30 at Rust commit `b0a9042cbba84d3681cb993b3015e645c17ff4f3`.
- **C89 remediation:** branch `c89-port`, based on `0e8706c0bb453767c585fe9f9596f4836f839ee4`.
- **Synchronization limitation:** no `origin`, upstream, or remote default branch is configured. Fetch/rebase and upstream comparison remain impossible.
- **Scope:** The Rust/Cargo implementation was replaced by an ISO C89 application, focused tests, Make build, vendored LZ4/utf8proc, and updated durable guidance.
- **Unvalidated:** Proprietary Grim Dawn data, real end-to-end output, Windows, non-English archives, crash injection, and representative performance measurements.

## Remediation validation

| Command | Result |
| --- | --- |
| `make clean && make check` | Exit 0; all project sources compile with strict C89 diagnostics and all focused tests pass. |
| `./gdse --help` | Exit 0; documents the supported CLI surface. |
| `./gdse --version` | Exit 0; reports `gdse 0.1.0-c89`. |
| `./gdse` | Exit 2 as designed; the mandatory install-path argument is diagnosed. |
| `./gdse /nonexistent` | Exit 1 as designed; invalid install paths are diagnosed. |
| `make clean && make CC=i686-w64-mingw32-gcc EXE=.exe all` | Exit 0; produces a 32-bit Windows executable with strict C89 diagnostics on project sources. |

## AUD-001 — Core transformation and inference behavior has no automated tests

- **Status:** Resolved
- **Classification:** Observed
- **Original severity:** High
- **Resolution:** `tests/test_rules.c` adds table-driven coverage for all color-placement branches, replacement/placeholder ordering, property exclusions and palette selection, Unicode letters, CRLF and no-final-newline preservation, conversion behavior, rarity ties, affixability, and name-part coloring. The Make `check` target compiles and runs the suite.
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

## Residual risks and follow-up

- Confirm the C readers against real version-3 game data and compare generated output byte-for-byte with the former Rust executable.
- Exercise publication rollback with injected rename/write failures on Linux and Windows.
- Validate non-English archives and invalid-byte behavior.
- Confirm archive record identifiers' documented contract upstream; containment is enforced defensively regardless.
- Run the cross-compiled binary and tests on native Windows or Wine; this checkout validates MinGW32 compilation but has no Windows runtime.
