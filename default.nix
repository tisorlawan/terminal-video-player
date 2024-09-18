with import <nixpkgs> { };
mkShell {
  buildInputs = [ ncurses ];
  LD_LIBRARY_PATH = lib.makeLibraryPath [
    ncurses
  ];
  NIX_LD = lib.fileContents "${stdenv.cc}/nix-support/dynamic-linker";
}
