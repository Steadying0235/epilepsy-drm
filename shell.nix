with import <nixpkgs> {};
stdenv.mkDerivation {
  name = "env";
  nativeBuildInputs = [ pkg-config ];
  buildInputs = [
    libdrm
    libGL
    xorg.libX11
  ];

  # possible workaround for libdrm CFLAGS not propagating correctly https://github.com/NixOS/nixpkgs/issues/348171
  #CFLAGS = "-I${libdrm}/include/libdrm";
}

