# Repository onboarding audit and remediation backlog

## Audit record

- **Audit date:** 2026-07-30
- **Branch:** `work`
- **Base branch:** Unidentified; the branch has no upstream, no `origin` remote is configured, and no remote default branch exists.
- **Commit examined:** `b0a9042cbba84d3681cb993b3015e645c17ff4f3`
- **Initial repository state:** Clean working tree; one worktree; no submodules; no pre-existing repository instruction file.
- **Synchronization:** `git fetch --prune origin` failed before investigation because `origin` is not configured. No rebase, merge, push, or history rewrite was attempted.
- **Scope examined:** Every tracked Rust source file, `Cargo.toml`, relevant `Cargo.lock` entries, `.gitignore`, README, recent Git history, branches, and the build/test/lint surface.
- **Sampled:** Dependency behavior was inferred only through gdse's calls and a successful compile; `lib_gddb` source was not audited. Git history was sampled to ten commits.
- **Not examined:** Proprietary Grim Dawn databases/text archives; real end-to-end output; Windows or non-English behavior; performance profiles; malformed/adversarial archives; upstream repository state; dependency internals and transitive security advisories.
- **Generalization limit:** Findings describe this small CLI at the examined commit. Runtime claims requiring actual game data are explicitly inferred or unverified.

## Baseline validation

| Command | Result |
| --- | --- |
| `cargo fmt --all -- --check` | Exit 0. |
| `cargo test --all-targets` | Exit 0; 0 passed, 0 failed, 0 ignored. This compiles the project but exercises no behavior. |
| `cargo clippy --all-targets --all-features -- -D warnings` | Exit 101; five style diagnostics (`collapsible_if` and `while_let_on_iterator`) in `colorize.rs`, `db.rs`, and `infer.rs`. |
| `cargo build --release` | Exit 0. |
| `rustc --version`; `cargo --version` | `rustc 1.95.0`; `cargo 1.95.0`. The repository does not pin or declare a minimum version. |

Network was available to obtain the Git dependency during this run. End-to-end execution could not be validated because no licensed Grim Dawn installation/data was supplied. The clippy failure and absence of tests are baseline conditions, not introduced by audit documentation.

## Architecture summary

gdse is a sequential Rust CLI. `main` parses arguments and composes database loading with `colorize::run`. Database item records flow through `infer::infer` and `color::color_for` to a tag-color map; language archive records flow through `recolor_file`, with additional name-derived rules from `property::color_for`, and changed files are written to the local filesystem. There are no network calls at runtime, persistence services, authorization, transactions, retries, or observability beyond stdout/stderr. See `docs/ARCHITECTURE.md` for the detailed map and invariants.

## Confirmed findings

## AUD-001 — Core transformation and inference behavior has no automated tests

- **Status:** Open
- **Classification:** Observed
- **Severity:** High
- **Impact area:** Correctness, Testing, Maintainability
- **Evidence:**
  - `src/colorize.rs`: private `recolor_file`, `apply_color`, `replace_color_codes`, `insert_after_brackets`, and `insert_after_bar_digit` (lines 97–262).
  - `src/infer.rs`: `infer`, `accumulate_item`, and `mode_rarity` (lines 65–203).
  - `src/property.rs`: `color_for` (lines 63–106).
  - `cargo test --all-targets`: “running 0 tests”.
  - Examined at commit `b0a9042cbba84d3681cb993b3015e645c17ff4f3`.
- **Description:** The text transformation contains multiple ordering-sensitive branches, while inference encodes product-specific rarity and affixability rules; none have unit, integration, fixture, or end-to-end coverage.
- **Impact:** Regressions in color placement, line-ending preservation, tag selection, rarity ties, exclusions, or localization handling can compile and ship undetected. Real game data is then the only effective validation.
- **Recommended remediation:** First add table-driven unit tests beside the pure color/property and text-rewriting functions. Add small synthetic `lib_gddb` record fixtures for inference if its public constructors permit it; otherwise isolate record classification behind a small pure input model. Add an opt-in integration fixture only if legally redistributable.
- **Blast radius:** Tests initially; small visibility/refactoring changes may later affect `colorize`, `infer`, `color`, and `property` internals.
- **Risks:** Tests may accidentally codify unintended behavior; expected cases require domain review. Do not vendor proprietary game assets.
- **Validation requirements:** Cover every `apply_color` branch, LF/CRLF/no-final-newline preservation, conversion closing codes, property exclusions/palette option, rarity tie behavior, faction and name-part rules. Run `cargo test --all-targets` and the existing release build.
- **Dependencies:** None. This should precede AUD-002 and AUD-003 remediation.
- **Rough effort:** Medium.
- **Resolution notes:** Not yet addressed.

