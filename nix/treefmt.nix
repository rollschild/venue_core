# treefmt-nix module: one `nix fmt` for every language in the repo, and
# `nix flake check` fails if anything is unformatted or fails lint. The dev
# shell (nix/shell.nix) puts the same tools on PATH for the editor.
{ lib, pkgs, ... }:
{
  projectRootFile = "flake.nix";

  programs.clang-format.enable = true; # C++, reads .clang-format
  programs.nixfmt.enable = true; # *.nix, RFC 166 style
  programs.ruff-check.enable = true; # Python lint with autofix, reads ruff.toml
  programs.ruff-format.enable = true; # Python format, reads ruff.toml

  # CMake. gersemi is the maintained successor to cmake-format (no release
  # since 2020); treefmt-nix has no module for it, hence a plain formatter.
  settings.formatter.gersemi = {
    command = lib.getExe pkgs.gersemi;
    options = [ "--in-place" ];
    includes = [
      "CMakeLists.txt"
      "*/CMakeLists.txt" # treefmt globs: `*` also matches `/`
      "*.cmake"
    ];
  };
}
