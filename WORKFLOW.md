# Workflow

Every command for this project, in the order you use them: one-time machine
setup, starting a new project from the template, opening an existing project,
the daily loop, checks before committing, dependencies, occasional tasks, and
troubleshooting.

## 0. One-time machine setup

- **Nix** with flakes enabled (`experimental-features = nix-command flakes`).
- **direnv + [nix-direnv]** load the dev shell when you `cd` into a project.
  nix-direnv caches the evaluated shell (re-evaluated only when `flake.nix`,
  `flake.lock` or `nix/*.nix` change), keeps it as a GC root, and falls back
  to the last working shell if an edit breaks evaluation. With Home Manager
  (NixOS has the same options under `programs.direnv`):

  ```nix
  programs.direnv = {
    enable = true;
    nix-direnv.enable = true;
  };
  ```

  Without direnv, `nix develop` gives the same shell.
- **Editor.** The dev shell puts clangd, clang-tidy and every formatter
  `nix fmt` uses on `PATH`, pinned by `flake.lock`. Start the editor from the
  project directory (direnv loads the shell) and format-on-save produces
  exactly what CI checks. For CMake, configure the editor to use `gersemi`
  (conform.nvim: `cmake = { "gersemi" }`).

[nix-direnv]: https://github.com/nix-community/nix-direnv

## 1. Start a new project from the template

```bash
cd ~/projects
nix run github:rollschild/cpp-nix-template#new -- my_tool   # or the local copy: nix run ~/projects/cpp-nix-template#new -- my_tool
cd my_tool
direnv allow                    # loads the dev shell (the generator already built it, so this is quick)
cmake --workflow --preset dev   # build + test: confirms everything works
```

- The generator creates `./my_tool`. The name must be lower_snake_case
  because it becomes the C++ namespace; anything else is converted into a
  suggestion that Enter accepts. It asks for a one-line description (no `"`,
  `\` or `$`), shows a summary, and on Enter:
  - fills in the name (namespace, CMake targets, file names) and the
    description (`CMakeLists.txt`, `flake.nix`, `README.md`). The Nix files
    read the name, version and description from `project(...)` in
    `CMakeLists.txt` (`nix/project.nix`), so they need no placeholders and
    stay identical to the template's;
  - sets the LICENSE year and holder (your git `user.name`);
  - leaves out the template-only parts: the generator itself
    (`nix/new-project/`) and the flake outputs that run and test it;
  - starts git on `main` (the branch CI runs on), stages everything (flakes
    only see files tracked by git) and runs `nix fmt`;
  - configures the `dev` preset, so clangd has
    `build/dev/compile_commands.json` before you open the editor (see
    Troubleshooting);
  - warns if any placeholder is left.
- Without prompts (e.g. from a script):
  `nix run github:rollschild/cpp-nix-template#new -- my_tool "One-line description"`.
- `github:` runs the template as pushed (Nix caches it for up to an hour; add
  `--refresh` right after pushing a template change). The local path runs your
  working tree, including uncommitted edits to tracked files.
- The template's `nix flake check` (and so its CI) runs the generator in the
  sandbox and checks that the result has no placeholders, formats cleanly,
  builds and passes its tests.

What the generator cannot fill in: the template's example code (a `greet`
function in `include/my_tool/my_tool.hpp`, `src/my_tool.cpp`, `apps/main.cpp`
and `tests/my_tool_test.cpp`; it keeps the build and tests green until you
replace it) and the body of `README.md`. Then:

```bash
nix flake check                 # optional: everything CI will check
git commit -m "Initial commit from cpp-nix-template"
gh repo create my_tool --private --source . --push   # optional: turns on CI and Dependabot
```

## 2. Open an existing project

Already on this machine:

```bash
cd ~/projects/venue_core         # direnv loads the cached dev shell
git pull                        # if anything changed upstream (e.g. a merged Dependabot PR)
cmake --workflow --preset dev --fresh   # only if flake.lock changed: new compiler path
```

Fresh clone (another machine, or from GitHub):

```bash
git clone git@github.com:rollschild/venue_core.git
cd venue_core
direnv allow                    # once per clone
cmake --workflow --preset dev   # before opening the editor: also writes the compile database clangd reads
```

## 3. Daily loop

