{ pkgs ? import <nixpkgs> { } }:
let
  pname = "rperf";
  version = "0.2.0";
  lib = pkgs.lib;
in
pkgs.mkShell {
  packages = with pkgs;
    [
      cmake
      flamegraph
      rustup
      clippy
      rust-analyzer
      direnv
    ];
  shellHook =
    let
      icon = "f121";
    in
    ''
      export PS1="$(echo -e '\u${icon}') {\[$(tput sgr0)\]\[\033[38;5;228m\]\w\[$(tput sgr0)\]\[\033[38;5;15m\]} (${pname}-${version}) \\$ \[$(tput sgr0)\]"

    '';
}