## AUD-002 — Input archive and database failures are silently treated as optional content

- **Status:** Open
- **Classification:** Observed
- **Severity:** High
- **Impact area:** Correctness, Reliability, Operability
- **Evidence:**
  - `src/db.rs::open_all` uses `Database::open(...).ok()` for every database and reports failure only when all four fail (lines 31–42).
  - `src/db.rs::iter_records` continues when `record_id` or `resolve` fails (lines 49–69), while iterator construction/collection instead panics through `expect`.
  - `src/colorize.rs::run` continues when `Archive::open` or `iter_records` fails and flattens per-record errors (lines 38–84).
  - Examined at commit `b0a9042cbba84d3681cb993b3015e645c17ff4f3`.
- **Description:** Missing optional expansions and corrupt, permission-denied, unsupported, or partially unreadable inputs collapse to the same control path. Text processing can exit successfully after skipping an expected archive, record, or expansion database, while other failures panic.
- **Impact:** Users may receive incomplete or stale output with no diagnostic and believe generation succeeded. Inconsistent panic/skip/exit behavior makes failures difficult to diagnose or automate.
- **Recommended remediation:** After AUD-001 characterization, return a typed/reportable error from database and colorization orchestration. Explicitly distinguish absent optional expansion paths from failures opening paths that exist; accumulate contextual record errors and fail before publishing output. Keep a concise CLI error and nonzero status.
- **Blast radius:** `db`, `colorize`, and `main`; CLI exit behavior and diagnostics. No file-format change is required.
- **Risks:** Previously tolerated damaged installations will begin failing; exact optional-expansion policy needs domain confirmation.
- **Validation requirements:** Tests for missing optional files, unreadable/corrupt existing files, per-record failures where injectable, all-inputs-missing, and successful partial installations. Validate exit codes and ensure no output publication after fatal input errors.
- **Dependencies:** AUD-001; domain confirmation of optional base/expansion resources.
- **Rough effort:** Medium.
- **Resolution notes:** Not yet addressed.

## AUD-003 — Output publication is non-atomic and does not reconcile stale files

- **Status:** Open
- **Classification:** Observed
- **Severity:** Medium
- **Impact area:** Correctness, Reliability, Operability
- **Evidence:**
  - `src/colorize.rs::run` creates the destination tree and directly calls `std::fs::write` once per changed archive record (lines 46–83).
  - Files with zero changes are skipped (lines 66–69); there is no manifest, cleanup, temporary tree, rename, rollback, or collision handling.
  - README “Installation & Usage” directs repeated writes to `$GRIM_DAWN_INSTALL_PATH/settings/` after patches.
  - Examined at commit `b0a9042cbba84d3681cb993b3015e645c17ff4f3`.
- **Description:** Generation mutates the live output incrementally. A late failure leaves a mixture of old and new files. Files produced by an earlier run remain when a later run no longer emits them. Multiple archive records resolving to the same destination overwrite in processing order without an explicit policy.
- **Impact:** Interrupted or failed runs can leave a partially updated mod; game patches or language/selection changes can leave obsolete overrides active. The practical frequency and archive collision behavior are unverified without game data.
- **Recommended remediation:** After characterization and explicit error handling, build a complete manifest in a sibling temporary directory, validate paths/collisions, then publish using the safest platform-appropriate rename/swap strategy. Remove only files owned by gdse, using a manifest rather than deleting arbitrary user content. A smaller first step is a dry-run manifest plus collision/stale-file diagnostics.
- **Blast radius:** Output orchestration and filesystem layout; existing user output directories and cross-platform rename semantics.
- **Risks:** Cleanup could delete user files if ownership is not strictly tracked; directory replacement differs on Windows; crashes can leave temporary directories.
- **Validation requirements:** Filesystem tests for mid-run failure, repeated runs with a removed output, collisions, unrelated files, cleanup recovery, and Windows-compatible publication. Manual game smoke test.
- **Dependencies:** AUD-001 and AUD-002; decide output ownership/compatibility policy.
- **Rough effort:** Large for full atomic publication; Small for manifest diagnostics.
- **Resolution notes:** Not yet addressed.