```bash
nvim .                          # from the project dir (after the first configure): clangd and formatters come from the dev shell
cmake --build --preset dev      # build
ctest --preset dev              # test (or both at once: cmake --workflow --preset dev)
ctest --preset dev -R <regex>   # only the tests whose names match
./build/dev/tests/venue_core_tests "<test case name>"   # one Catch2 test case, with Catch2's own output
./build/dev/apps/venue_core [args]                      # run the executable (Debug build)
gdb --args ./build/dev/apps/venue_core [args]           # debug it (or nvim-dap: <leader>db, <leader>dc)
```

- New `.cpp`/`.hpp` files must be listed in their target's `CMakeLists.txt`
  (sources are not globbed). Saving a `CMakeLists.txt` in Neovim re-runs
  configure for existing build trees, so clangd stays current.
- `git add` new files: Nix (`nix build`, `nix flake check`) only sees tracked
  files. CMake builds see them regardless.
- To get past `-Werror` while experimenting:
  `cmake --preset dev --compile-no-warning-as-error`.

## 4. Before committing or pushing

```bash
cmake --workflow --preset asan  # Debug + AddressSanitizer + UBSan
cmake --workflow --preset tidy  # clang-tidy on every file; any warning fails it
nix fmt                         # format and lint C++, CMake, Python and Nix
git add -A
nix flake check                 # what CI runs: GCC + Clang builds and tests, clang-tidy, formatting and lint
git commit -m "..."
git push                        # CI also runs the asan workflow in the dev shell
```

## 5. Dependencies

Where each kind of dependency goes:

| You need | Put it in | Examples |
| --- | --- | --- |
| A library your code links against | `buildInputs` in `nix/package.nix` | `abseil-cpp`, `boost`, `liburing` |
| A library only the tests use | `checkInputs` in `nix/package.nix` | `catch2_3` (already there) |
| A tool that runs during the build | `nativeBuildInputs` in `nix/package.nix` | `protobuf` (for `protoc`) |
| A tool you only use at the keyboard | `packages` in `nix/shell.nix` | `hyperfine`, `valgrind`, `lldb` |
| A Python library for `scripts/` | `python3.withPackages` in `nix/shell.nix` | `ps.numpy`, `ps.matplotlib` |

Each Nix file is a function: the name goes in **two** places, the argument
list at the top (`{ lib, stdenv, ..., abseil-cpp }:`) and the list itself.
Save, and direnv reloads the shell.

### Find the package and its CMake names

```bash
nix search --inputs-from . nixpkgs '^abseil'      # attribute name, in the project's pinned nixpkgs
find $(nix build --no-link --print-out-paths --inputs-from . 'nixpkgs#abseil-cpp^*') \
    -name '*[Cc]onfig.cmake'                       # .../lib/cmake/absl/abslConfig.cmake -> find_package(absl)
grep -ho 'add_library([^ ]*' $(find $(nix build --no-link --print-out-paths --inputs-from . \
    'nixpkgs#abseil-cpp^*') -name '*[Tt]argets.cmake') | sort -u   # targets: absl::flat_hash_map, ...
```

### Add a C++ library (example: abseil)

1. `nix/package.nix`: add `abseil-cpp` to the arguments and to
   `buildInputs = [ abseil-cpp ];`.
2. `src/CMakeLists.txt`: `find_package(absl REQUIRED)` and
   `target_link_libraries(venue_core_lib PRIVATE absl::flat_hash_map)`.
   Use `PUBLIC` instead if its types appear in your headers under
   `include/venue_core/`, and then also add `find_dependency(absl)` to
   `cmake/venue_core-config.cmake.in`, so consumers of the installed package get
   it too.
3. Build and test: `cmake --workflow --preset dev`, then `nix build` or
   `nix flake check`. The Nix build runs in a sandbox, so it catches anything
   that only happens to be available in your shell.

A library only the tests use goes in `checkInputs` instead, with its
`find_package` and `target_link_libraries(venue_core_tests PRIVATE ...)` in
`tests/CMakeLists.txt`: that file is skipped when tests are off.

### Remove a dependency

1. Delete it from the list and from the arguments in `nix/package.nix` (or
   `nix/shell.nix`).
2. Delete its `find_package`, `target_link_libraries` entry,
   `find_dependency` line (if any) and `#include`s.
3. `cmake --workflow --preset dev --fresh` (`--fresh` drops the cached
   `find_package` results), then `nix flake check`.

### Versions

The pinned nixpkgs (`flake.lock`) decides every version, and
`nix flake update` moves them all together. When you need a different version
of one library, nixpkgs often carries it as its own attribute (`boost187` next
to `boost`); `nix search` lists them.

