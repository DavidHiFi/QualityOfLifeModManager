scripts/
========

build.ps1 (the one used for releases)
-------------------------------------
Dynamic build against an installed Qt 6 MinGW kit, then windeployqt stages
dist\ with the Qt DLLs next to QualityOfLifeModManager.exe. Zip dist\ and
that is the portable download.

  powershell -ExecutionPolicy Bypass -File scripts\build.ps1 [-Clean]

Paths at the top of the script: Qt kit, MinGW bin, and the Visual Studio
folder that carries cmake.exe and ninja.exe. windres is run with
--use-temp-file because the piped preprocessor fails on some machines.

gen_update.py
-------------
Regenerates qol_update.json from the latest GitHub releases. Run after
publishing a release, commit the result, and the app offers the update.

  python scripts\gen_update.py -o qol_update.json


The two .bat files below are the upstream Cod LAN Launcher static-Qt
route (one exe, no DLLs). They still work; expect a 1-3 hour Qt build.

1. build-qt-static.bat
----------------------
One-shot compile of Qt itself. Run this only when you do not already have
Y:\QT\6.11.2-static\bin\qmake.exe. Expect 1-3 hours.

  scripts\build-qt-static.bat

Variables to change (top of the file):

  QT_SHARED   Official (shared) Qt kit already installed.
              Default: Y:\QT\6.11.2\mingw_64
              Used only so qmake / cmake from that kit sit on PATH.

  MINGW_BIN   MinGW that will compile Qt and later the app.
              Default: Y:\QT\Tools\mingw1310_64\bin
              Must contain gcc.exe and mingw32-make.exe.

  NINJA_DIR   Folder with ninja.exe (Qt's configure uses it).
              Default: Y:\QT\Tools\Ninja

  QT_SRC      Unpacked qt-everywhere source tree.
              Default: Y:\src\qt-everywhere-src-6.11.2
              configure.bat must exist in that folder.

  PREFIX      Where the finished static Qt is installed.
              Default: Y:\QT\6.11.2-static
              This path must match PREFIX in build-app.bat.

  BUILD       Scratch directory for the Qt build (deleted each run).
              Default: Y:\QT\build-6.11.2-static

Needs gcc, cmake, python, perl (Strawberry) and ninja on PATH.


2. build-app.bat
----------------
Configure + compile the app and copy the exe to dist-static\.
Art, icons and language files are already inside the exe.

  scripts\build-app.bat

Variables to change (top of the file):

  PREFIX      Same static Qt prefix produced above.
              Default: Y:\QT\6.11.2-static

  MINGW_BIN   Same MinGW bin folder.
              Default: Y:\QT\Tools\mingw1310_64\bin

  BUILD_DIR   App build folder (wiped every run).
              Default: build-static   (next to CMakeLists.txt)

  DIST        Output folder. Only QualityOfLifeModManager.exe is copied here.
              Default: dist-static

Uses the "MinGW Makefiles" generator on purpose. Ninja was observed to
re-run CMake forever when the repo lives on C: and Qt lives on Y:.


Typical flow
------------
  1. Install a shared Qt 6.11.2 MinGW kit (Maintenance Tool) on Y:\QT
  2. Unpack qt-everywhere-src-6.11.2 under Y:\src
  3. Edit the SET lines if your layout is different
  4. scripts\build-qt-static.bat
  5. scripts\build-app.bat
  6. Ship dist-static\QualityOfLifeModManager.exe


Windows exe icon
----------------
resources/icons/icon.ico is linked into QualityOfLifeModManager.exe via resources/app.rc.in.
Replace that .ico and rebuild if you want a different mark.