## AUD-004 — Record filtering eagerly collects every raw database record

- **Status:** Open
- **Classification:** Inferred
- **Severity:** Medium
- **Impact area:** Performance, Reliability, Maintainability
- **Evidence:**
  - `src/db.rs::iter_records` collects `db.iter_records()` into `Vec<RawRecord>` before checking each record identifier and resolving retained records (lines 45–69).
  - Its documentation says filtering the cheap record ID first avoids decompressing unwanted records, but does not mention that all raw metadata is retained simultaneously (lines 45–48).
  - `src/infer.rs::infer` then retains all resolved item records in another vector via `db::iter_records` (lines 65–79).
  - Examined at commit `b0a9042cbba84d3681cb993b3015e645c17ff4f3`.
- **Description:** Memory use appears to scale with all raw records plus all resolved item records rather than the working set needed for one-pass inference. This is a code-path observation, but no heap profile or representative database measurement establishes that it is a user-visible bottleneck.
- **Impact:** Large or future databases may cause avoidable peak memory and delayed first processing. Severity is provisional pending measurement.
- **Recommended remediation:** First benchmark wall time and peak RSS with representative base/all-expansion data and inspect whether `lib_gddb` borrowing rules require collection. If material, stream raw records and fold resolved item data directly into inference state, in small independently tested steps.
- **Blast radius:** Database iteration and inference ownership/lifetimes; no intended user-visible behavior change.
- **Risks:** Borrowing constraints may make streaming complex; optimization without measurements could reduce clarity or alter error handling.
- **Validation requirements:** Representative peak-RSS/time measurements before and after, identical inferred-map comparison, and tests from AUD-001. Profile raw count, retained item count, and resolved payload sizes.
- **Dependencies:** AUD-001; access to representative licensed game data; inspection of `lib_gddb` iterator/borrowing contracts.
- **Rough effort:** Small investigation; Medium implementation if supported.
- **Resolution notes:** Not yet addressed.

## Suspected or unverified concerns

- **Archive path containment:** `colorize::run` joins `record.id` directly beneath the output path. It is unverified whether `lib_gddb` guarantees normalized relative identifiers. Confirm the dependency contract and test containment before classifying this as a security finding; game archives are normally trusted local assets.
- **Lossy decoding:** `String::from_utf8_lossy` can replace invalid bytes. Whether Grim Dawn localization archives guarantee UTF-8, and whether any supported locale uses other encodings, was not verified.
- **Cross-platform support:** README states only Linux/English were tested. Path casing, filesystem replacement semantics, and localized archive layout need Windows and non-English fixtures before broader support claims.

## Prioritized incremental remediation strategy

1. **AUD-001:** Add pure, table-driven characterization coverage without changing runtime behavior. This is reversible and protects every later step.
2. **AUD-002:** Introduce contextual error propagation and distinguish missing optional content from broken existing content. Preserve successful-install behavior; document deliberate CLI exit changes.
3. **AUD-003 (diagnostic slice):** Build/validate an output manifest, reject path escape/collisions, and report stale owned files before attempting atomic publication.
4. **AUD-003 (publication slice):** Add explicitly owned staging/manifest semantics and platform-tested publication/rollback.
5. **AUD-004:** Measure representative data first; stream only if profiling justifies the added lifetime/iterator complexity.

No remediation is approved or implemented by this audit. Rollback for each proposal is the focused commit reverting that independently shippable slice. Findings that change behavior require a follow-up plan approval, narrow tests first, and updates to their status/resolution notes.

## Recommended follow-up questions

- Which databases and language archives are truly optional for each supported installation/DLC combination?
- May gdse exclusively own `settings/text_<language>`, or must it coexist with user files despite documented mod incompatibility?
- What minimum Rust version and supported OS/locales are intended?
- Can sanitized synthetic `.arz`/`.arc` fixtures be generated and redistributed?
- Does `lib_gddb` guarantee safe relative archive record IDs and expose streaming iteration compatible with record resolution?