### A library that is not in nixpkgs

Package it in a small file under `nix/`, for example
`nix/unordered-dense.nix`:

```nix
{
  lib,
  stdenv,
  fetchFromGitHub,
  cmake,
  ninja,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "unordered-dense";
  version = "4.5.0";
  src = fetchFromGitHub {
    owner = "martinus";
    repo = "unordered_dense";
    tag = "v${finalAttrs.version}";
    hash = lib.fakeHash; # replace with the hash Nix prints on the first build
  };
  nativeBuildInputs = [
    cmake
    ninja
  ];
})
```

1. `git add nix/unordered-dense.nix`: flakes only see tracked files
   (otherwise: "... is not tracked by Git").
2. In `nix/package.nix`, add `callPackage` to the arguments and
   `(callPackage ./unordered-dense.nix { })` to `buildInputs`.
3. `nix build` fails once with `got: sha256-...`; paste that as the `hash`.
4. Use it from CMake like any other package
   (`find_package(unordered_dense REQUIRED)`,
   `unordered_dense::unordered_dense`).

Don't use CMake's `FetchContent` for dependencies: the Nix build sandbox has
no network, so `nix build` and CI would fail.

## 6. Occasional tasks

### Python scripts

- Put helper scripts (data generation, analysis, plotting) in `scripts/` and
  run them in the dev shell: `python3 scripts/foo.py`. Their libraries are in
  §5.
- `nix fmt` runs ruff (lint with autofix, then format) and `nix flake check`
  fails on what it cannot fix. The rules are in `ruff.toml`.
- Scripts run by the build (code generators): put
  `(python3.withPackages (ps: [ ... ]))` in `nativeBuildInputs` in
  `nix/package.nix` instead, call it from CMake through
  `find_package(Python3 COMPONENTS Interpreter)`, and add `../scripts` to the
  `fileset` there. The dev shell inherits it.

### Everything else

| Task | Commands |
| --- | --- |
| Optimised build (RelWithDebInfo), e.g. to benchmark or read the disassembly | `cmake --workflow --preset release`: exactly CMake's flags (the dev shell adds no hardening; the Nix package does) |
| Build the Nix package | `nix build`, then `./result/bin/venue_core` |
| Run the package without a checkout | `nix run . -- [args]` |
| Install to a prefix | `cmake --install build/release --prefix ~/.local` |
| Enter the dev shell without direnv | `nix develop` |
| Update the toolchain yourself | `nix flake update`, `cmake --workflow --preset dev --fresh`, `nix flake check`, commit `flake.lock` |
| Dependabot PR (monthly) | merge if CI is green, then `git pull` and `cmake --workflow --preset dev --fresh` |
| Bump the version | edit `project(... VERSION x.y.z)` in `CMakeLists.txt`; Nix reads it from there |
| Clean rebuild | `rm -rf build/dev && cmake --workflow --preset dev` |
| Personal CMake overrides (ccache, another generator or compiler) | `CMakeUserPresets.json`, git-ignored, can inherit these presets |

## 7. Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| clangd: `'venue_core/venue_core.hpp' file not found`, `no member named 'format' in namespace 'std'` | No compile database yet: clangd reads `build/dev/compile_commands.json` (see `.clangd`) and otherwise guesses the flags (no include path, C++17). A clangd started before the database appeared keeps the guess. | `cmake --preset dev`, then restart clangd: `:lsp restart clangd` in Neovim, `editor: restart language server` in Zed |
| `error: Path 'nix/foo.nix' ... is not tracked by Git` | Flakes only see files tracked by git | `git add nix/foo.nix` |
| `hash mismatch ... got: sha256-...` | A new or changed `fetchFromGitHub` | Paste the `got:` hash into `hash` |
| CMake complains the compiler changed, or still finds a removed package | Cached paths from before `nix flake update` or a dependency change | `cmake --workflow --preset dev --fresh` |
| clang-tidy fails on a false positive | Every warning is an error (`.clang-tidy`) | `// NOLINT(check-name): reason` on that line, or disable the check in `.clang-tidy` for the whole project |
| `direnv: error .envrc is blocked` | `.envrc` changed (or a fresh clone) | `direnv allow` |
| A tool or library is missing in the shell right after editing `nix/*.nix` | The shell hasn't reloaded yet | Press Enter at the prompt (direnv reloads), or `direnv reload` |
