This box builds nothing: build the Windows programs on a linux box
(pixi run -e dev build RelWithDebInfo -- --platforms=@xclang//platforms:x86_64-w64-mingw32,
aarch64-w64-mingw32 for windows-arm), bring them over with
box cp <linux> <this> build/RelWithDebInfo/bin/bin --exclude '*.params' --exclude '*.stripped' --exclude '*.gsymutil.sh'
and run the suites as CI does: pixi run -e dev unit-test RelWithDebInfo,
pixi run -e dev cases-test RelWithDebInfo, through PowerShell
(pwsh -NoProfile -Command "..."), as CI's default shell.
