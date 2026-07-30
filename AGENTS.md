# Repository operating guide

## Toolchain and commands

- Rust 2024 edition; the audit baseline used `rustc 1.95.0` and `cargo 1.95.0`. No pinned toolchain file exists.
- Build: `cargo build` or `cargo build --release`.
- Run: `GRIM_DAWN_INSTALL_PATH=/path/to/game cargo run --release -- [--language en] [--out PATH] [--rainbow-filter-damage-colors]`.
- Test: `cargo test --all-targets` (currently no tests).
- Format: `cargo fmt --all` (check with `cargo fmt --all -- --check`).
- Lint: `cargo clippy --all-targets --all-features -- -D warnings` (known baseline failures are recorded in `docs/AUDIT.md`).
- There is no separate type-check, code-generation, packaging, or deployment command.

## Architecture and map

`src/main.rs` parses the CLI, opens Grim Dawn databases through `src/db.rs`, and invokes `src/colorize.rs`. `src/infer.rs` derives item/affix metadata; `src/color.rs` maps that metadata to rarity colors; `src/property.rs` recognizes damage labels; and `src/palette.rs` owns color codes. See `docs/ARCHITECTURE.md` for flows and boundaries and `docs/AUDIT.md` for the remediation backlog.

## Conventions and footguns

- `GRIM_DAWN_INSTALL_PATH` must resolve to an installed Grim Dawn tree containing at least one `.arz` database. End-to-end operation additionally needs proprietary game archives unavailable in a normal checkout.
- The default output is inside the game installation at `settings/text_<language>`; use `--out` while developing.
- `Cargo.lock` pins the Git dependency `lib_gddb`; network access may be needed on a cold build.
- `target/` is generated and ignored. No source files are documented as generated.
- Preserve source localization lines and their CRLF/LF endings when changing rewriting logic.
- Do not silently broaden the supported rarity or property rules: comments in `color.rs`, `infer.rs`, and `property.rs` document product decisions that require game-data/domain validation.
