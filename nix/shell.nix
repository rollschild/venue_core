# Developer shell. Inherits every build and test dependency from the package so
# the two never drift apart, then adds tools you only want at the keyboard.
{
  lib,
  stdenv,
  buildEnv,
  mkShell,
  package,
  clang-tools,
  gdb,
  gersemi,
  nixfmt,
  python3,
  ruff,
}:

mkShell {
  name = "${package.pname}-dev";

  inputsFrom = [ package ];

  packages = [
    clang-tools # clangd, clang-tidy, clang-format
    # The formatters `nix fmt` runs (nix/treefmt.nix), on PATH so that editor
    # format-on-save produces exactly what CI checks. gersemi is a Python
    # application: exposing only its bin/ keeps its propagated interpreter and
    # libraries out of the shell, where they would shadow python3 below and
    # leak into PYTHONPATH.
    (buildEnv {
      name = "gersemi";
      paths = [ gersemi ];
      pathsToLink = [ "/bin" ];
    })
    nixfmt
    ruff
    # Python for scripts/. Add libraries from nixpkgs, e.g. `ps: [ ps.numpy ]`.
    (python3.withPackages (ps: [ ]))
  ]
  ++ lib.optionals stdenv.hostPlatform.isLinux [ gdb ];

  # No cc-wrapper hardening in the dev shell, so dev builds compile with exactly
  # the flags CMake (and compile_commands.json, i.e. clangd and clang-tidy)
  # shows. Otherwise the wrapper silently adds, among others:
  #   - -fno-strict-overflow: signed overflow wraps, so UBSan can't catch it
  #   - fortify's -O2: Debug builds get optimised
  #   - -fzero-call-used-regs, stack protector and stack-clash probes: extra
  #     instructions in every function you disassemble or benchmark
  # The Nix package (nix build, nix flake check) keeps nixpkgs' full hardening:
  # it is what you ship.
  hardeningDisable = [ "all" ];
}
