@echo off
rem ===========================================================================
rem  AX25Chat - complete Windows build
rem
rem  Fetches the Dire Wolf sources, configures, compiles, deploys the Qt
rem  runtime and the data files, bundles the Visual C++ runtime and builds
rem  the installer, in one go.
rem
rem  Run it from the project root, in a "x64 Native Tools Command Prompt
rem  for VS 2022" (or 2026). Needs, installed once:
rem    - Visual Studio with "Desktop development with C++" AND the individual
rem      component "C++ Clang Compiler for Windows" (clang-cl): the Dire Wolf
rem      sources use GCC extensions that cl.exe does not accept. CMake does
rem      not let one project mix clang-cl and cl, so clang-cl compiles
rem      everything, C and C++ alike; it speaks the MSVC ABI and runtime,
rem      which is what the Qt MSVC binaries expect.
rem    - Qt 6 for MSVC 64-bit (Widgets, Network, SerialPort; Multimedia,
rem      Positioning and Quick/QuickControls2 optional: the first adds custom
rem      notification sounds, the second the device's location, the third
rem      builds the touch interface, ax25chat-mobile, which runs on Windows too)
rem    - git (to fetch Dire Wolf)
rem    - Inno Setup 6, for the installer
rem
rem  Options:
rem    /deps         fetch and patch the Dire Wolf sources, and the symbol
rem                  artwork, if missing
rem    /clean        wipe the build directory first
rem    /nobuild      skip configure and compile, deploy and package only
rem    /noinstaller  stop after staging, do not run Inno Setup
rem    /test         run the unit tests after compiling
rem    /help         show this help (/? too)
rem ===========================================================================

setlocal enabledelayedexpansion
title AX25Chat - complete build

rem ------------------------------------------------------- paths to adjust
set "QT_DIR=C:\Qt\6.11.2\msvc2022_64"

set "BUILD_DIR=build"
set "DIST=installer\dist"
set "DIREWOLF_DIR=external\direwolf"
set "DIREWOLF_REF=1.8"

rem ------------------------------------------------------------- arguments
set DO_DEPS=0
set DO_CLEAN=0
set DO_BUILD=1
set DO_INSTALLER=1
set DO_TEST=0

:parse
if "%~1"=="" goto parsed
if /i "%~1"=="/deps"        set DO_DEPS=1&       shift & goto parse
if /i "%~1"=="/clean"       set DO_CLEAN=1&      shift & goto parse
if /i "%~1"=="/nobuild"     set DO_BUILD=0&      shift & goto parse
if /i "%~1"=="/noinstaller" set DO_INSTALLER=0&  shift & goto parse
if /i "%~1"=="/test"        set DO_TEST=1&       shift & goto parse
if /i "%~1"=="/?"           goto usage
if /i "%~1"=="/help"        goto usage
echo Unknown option: %~1
goto usage
:parsed

if not exist "CMakeLists.txt" (
    echo [X] Run this script from the project root, next to CMakeLists.txt.
    goto fail
)

rem The application version, read from CMakeLists.txt and passed to Inno
rem Setup. This line stays at top level: the searched string holds a
rem parenthesis, which would break a parenthesised block.
set "APP_VERSION="
for /f "tokens=1-3" %%A in ('findstr /b /c:"project(AX25Chat" CMakeLists.txt') do set "APP_VERSION=%%C"
if not defined APP_VERSION for /f "tokens=2" %%A in ('findstr /r /c:"^  VERSION [0-9]" CMakeLists.txt') do set "APP_VERSION=%%A"
if not defined APP_VERSION set "APP_VERSION=0.0.0"

rem ---------------------------------------------------------- prerequisites
echo.
echo === Checking prerequisites ===

where cmake >nul 2>&1
if errorlevel 1 (
    echo [X] cmake not found in PATH. Open a "x64 Native Tools Command Prompt".
    goto fail
)
where ninja >nul 2>&1
if errorlevel 1 (
    echo [X] ninja not found in PATH. It comes with the "C++ CMake tools for
    echo     Windows" component of Visual Studio; open a "x64 Native Tools
    echo     Command Prompt".
    goto fail
)
where cl >nul 2>&1
if errorlevel 1 (
    echo [X] cl.exe not found. This must run in a "x64 Native Tools Command Prompt".
    goto fail
)
rem clang-cl: in PATH when the Clang component is installed, otherwise looked
rem up in the Visual Studio this prompt belongs to.
set "CLANG_CL="
where clang-cl >nul 2>&1
if not errorlevel 1 set "CLANG_CL=clang-cl"
if not defined CLANG_CL if defined VCINSTALLDIR if exist "%VCINSTALLDIR%Tools\Llvm\x64\bin\clang-cl.exe" set "CLANG_CL=%VCINSTALLDIR%Tools\Llvm\x64\bin\clang-cl.exe"
if not defined CLANG_CL (
    echo [X] clang-cl.exe not found. The Dire Wolf core needs it: install the
    echo     "C++ Clang Compiler for Windows" component with the Visual Studio
    echo     Installer, under Individual components, then open a new prompt.
    goto fail
)
echo [ok] cl and clang-cl (!CLANG_CL!)

