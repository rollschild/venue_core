# The package, written nixpkgs-style so it can be `callPackage`d from the flake,
# from an overlay, or copied into nixpkgs later.
{
  lib,
  stdenv,
  cmake,
  ninja,
  catch2_3,
}:

let
  # Name, version and description come from project(...) in CMakeLists.txt.
  project = import ./project.nix;
in
stdenv.mkDerivation (finalAttrs: {
  pname = project.name;
  inherit (project) version;

  # Only what CMake needs: editing docs, scripts or Nix files never rebuilds.
  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../CMakeLists.txt
      ../cmake
      ../include
      ../src
      ../apps
      ../tests
      ../.clang-tidy # read only by the `tidy` check in flake.nix
    ];
  };

  nativeBuildInputs = [
    cmake
    ninja
  ];

  # Libraries the project links against; each becomes a `find_package` target.
  buildInputs = [ ];

  # Test-only dependencies, added to the build only when doCheck is true.
  checkInputs = [ catch2_3 ];

  cmakeFlags = [
    (lib.cmakeBool "CMAKE_COMPILE_WARNING_AS_ERROR" true)
  ];

  # Run the tests (ctest) after building. When false, the cmake hook passes
  # -DBUILD_TESTING=OFF, so Catch2 is not needed at all.
  doCheck = true;

  # Keep build-time and run-time dependencies separate (catches misplaced inputs).
  strictDeps = true;

  meta = {
    inherit (project) description;
    homepage = "https://github.com/rollschild/${finalAttrs.pname}";
    license = lib.licenses.mit;
    platforms = lib.platforms.unix;
    mainProgram = finalAttrs.pname; # apps/ builds the executable under the project name
  };
})
