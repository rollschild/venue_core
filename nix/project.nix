# Project metadata, parsed from `project(<name> VERSION ... DESCRIPTION "...")`
# in CMakeLists.txt, the single source of truth (like Rust flakes reading
# Cargo.toml). The Nix files never spell out the project's name or version.
let
  cmakeLists = builtins.readFile ../CMakeLists.txt;
  field = re: builtins.head (builtins.match ".*project\\([[:space:]]*${re}.*" cmakeLists);
in
{
  name = field "([A-Za-z0-9_]+)";
  version = field "[^)]*VERSION[[:space:]]+([^[:space:])]+)";
  description = field "[^)]*DESCRIPTION[[:space:]]+\"([^\"]*)\"";
}
