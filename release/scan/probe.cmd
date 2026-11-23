@echo off
setlocal EnableExtensions DisableDelayedExpansion
for /f "usebackq tokens=1-6 delims=|" %%A in ("%~dp0probes.txt") do (
    if /i "%%B"=="I" call :instance %%A "%%C"
    if /i "%%B"=="G" call :global %%A "%%C"
    if /i "%%B"=="D" call :registry %%A "%%C" "%%D"
    if /i "%%B"=="S" call :registry %%A "%%C" "%%D"
    if /i "%%B"=="B" call :registry %%A "%%C" "%%D"
)
exit /b 0

:instance
if not defined O3_SCAN_CONFIG exit /b 0
if not defined O3_SCAN_INSTANCE exit /b 0
echo @%1
"%SystemRoot%\System32\findstr.exe" /b /l /c:"bst.instance.%O3_SCAN_INSTANCE%.%~2=" "%O3_SCAN_CONFIG%" 2>nul
exit /b 0

:global
if not defined O3_SCAN_CONFIG exit /b 0
echo @%1
"%SystemRoot%\System32\findstr.exe" /b /l /c:"bst.%~2=" "%O3_SCAN_CONFIG%" 2>nul
exit /b 0

:registry
echo @%1
"%SystemRoot%\System32\reg.exe" query "HKCU\%~2" /v "%~3" 2>nul
exit /b 0
