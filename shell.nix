{ pkgs ? import <nixpkgs> {}}:
pkgs.mkShell {
	buildInputs = with pkgs; [
		pkg-config
		cmake
		libpulseaudio
		alsa-lib
	];
	LD_LIBRARY_PATH = [ "${pkgs.libpulseaudio}/lib:${pkgs.alsa-lib}/lib" ];
}


