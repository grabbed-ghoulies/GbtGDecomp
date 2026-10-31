GbtGDecomp for Linux and the Steam Deck
=======================================

This package has no game files. You need your own dump of the retail Xbox disc of
Grabbed by the Ghoulies (the folder with default.xbe, bundles/, demand/, font/,
loctext/, movie/ and xwavebank/).

Install
-------
1. Extract this folder anywhere, for example /home/deck/Games/gbtg.
2. Put your disc dump beside gbtg.sh as a folder named OriginalFiles, or set
   GameDir in the settings file (below) to wherever it is.
3. Run gbtg.sh once. It writes ~/.local/share/gbtg/gbtg.ini.

Steam Deck (Game Mode)
----------------------
1. In Desktop Mode, open Steam, choose Games > Add a Non-Steam Game to My Library,
   browse to gbtg.sh and add it.
2. The default controller layout for a non-Steam game is the gamepad layout,
   which the game reads as an Xbox controller.
3. Switch back to Game Mode and start it from the library.

Settings: ~/.local/share/gbtg/gbtg.ini
--------------------------------------
[Video] Width, Height, Fullscreen (1 covers the screen), VSync, Widescreen (the
Xbox's 16:9 setting, on by default)
[Audio] MasterVolume (0 to 100)
[Game]  Language, GameDir

The same folder holds gbtg.log, saves/ and cache/ (compiled shaders; the first
run of each scene compiles its shaders and can stutter briefly).
Set GBTG_DATA_DIR to use a different folder.

Keyboard: arrows, Enter (A), Backspace or B (B), Space (Start), X, [ and ],
WASD and IJKL for the sticks, Escape quits, F8 debug menu, F12 timing overlay.

licenses/ has the licences of the libraries shipped here. FFmpeg is LGPL and
ships as unmodified shared libraries (libav*.so, libsw*.so).
