@echo off
setlocal
set ROOT_DIR=%~dp0..
set BUILD_DIR=%ROOT_DIR%\build

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%" --config Release --parallel
if errorlevel 1 exit /b 1

ctest --test-dir "%BUILD_DIR%" -C Release --output-on-failure
if errorlevel 1 exit /b 1

echo.
echo Build complete! Plugin artifacts are located in:
echo   %BUILD_DIR%\ALRVoiceColor_artefacts\Release\VST3\
echo   %BUILD_DIR%\ALRVoiceColor_artefacts\Release\Standalone\
endlocal
