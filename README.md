# venue_core

An exchange-side matching engine.

A C++23 library, a command-line executable and Catch2 tests, built with CMake
presets and developed and packaged with a Nix flake, with room for Python
helper scripts. `nix flake check` runs the same build, tests, formatting and
lint checks that CI runs.

## Quick start

```bash
direnv allow                    # or: nix develop
cmake --workflow --preset dev   # configure + build + test
```

**[WORKFLOW.md](WORKFLOW.md) is the full command reference**, in the order you
use the commands: machine setup, starting a new project from the template,
opening an existing project, the daily build/test/debug loop, checks before
committing, dependencies and updates.

## Layout

```
.
├── flake.nix                 # inputs + outputs; the logic lives in nix/
├── flake.lock                # pinned nixpkgs (nixos-unstable) and treefmt-nix
├── nix/
│   ├── package.nix           # the derivation, nixpkgs-style (callPackage)
│   ├── project.nix           # name, version, description, read from CMakeLists.txt
│   ├── shell.nix             # dev shell = package deps + clangd, formatters, Python, gdb
│   └── treefmt.nix           # `nix fmt`: clang-format, gersemi, ruff, nixfmt
├── CMakeLists.txt            # project(VERSION), subdirectories, install rules
├── CMakePresets.json         # dev / asan / tidy / release presets + workflows
├── cmake/                    # warning flags, install/export rules, package config
├── include/venue_core/        # public headers (installed, FILE_SET HEADERS)
├── src/                      # library sources and private headers
├── apps/                     # executables
├── tests/                    # Catch2 tests, each TEST_CASE registered with CTest
├── ruff.toml                 # Python lint + format rules (scripts/ is created as needed)
├── WORKFLOW.md               # every command, in the order you use them
├── .envrc                    # direnv: `use flake`
├── .clangd                   # clangd reads build/dev/compile_commands.json
├── .clang-format  .clang-tidy  .editorconfig
└── .github/                  # CI (`nix flake check` + ASan run), Dependabot
```

The layout follows the Pitchfork Layout with separate header placement
(`include/` public, `src/` private) and `apps/` for executables. Every preset
configures into `build/<preset>`, so Debug, sanitizer and release trees never
collide.

## Design notes

- **One source of truth for each fact.** Dependencies live in
  `nix/package.nix`; the dev shell inherits them through `inputsFrom`. The
  name, version and description live in `project(...)` in `CMakeLists.txt`,
  and `nix/project.nix` reads them from there, so no Nix file names the project
  (only `flake.nix`'s own `description`, which Nix requires to be a literal).
- **The nixpkgs CMake hook does the work.** No hand-written phases: it passes
  the compiler, `-DCMAKE_BUILD_TYPE=Release` and `-GNinja`, runs `ctest` when
  `doCheck` is set (passing `-DBUILD_TESTING=OFF` otherwise), then `install`.
- **`lib.fileset` source filtering.** Editing docs, scripts or Nix files does
  not rebuild the package.
- **Dev flags never leak.** Warnings are attached `PRIVATE` to each target
  (`venue_core_enable_warnings`). `-Werror` comes from CMake's built-in
  `CMAKE_COMPILE_WARNING_AS_ERROR`, which the presets and the Nix package turn
  on. Sanitizers exist only in the `asan` preset.
- **Two compilers, and lint that can fail.** `nix flake check` builds and
  tests with the default stdenv (GCC on Linux) and with `clangStdenv`, and runs
  clang-tidy over every file with warnings as errors. CI also runs the dev
  workflow under ASan and UBSan.
- **Automatic updates.** nixpkgs follows `nixos-unstable` (which
  cache.nixos.org builds), and Dependabot opens a monthly PR bumping
  `flake.lock` and the GitHub Actions, which CI then verifies.
- **No compiler-wrapper hardening in the dev shell.** nixpkgs' compiler
  wrapper silently adds hardening flags that CMake and `compile_commands.json`
  never show: `-fno-strict-overflow` makes signed overflow wrap, so UBSan can't
  catch it; fortify's `-O2` optimises Debug builds; register clearing, stack
  protector and stack-clash probes add instructions to every function. Dev
  builds therefore compile with exactly CMake's flags. The Nix package keeps
  nixpkgs' full hardening, because it is what you ship.
- **gersemi formats CMake.** It is maintained and understands current CMake
  syntax; cmake-format has not had a release since 2020.
- **C++20 module scanning is off.** The project does not use modules, and
  scanning injects GCC-only flags that clangd and clang-tidy reject.
- **No `shellHook`.** Your own shell and prompt stay exactly as configured.
