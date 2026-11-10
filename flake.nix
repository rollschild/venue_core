{
  description = "An exchange-side matching engine";

  inputs = {
    # Pin a channel that cache.nixos.org actually builds; `master` is not cached.
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    # One `nix fmt` entry point for C++, CMake, Python and Nix sources.
    treefmt-nix = {
      url = "github:numtide/treefmt-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      treefmt-nix,
    }:
    let
      inherit (nixpkgs) lib;

      # Name, version and description, parsed from project(...) in CMakeLists.txt.
      project = import ./nix/project.nix;

      # nixpkgs dropped x86_64-darwin in 26.11.
      systems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
      ];

      # { <system> = f pkgs; } for every supported system, no flake-utils needed.
      forAllSystems = f: lib.genAttrs systems (system: f nixpkgs.legacyPackages.${system});

      treefmtFor = forAllSystems (pkgs: treefmt-nix.lib.evalModule pkgs ./nix/treefmt.nix);
    in
    {
      # Consumers add this overlay and get `pkgs.<project name>`.
      overlays.default = final: _prev: {
        ${project.name} = final.callPackage ./nix/package.nix { };
      };

      packages = forAllSystems (
        pkgs:
        let
          package = pkgs.callPackage ./nix/package.nix { };
        in
        {
          default = package;
          ${project.name} = package;
        }
      );

      devShells = forAllSystems (pkgs: {
        default = pkgs.callPackage ./nix/shell.nix {
          package = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
        };
      });

      # `nix flake check`: build and test with the default compiler and with
      # Clang, run clang-tidy over every file, then verify formatting and lint.
      checks = forAllSystems (
        pkgs:
        let
          inherit (pkgs.stdenv.hostPlatform) system;
          package = self.packages.${system}.default;
        in
        {
          inherit package;
          clang = package.override { stdenv = pkgs.clangStdenv; };
          # clang-tidy on every translation unit, tests included (.clang-tidy:
          # warnings are errors)
          tidy = package.overrideAttrs (old: {
            pname = "${old.pname}-tidy";
            nativeBuildInputs = old.nativeBuildInputs ++ [ pkgs.clang-tools ];
            cmakeFlags = old.cmakeFlags ++ [
              "-DCMAKE_CXX_CLANG_TIDY=clang-tidy;--extra-arg=-Wno-unknown-warning-option"
            ];
          });
          formatting = treefmtFor.${system}.config.build.check self;
        }
      );

      # `nix fmt` formats the whole tree.
      formatter = forAllSystems (
        pkgs: treefmtFor.${pkgs.stdenv.hostPlatform.system}.config.build.wrapper
      );
    };
}
