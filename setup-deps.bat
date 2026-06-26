@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "ROOT=%ROOT:~0,-1%"
set "BUILD_CONFIG=Release"
set "CONAN_OUT=%ROOT%\conan-msvc"
set "DEPS_DIR=%ROOT%\build-deps"
set "TOOLS_DIR=%DEPS_DIR%\tools"
set "PY_VENV=%DEPS_DIR%\python"
set "PIP_CACHE_DIR=%DEPS_DIR%\pip-cache"
set "CONAN_HOME=%DEPS_DIR%\conan-home"
set "CONAN_EXE=%PY_VENV%\Scripts\conan.exe"
set "CONAN_PYTHON=%PY_VENV%\Scripts\python.exe"
set "DEPS_ARCHIVE=%USERPROFILE%\Downloads\dependencies-windows-x64.txz"
set "DEPS_RESTORE_MARKER=%DEPS_DIR%\dependencies-windows-x64.restored"
set "CONAN_DEFAULT_PROFILE=%CONAN_HOME%\profiles\default"

cd /d "%ROOT%" || exit /b 1
set "PATH=%TOOLS_DIR%;%PATH%"

where python >nul 2>nul
if not errorlevel 1 (
	set "PYTHON_CMD=python"
) else (
	where py >nul 2>nul
	if errorlevel 1 (
	echo Python was not found on PATH.
	exit /b 1
	)
	set "PYTHON_CMD=py -3"
)

if not exist "%CONAN_EXE%" (
	echo Creating local Python environment in %PY_VENV%...
	if not exist "%DEPS_DIR%" mkdir "%DEPS_DIR%" || exit /b 1
	%PYTHON_CMD% -m venv "%PY_VENV%" || exit /b 1
	echo Installing Conan into local build-deps environment...
	"%PY_VENV%\Scripts\python.exe" -m pip install --cache-dir "%PIP_CACHE_DIR%" --upgrade pip || exit /b 1
	"%PY_VENV%\Scripts\python.exe" -m pip install --cache-dir "%PIP_CACHE_DIR%" conan || exit /b 1
)

if not exist "%CONAN_DEFAULT_PROFILE%" (
	echo Detecting Conan profile...
	"%CONAN_PYTHON%" -m conan profile detect --force || exit /b 1
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
	"%CONAN_PYTHON%" -m conan cache restore "%DEPS_ARCHIVE%" || exit /b 1
	echo restored>"%DEPS_RESTORE_MARKER%"
) else (
	echo Prebuilt Conan dependencies already restored.
)

echo Generating Conan toolchain...
"%CONAN_PYTHON%" -m conan install "%ROOT%" ^
	--output-folder="%CONAN_OUT%" ^
	--build=never ^
	--profile="dependencies\conan_profiles\msvc-x64" ^
	-c "tools.cmake.cmaketoolchain:generator=Ninja" ^
	-c "tools.microsoft.msbuild:vs_version=17" ^
	-s "&:compiler.version=192" ^
	-s "&:build_type=%BUILD_CONFIG%" ^
	-o "&:target_pre_windows10=True" || exit /b 1

echo Done. Dependencies and Conan toolchain are ready.
exit /b 0
