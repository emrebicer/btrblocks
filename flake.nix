{
  description = "BtrBlocks git development flake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-24.05";
    nixpkgs-unstable.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = { self, nixpkgs, nixpkgs-unstable, ... }@inputs: 

    let 
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in {

        devShells.${system}.default = pkgs.mkShellNoCC {
          buildInputs = with pkgs; [
            (pkgs.python3.withPackages (python-pkgs: with python-pkgs; [
              pandas
              numpy
            ]))
              python311Packages.cmake
              gcc9
              gnumake
              zlib
              openssl_legacy
              curlFull
              ccls
              gflags
              yaml-cpp
              spdlog
              tbb
              boost
          ];
        };

    };

}
