@echo off
setlocal
cd /d "%~dp0"

where nasm >nul 2>&1 || (echo NASM tidak ditemukan di PATH. & exit /b 1)
where gcc >nul 2>&1 || (echo GCC MinGW-w64 tidak ditemukan di PATH. & exit /b 1)

if not exist build mkdir build
if not exist build\scan mkdir build\scan
g++ -std=c++17 -Os -s -Wall -Wextra -fno-exceptions -fno-rtti src/windows_settings.cpp -lruntimeobject -lole32 -static-libgcc -static-libstdc++ -o build/scan/O3Windows.exe || exit /b 1
windres src/o3.rc -O coff -o build/o3res.obj || exit /b 1
nasm -f win64 -I src/ src/main.asm -o build/main.obj || exit /b 1
gcc -std=c11 -Os -Wall -Wextra -fno-stack-protector -fno-builtin -fno-tree-loop-distribute-patterns ^
    -c src/probes.c -o build/probes.obj || exit /b 1
g++ -std=c++17 -Os -Wall -Wextra -fno-exceptions -fno-rtti -c src/audio_session.cpp -o build/audio_session.obj || exit /b 1
gcc -nostdlib -nostartfiles -Wl,-e,entry -Wl,--subsystem,windows build/main.obj build/probes.obj build/audio_session.obj build/o3res.obj -lole32 -loleaut32 -luuid -lws2_32 -lpsapi ^
    -luser32 -lgdi32 -ladvapi32 -lpowrprof -lwlanapi -lshell32 -lshlwapi -lkernel32 -o build/OffOrOn.exe || exit /b 1
if not exist build\scan mkdir build\scan
copy /y scan\probes.txt build\scan\probes.txt >nul || exit /b 1
copy /y scan\probe.ps1 build\scan\probe.ps1 >nul || exit /b 1
copy /y scan\probe.cmd build\scan\probe.cmd >nul || exit /b 1
echo build\OffOrOn.exe
