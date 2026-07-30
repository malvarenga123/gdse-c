# Repository operating guide

## Toolchain and commands

- Application sources are ISO C89. A POSIX-like `make` and C compiler are required.
- Build: `make` (override `CC` and `CFLAGS` conventionally).
- Test: `make test`; full compile-and-test check: `make check`.
- Clean: `make clean`.
- Run: `GRIM_DAWN_INSTALL_PATH=/path/to/game ./gdse [--language en] [--out PATH] [--rainbow-filter-damage-colors]`.
- Project sources compile with `-std=c89 -pedantic -Wall -Wextra -Werror`. Vendored upstream sources compile with their supported dialect and are isolated behind project-owned interfaces.

## Architecture and map

`src/main.c` owns CLI parsing, input policy, staging, manifests, and publication. `src/archive.c` streams Grim Dawn ARZ/ARC data. `src/rules.c` infers item metadata and applies rarity/property colors. `src/util.c` contains allocation, little-endian I/O, paths, and filesystem helpers. `src/gdse.h` is the internal interface. See `docs/ARCHITECTURE.md` and `docs/AUDIT.md`.

## Conventions and footguns

- `GRIM_DAWN_INSTALL_PATH` must be an existing Grim Dawn tree. The base ARZ and requested base language ARC are mandatory; nonexistent DLC inputs are optional, but an existing unreadable/corrupt input is fatal.
- Default output is `settings/text_<language>`; use `--out` while developing.
- Preserve source localization content and CRLF/LF/no-final-newline behavior in rewriting code.
- Output ownership is limited to paths in `.gdse-manifest`. Never broaden deletion beyond that manifest.
- Archive record paths must pass `gd_safe_record_path` before filesystem use.
- Do not broaden rarity/property rules without game-data/domain validation.
- `vendor/lz4` and `vendor/utf8proc` are third-party source with adjacent licenses; avoid modifying them except for a deliberate vendor update.
- Proprietary game archives are unavailable in a normal checkout, so real end-to-end validation remains manual.