if not exist "%QT_DIR%\bin\windeployqt.exe" (
    echo [X] Qt not found at %QT_DIR%
    echo     Edit QT_DIR at the top of this script.
    goto fail
)
echo [ok] Qt          %QT_DIR%

rem Inno Setup, looked up in the usual places then in PATH.
set "ISCC="
for %%P in (
    "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
    "%ProgramFiles%\Inno Setup 6\ISCC.exe"
    "%ProgramFiles(x86)%\Inno Setup 7\ISCC.exe"
    "%ProgramFiles%\Inno Setup 7\ISCC.exe"
) do if not defined ISCC if exist %%P set "ISCC=%%~P"
if not defined ISCC for /f "delims=" %%P in ('where ISCC 2^>nul') do if not defined ISCC set "ISCC=%%P"

if "%DO_INSTALLER%"=="1" (
    if defined ISCC (
        echo [ok] Inno Setup  !ISCC!
    ) else (
        echo [--] Inno Setup not found, the installer step will be skipped.
        set DO_INSTALLER=0
    )
)

rem --------------------------------------------------------------- deps
if "%DO_DEPS%"=="1" call :deps
if errorlevel 1 goto fail

if not exist "%DIREWOLF_DIR%\src\direwolf.h" (
    echo [X] Dire Wolf sources not found in %DIREWOLF_DIR%. Run build_all.bat /deps
    goto fail
)
findstr /c:"tq_term" "%DIREWOLF_DIR%\src\tq.h" >nul 2>&1
if errorlevel 1 (
    echo [X] The Dire Wolf sources in %DIREWOLF_DIR% do not carry the patch.
    echo     Run build_all.bat /deps, or apply patches\direwolf\*.patch by hand.
    goto fail
)
echo [ok] Dire Wolf   %DIREWOLF_DIR%

rem ------------------------------------------------------------------ clean
if "%DO_CLEAN%"=="1" (
    echo.
    echo === Cleaning ===
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    if exist "%DIST%"      rmdir /s /q "%DIST%"
    if exist "installer\output" rmdir /s /q "installer\output"
)

rem -------------------------------------------------------------- configure
if "%DO_BUILD%"=="1" (
    echo.
    echo === Configuring ===
    cmake -B "%BUILD_DIR%" -G Ninja ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_C_COMPILER="!CLANG_CL:\=/!" ^
        -DCMAKE_CXX_COMPILER="!CLANG_CL:\=/!" ^
        -DCMAKE_PREFIX_PATH="%QT_DIR%"
    if errorlevel 1 (
        echo [X] Configuration failed.
        goto fail
    )

    echo.
    echo === Compiling ===
    cmake --build "%BUILD_DIR%"
    if errorlevel 1 (
        echo [X] Compilation failed.
        goto fail
    )
)

if "%DO_TEST%"=="1" (
    echo.
    echo === Tests ===
    set "QT_QPA_PLATFORM=offscreen"
    ctest --test-dir "%BUILD_DIR%" --output-on-failure
    if errorlevel 1 (
        echo [X] A test failed.
        goto fail
    )
    set "QT_QPA_PLATFORM="
)

set "OUT=%BUILD_DIR%"
if not exist "%OUT%\ax25chat.exe" (
    echo [X] %OUT%\ax25chat.exe missing. Compile without /nobuild first.
    goto fail
)

rem ------------------------------------------------------------------ stage
echo.
echo === Gathering into %DIST% ===
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%DIST%" 2>nul

copy /y "%OUT%\ax25chat.exe" "%DIST%\" >nul

echo   Qt runtime...
"%QT_DIR%\bin\windeployqt.exe" --release --no-system-d3d-compiler --no-opengl-sw ^
    "%DIST%\ax25chat.exe" >nul
if errorlevel 1 (
    echo [X] windeployqt failed.
    goto fail
)

rem The files the decoders look for in data\ next to the program: device
rem identification from the destination address, and the newer overlay
rem symbols. Taken from the Dire Wolf tree.
echo   Data files...
mkdir "%DIST%\data" 2>nul
copy /y "%DIREWOLF_DIR%\data\tocalls.yaml"     "%DIST%\data\" >nul
copy /y "%DIREWOLF_DIR%\data\symbols-new.txt"  "%DIST%\data\" >nul

rem The aprs.fi symbol artwork, when it was fetched (build_all.bat /deps).
if exist "assets\aprs-symbols\aprs-symbols-64-0.png" (
    echo   Symbol artwork...
    mkdir "%DIST%\assets\aprs-symbols" 2>nul
    copy /y "assets\aprs-symbols\*" "%DIST%\assets\aprs-symbols\" >nul
) else (
    echo [--] assets\aprs-symbols not present: the picker and the station list
    echo      will show names and codes. Run build_all.bat /deps to fetch it.
)

