with import <nixpkgs> { };
mkShell {
  buildInputs = [ ncurses ffmpeg ];
  LD_LIBRARY_PATH = lib.makeLibraryPath [
    ncurses
    ffmpeg
  ];
  NIX_LD = lib.fileContents "${stdenv.cc}/nix-support/dynamic-linker";
}
