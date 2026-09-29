@echo off
REM Builds Sideways City with Visual Studio 2022 (Desktop development with C++) and CMake.
REM raylib 5.5 is downloaded automatically on the first build (needs git + internet).
cmake -B build -G "Visual Studio 17 2022" -A x64 || goto :error
cmake --build build --config Release || goto :error
echo.
echo Done. Run build\bin\sideways_launcher.exe
goto :eof
:error
echo Build failed.
exit /b 1