rem The Visual C++ runtime. A fresh Windows does not have it, and the
rem program then stops at start-up on a missing MSVCP140.dll. windeployqt
rem usually drops the redistributable next to the program; if not, it comes
rem from the Visual Studio this prompt belongs to. The installer runs it.
echo   Visual C++ runtime...
if not exist "%DIST%\vc_redist.x64.exe" (
    if defined VCToolsRedistDir if exist "%VCToolsRedistDir%vc_redist.x64.exe" copy /y "%VCToolsRedistDir%vc_redist.x64.exe" "%DIST%\" >nul
)
if exist "%DIST%\vc_redist.x64.exe" (
    echo [ok] vc_redist.x64.exe bundled
) else (
    echo [--] vc_redist.x64.exe not found: the installer will rely on the runtime
    echo      already being present on the target machine.
)

copy /y "README.md" "%DIST%\README.md" >nul
copy /y "LICENSE.txt" "%DIST%\LICENSE.txt" >nul

rem -------------------------------------------------------------- installer
if "%DO_INSTALLER%"=="1" (
    echo.
    echo === Building the installer, version %APP_VERSION% ===
    "!ISCC!" /Qp /DAppVersion=%APP_VERSION% "installer\AX25Chat.iss"
    if errorlevel 1 (
        echo [X] Inno Setup failed.
        goto fail
    )
    echo.
    echo [ok] Installer: installer\output\AX25Chat-%APP_VERSION%-setup.exe
) else (
    echo.
    echo [ok] Staged in %DIST%; run ax25chat.exe from there, or build the
    echo      installer with: "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" /DAppVersion=%APP_VERSION% installer\AX25Chat.iss
)

echo.
echo Done.
endlocal
exit /b 0

:deps
rem The Dire Wolf sources (tag %DIREWOLF_REF% plus our patch) and the symbol
rem artwork. Qt, Visual Studio and Inno Setup have installers of their own
rem and are not handled here.
echo.
echo === Dependencies ===
if not exist "%DIREWOLF_DIR%\src\direwolf.h" (
    where git >nul 2>&1
    if errorlevel 1 (
        echo [X] git is needed to fetch Dire Wolf: https://git-scm.com/download/win
        exit /b 1
    )
    echo   Cloning Dire Wolf %DIREWOLF_REF% into %DIREWOLF_DIR%...
    if exist "%DIREWOLF_DIR%" rmdir /s /q "%DIREWOLF_DIR%"
    git clone --depth 1 --branch %DIREWOLF_REF% https://github.com/wb2osz/direwolf.git "%DIREWOLF_DIR%"
    if errorlevel 1 (
        echo [X] Cloning Dire Wolf failed.
        exit /b 1
    )
)
findstr /c:"tq_term" "%DIREWOLF_DIR%\src\tq.h" >nul 2>&1
if errorlevel 1 (
    echo   Applying patches\direwolf\*.patch...
    for %%P in (patches\direwolf\*.patch) do (
        git -C "%DIREWOLF_DIR%" apply --ignore-whitespace "%CD%\%%P"
        if errorlevel 1 (
            echo [X] %%P does not apply to the sources in %DIREWOLF_DIR%.
            exit /b 1
        )
    )
)
echo [ok] Dire Wolf sources ready
if not exist "assets\aprs-symbols\aprs-symbols-64-0.png" (
    echo   Fetching the aprs.fi symbol sheets...
    mkdir "assets\aprs-symbols" 2>nul
    for %%F in (aprs-symbols-64-0.png aprs-symbols-64-1.png aprs-symbols-64-2.png) do (
        curl -fsSL -o "assets\aprs-symbols\%%F" "https://raw.githubusercontent.com/hessu/aprs-symbols/master/png/%%F"
        if errorlevel 1 echo [--] Could not fetch %%F; the artwork stays optional.
    )
    curl -fsSL -o "assets\aprs-symbols\COPYRIGHT.md" "https://raw.githubusercontent.com/hessu/aprs-symbols/master/COPYRIGHT.md" 2>nul
)
exit /b 0

:usage
echo.
echo Usage: build_all.bat [/deps] [/clean] [/nobuild] [/noinstaller] [/test]
echo.
echo   /deps         fetch and patch the Dire Wolf sources, and the symbol artwork
echo   /clean        wipe the build directory first
echo   /nobuild      skip configure and compile; deploy and package only
echo   /noinstaller  stop after staging installer\dist
echo   /test         run the unit tests after compiling
echo.
echo Adjust QT_DIR at the top of the file.
endlocal
exit /b 2

:fail
echo.
echo Build aborted.
endlocal
exit /b 1
