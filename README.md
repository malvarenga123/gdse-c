# gdse

gdse is a fully automatic, opinionated alternative to the Rainbow Filter mod for Grim Dawn. It infers Common, Magical, and Rare item/affix colors from game databases and colors damage-type labels in localization text. Pierce is pink by default; pass `--rainbow-filter-damage-colors` to use Rainbow Filter's red.

## Build

A C compiler and POSIX-like `make` are required:

```sh
make
make test
```

The application is written to ISO C89. LZ4 and utf8proc are vendored under `vendor/` so builds do not download dependencies. Their licenses are included beside their sources.

### Windows / MinGW32

From an MSYS2 MinGW shell, use its POSIX `make` normally:

```sh
make
make test
```

With native `mingw32-make` available on `PATH`, use:

```bat
mingw32-make
mingw32-make test
```

The build produces `gdse.exe`. Windows-specific filesystem and 64-bit seek calls are selected behind `_WIN32`; project code does not include POSIX headers on that path. The i686 MinGW-w64 cross compiler is also exercised with strict C89 diagnostics during development.

## Usage

```sh
./gdse /path/to/Grim\ Dawn \
  [--language en] [--out PATH] [--rainbow-filter-damage-colors]
```

`GRIM_DAWN_INSTALL_PATH` is the mandatory first positional argument and must name the game installation. The base database and requested base language archive are required. DLC files are optional when absent, but gdse fails rather than silently ignoring an existing unreadable or corrupt file. The default destination is `GRIM_DAWN_INSTALL_PATH/settings/text_<language>`.

gdse stages all generated files before publication, rejects unsafe/colliding archive paths, and records owned output in `.gdse-manifest`. Later runs remove only stale files named by that manifest, leaving unrelated files alone. Existing generated files are backed up during publication and restored if publication fails.

Re-run gdse after game patches. It has only been verified against the repository's synthetic tests; a licensed Grim Dawn installation is needed for an end-to-end smoke test. The output is not compatible with other mods that rewrite the same game text.

Generated localization files and the ownership manifest use Windows CRLF line endings. An input file without a final newline remains unterminated after rewriting.
