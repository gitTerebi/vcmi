@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "BUILD_CONFIG=Release"
set "BUILD_DIR=%ROOT%\build-ninja-%BUILD_CONFIG%"
set "DEPLOY_DIR=G:\games\VCMI"
set "CONAN_OUT=%ROOT%\conan-msvc"
set "DEPS_DIR=%ROOT%\build-deps"
set "TOOLS_DIR=%DEPS_DIR%\tools"
set "CONAN_HOME=%DEPS_DIR%\conan-home"
set "VS_NINJA_DIR=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "VS_VCVARSALL=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

cd /d "%ROOT%" || exit /b 1
set "PATH=%TOOLS_DIR%;%PATH%"
if exist "%VS_NINJA_DIR%\ninja.exe" set "PATH=%VS_NINJA_DIR%;%PATH%"

where cl >nul 2>nul
if errorlevel 1 (
	if exist "%VS_VCVARSALL%" (
		echo Loading Visual Studio compiler environment...
		call "%VS_VCVARSALL%" x64 -vcvars_ver=14.2 || exit /b 1
	) else (
		echo MSVC compiler environment was not found.
		echo Run this script from a Developer Command Prompt for VS 2022.
		exit /b 1
	)
)

if not exist "%CONAN_OUT%\conan_toolchain.cmake" (
	echo Missing Conan toolchain:
	echo   %CONAN_OUT%\conan_toolchain.cmake
	echo Run setup-deps.bat first.
	exit /b 1
)

if not exist "%CONAN_OUT%\conanrun.bat" (
	echo Missing Conan runtime environment:
	echo   %CONAN_OUT%\conanrun.bat
	echo Run setup-deps.bat first.
	exit /b 1
)

if exist "%CONAN_OUT%\conanrun.bat" (
	echo Loading Conan runtime environment...
	call "%CONAN_OUT%\conanrun.bat" || exit /b 1
)

where ninja >nul 2>nul
if errorlevel 1 (
	echo Ninja was not found on PATH.
	echo Install Ninja or make sure Visual Studio CMake tools are installed.
	exit /b 1
)

if not exist "%BUILD_DIR%\CMakeCache.txt" (
	echo No CMake cache found. Run setup-deps.bat first to configure.
	exit /b 1
)

echo Building %BUILD_CONFIG%...
cmake --build "%BUILD_DIR%" --parallel || exit /b 1

echo Copying built files to %DEPLOY_DIR%...
if not exist "%DEPLOY_DIR%" mkdir "%DEPLOY_DIR%" || exit /b 1
robocopy "%BUILD_DIR%\bin" "%DEPLOY_DIR%" *.exe /COPY:DAT /R:3 /W:1
if errorlevel 8 exit /b 1
robocopy "%BUILD_DIR%\bin" "%DEPLOY_DIR%" *.dll /S /COPY:DAT /DCOPY:DAT /R:3 /W:1 /XJ
if errorlevel 8 exit /b 1
if exist "%BUILD_DIR%\bin\config" (
	robocopy "%BUILD_DIR%\bin\config" "%DEPLOY_DIR%\config" /E /XO /COPY:DAT /DCOPY:DAT /R:3 /W:1 /XJ
	if errorlevel 8 exit /b 1
)
if exist "%ROOT%\scripts" (
	robocopy "%ROOT%\scripts" "%DEPLOY_DIR%\scripts" /E /COPY:DAT /DCOPY:DAT /R:3 /W:1 /XJ
	if errorlevel 8 exit /b 1
)
if exist "%ROOT%\Mods" (
	robocopy "%ROOT%\Mods" "%DEPLOY_DIR%\Mods" /E /COPY:DAT /DCOPY:DAT /R:3 /W:1 /XJ
	if errorlevel 8 exit /b 1
)

echo Done. Built files are in %BUILD_DIR%\bin.
echo Deployed files are in %DEPLOY_DIR%.
exit /b 0
