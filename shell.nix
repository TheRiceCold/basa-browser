{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {

  buildInputs = with pkgs; [gcc cmake ninja gdb];

  shellHook = ''
    echo "Basa Browser development environment"

    function check-version() {
      g++ --version; cmake --version; gdb --version
    }

    function configure() {
      cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -G Ninja
    }

    function build() {
      cmake --build build
    }
    function rebuild() {
      cmake --build build --clean-first
    }
  '';
}