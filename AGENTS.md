# Repository operating guide

## Toolchain and commands

- Application sources are ISO C89. A POSIX-like `make` and C compiler are required.
- Build: `make` (override `CC` and `CFLAGS` conventionally).
- Test: `make test`; full compile-and-test check: `make check`.
- Clean: `make clean`.
- MSYS2 MinGW build/test: `make` and `make test`; recipes follow the active POSIX shell even though `OS=Windows_NT`.
- MinGW32 build/test: `mingw32-make` and `mingw32-make test` (produces `gdse.exe`). Cross-compile check: `make clean && make CC=i686-w64-mingw32-gcc EXE=.exe all`.
- Run from the game directory: `./gdse [--language LANG] [--out PATH] [--no-rainbow-filter-damage-colors]`; alternatively pass `/path/to/game` as the optional first positional argument. The language defaults to `en`, and Rainbow Filter damage colors default to enabled.
- Project sources compile with `-std=c89 -pedantic -Wall -Wextra -Werror`. Vendored upstream sources compile with their supported dialect and are isolated behind project-owned interfaces.

## Architecture and map

`src/main.c` owns CLI parsing, input policy, staging, manifests, and publication. `src/archive.c` streams Grim Dawn ARZ/ARC data. `src/rules.c` infers item metadata and applies rarity/property colors. `src/util.c` contains allocation, little-endian I/O, paths, and filesystem helpers. `src/gdse.h` is the internal interface. See `docs/ARCHITECTURE.md` and `docs/AUDIT.md`.

## Changelog

- Root-level `CHANGELOG.md` is the canonical changelog and follows Keep a Changelog 1.1.0.
- Add consumer- or operator-visible additions, changes, deprecations, removals, fixes, and security updates under the applicable heading in `[Unreleased]`.
- Purely internal refactoring, tests, and documentation changes ordinarily do not require changelog entries unless they have an externally observable effect.
- There is no legacy or generated changelog and no synchronization or generation command is required.
- Do not maintain another changelog as an independent authority; future manually maintained entries belong in root-level `CHANGELOG.md`.

## Conventions and footguns

- The optional `GRIM_DAWN_INSTALL_PATH` positional argument must be an existing Grim Dawn tree; when omitted, gdse uses the current directory. The base ARZ and requested base language ARC are mandatory; nonexistent DLC inputs are optional, but an existing unreadable/corrupt input is fatal.
- Default output is `settings/text_<language>`; use `--out` while developing.
- Preserve source localization content, normalize existing line separators to CRLF, and preserve a missing final newline in rewriting code.
- Output ownership is limited to paths in `.gdse-manifest`. Never broaden deletion beyond that manifest.
- Archive record paths must pass `gd_safe_record_path` before filesystem use.
- Do not broaden rarity/property rules without game-data/domain validation.
- `vendor/lz4` and `vendor/utf8proc` are third-party source with adjacent licenses; avoid modifying them except for a deliberate vendor update.
- Proprietary game archives are unavailable in a normal checkout, so real end-to-end validation remains manual.
