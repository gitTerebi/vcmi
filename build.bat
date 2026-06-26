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
set "VS_NINJA_DIR=%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja"
set "VS_VCVARSALL=%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set "PY_VENV=%DEPS_DIR%\python"
set "PIP_CACHE_DIR=%DEPS_DIR%\pip-cache"
set "CONAN_HOME=%DEPS_DIR%\conan-home"
set "CONAN_EXE=%PY_VENV%\Scripts\conan.exe"
set "DEPS_ARCHIVE=%USERPROFILE%\Downloads\dependencies-windows-x64.txz"
set "DEPS_RESTORE_MARKER=%DEPS_DIR%\dependencies-windows-x64.restored"
set "CONAN_DEFAULT_PROFILE=%CONAN_HOME%\profiles\default"

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

where python >nul 2>nul
if errorlevel 1 (
	echo Python was not found on PATH.
	exit /b 1
)

if not exist "%CONAN_EXE%" (
	echo Creating local Python environment in %PY_VENV%...
	if not exist "%DEPS_DIR%" mkdir "%DEPS_DIR%" || exit /b 1
	python -m venv "%PY_VENV%" || exit /b 1
	echo Installing Conan into local build-deps environment...
	"%PY_VENV%\Scripts\python.exe" -m pip install --cache-dir "%PIP_CACHE_DIR%" --upgrade pip || exit /b 1
	"%PY_VENV%\Scripts\python.exe" -m pip install --cache-dir "%PIP_CACHE_DIR%" conan || exit /b 1
)

if not exist "%CONAN_DEFAULT_PROFILE%" (
	echo Detecting Conan profile...
	"%CONAN_EXE%" profile detect --force || exit /b 1
) else (
	echo Conan profile already exists.
)

if not exist "%DEPS_ARCHIVE%" (
	echo Missing prebuilt dependency archive:
	echo   %DEPS_ARCHIVE%
	echo Download dependencies-windows-x64.txz from:
	echo   https://github.com/vcmi/vcmi-dependencies/releases
	exit /b 1
)

if not exist "%DEPS_RESTORE_MARKER%" (
	echo Restoring prebuilt Conan dependencies...
	"%CONAN_EXE%" cache restore "%DEPS_ARCHIVE%" || exit /b 1
	echo restored>"%DEPS_RESTORE_MARKER%"
) else (
	echo Prebuilt Conan dependencies already restored.
)

echo Generating Conan toolchain...
"%CONAN_EXE%" install "%ROOT%" ^
	--output-folder="%CONAN_OUT%" ^
	--build=never ^
	--profile="dependencies\conan_profiles\msvc-x64" ^
	-c "tools.cmake.cmaketoolchain:generator=Ninja" ^
	-c "tools.microsoft.msbuild:vs_version=17" ^
	-s "&:compiler.version=192" ^
	-s "&:build_type=%BUILD_CONFIG%" ^
	-o "&:target_pre_windows10=True" || exit /b 1

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

if exist "%BUILD_DIR%\CMakeCache.txt" (
	findstr /c:"CMAKE_GENERATOR_PLATFORM" "%BUILD_DIR%\CMakeCache.txt" >nul 2>nul
	if not errorlevel 1 (
		echo Removing stale Ninja configure cache with Visual Studio platform settings...
		del "%BUILD_DIR%\CMakeCache.txt" || exit /b 1
		if exist "%BUILD_DIR%\CMakeFiles" rmdir /s /q "%BUILD_DIR%\CMakeFiles" || exit /b 1
	)
)

echo Configuring Ninja build...
cmake -S "%ROOT%" -B "%BUILD_DIR%" -G Ninja --toolchain "%CONAN_OUT%\conan_toolchain.cmake" -D CMAKE_BUILD_TYPE=%BUILD_CONFIG% -D CMAKE_C_COMPILER=cl -D CMAKE_CXX_COMPILER=cl -D ENABLE_CCACHE=ON || exit /b 1

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

echo Done. Built files are in %BUILD_DIR%\bin.
echo Deployed files are in %DEPLOY_DIR%.
exit /b 0
