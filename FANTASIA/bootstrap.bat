@echo off
setlocal EnableDelayedExpansion

REM ─────────────────────────────────────────────────────────────────────────────
REM  FANTASIA artifact fetcher (Windows)
REM
REM  Downloads, from the runtime-v2.0 GitHub Release, everything needed to run a
REM  typical FANTASIA interactive MetaHuman with as little manual setup as
REM  possible. By DEFAULT it installs EVERYTHING; use --no-* flags to opt out.
REM
REM  What it can fetch:
REM    Whisper ASR model            -> Resources/                  (~574 MB)
REM    Prebuilt GPU whisper stack   -> Binaries/Win64/             (~540 MB)
REM    Win64 build-time static libs -> ThirdParty/                 (~330 MB)
REM    NVIDIA ACE (A2F runtime)     -> NVIDIA-UE57-Bundle/         (~656 MB)
REM    NVIDIA "Mark" A2F character  -> NVIDIA-UE57-Bundle/         (~1.5 GB)
REM    NVIDIA "James" A2F character -> NVIDIA-UE57-Bundle/         (~1.5 GB)
REM    NVIDIA "Claire" A2F character-> NVIDIA-UE57-Bundle/         (~1.4 GB)
REM
REM  The NVIDIA ACE/Mark/James/Claire plugins (MIT + NVIDIA Community Model License) are
REM  fetched into NVIDIA-UE57-Bundle/ and then, if this clone sits inside a UE
REM  project's Plugins/ folder, MOVED automatically next to FANTASIA there.
REM  (If no Plugins/ ancestor is found, they're left staged with instructions.)
REM  An NVIDIA plugin already installed in that Plugins/ folder is left
REM  exactly as it is, so if this clone installed it for a DIFFERENT engine
REM  version, delete it there before this run or you keep that other line's
REM  build. The staging directory needs no such care: each plugin staged
REM  there records the release it came from, and one from another release
REM  is replaced automatically.
REM  DLSS / Streamline are NOT bundled — NVIDIA's RTX SDK license forbids
REM  standalone redistribution; install them from the Epic Marketplace if you
REM  want them (not needed for lip-sync).
REM
REM  Idempotent: artifacts already present are skipped -- core ones are
REM  size-checked, NVIDIA plugins release-checked; --force redoes.
REM
REM  Usage:
REM    bootstrap.bat                 install everything (recommended, default)
REM    bootstrap.bat --no-gpu        skip the CUDA GPU whisper stack
REM    bootstrap.bat --no-deps       skip build-time libs (Blueprint-only users)
REM    bootstrap.bat --no-ace        skip the NVIDIA ACE runtime
REM    bootstrap.bat --no-mark       skip the "Mark" character
REM    bootstrap.bat --no-james      skip the "James" character
REM    bootstrap.bat --no-claire     skip the "Claire" character
REM    bootstrap.bat --no-characters skip Mark, James and Claire
REM    bootstrap.bat --no-nvidia     skip ACE + Mark + James + Claire
REM    bootstrap.bat --minimal       Whisper model only (no GPU/deps/NVIDIA)
REM    bootstrap.bat --enable-plugins  edit the project's .uproject to enable the
REM                                    FANTASIA stack (writes a .uproject.bak first)
REM    bootstrap.bat --force         re-download even if present
REM ─────────────────────────────────────────────────────────────────────────────

set RELEASE_TAG=runtime-v2.0
set RELEASE_BASE=https://github.com/antori82/FANTASIA/releases/download/%RELEASE_TAG%
set MODEL_ASSET=whisper-large-v3-turbo-q5_0.bin
set GPU_ASSET=fantasia-gpu-cuda13-win64.tar.gz
set DEPS_ASSET=fantasia-build-deps-win64.tar.gz
set ACE_ASSET=nv-ace-reference-ue57-full.tar.gz
set MARK_ASSET=nv-audio2face-mark-ue57.tar.gz
set JAMES_ASSET=nv-audio2face-james-ue57.tar.gz
set CLAIRE_ASSET=nv-audio2face-claire-ue57.tar.gz

REM Name of the stamp bootstrap drops inside each staged NVIDIA plugin,
REM holding the release tag that produced it. The staging directory keeps the
REM same name across engine lines, so the stamp -- not the directory name --
REM is what says which line a staged plugin belongs to.
set NV_STAMP_FILE=FANTASIA-bootstrap-release.txt

REM Default: install EVERYTHING. The --no-* flags below turn parts off.
set FETCH_MODEL=1
set FETCH_GPU=1
set FETCH_DEPS=1
set FETCH_ACE=1
set FETCH_MARK=1
set FETCH_JAMES=1
set FETCH_CLAIRE=1
set FORCE=0
set ENABLE_PLUGINS=0
set PLACED=0

REM Capture the script's own directory BEFORE the arg loop. The `shift`
REM calls below shift %0 as well, after which %~dp0 would resolve against
REM the current working directory instead of the script's location --
REM sending all fetched artifacts one level too high when any flag is used.
set SCRIPT_DIR=%~dp0

REM NOTE: quoted `set "VAR=val"` is required throughout the parse loop -- an
REM unquoted `set VAR=0 & ...` captures the space before `&` INTO the value
REM ("0 "), which then fails the `=="1"` gate checks below.
:parse_args
if "%~1"=="" goto :args_done
if /I "%~1"=="--no-model"      ( set "FETCH_MODEL=0" & shift & goto :parse_args )
if /I "%~1"=="--no-gpu"        ( set "FETCH_GPU=0" & shift & goto :parse_args )
if /I "%~1"=="--no-deps"       ( set "FETCH_DEPS=0" & shift & goto :parse_args )
if /I "%~1"=="--no-ace"        ( set "FETCH_ACE=0" & shift & goto :parse_args )
if /I "%~1"=="--no-mark"       ( set "FETCH_MARK=0" & shift & goto :parse_args )
if /I "%~1"=="--no-james"      ( set "FETCH_JAMES=0" & shift & goto :parse_args )
if /I "%~1"=="--no-claire"     ( set "FETCH_CLAIRE=0" & shift & goto :parse_args )
if /I "%~1"=="--no-characters" ( set "FETCH_MARK=0" & set "FETCH_JAMES=0" & set "FETCH_CLAIRE=0" & shift & goto :parse_args )
if /I "%~1"=="--no-nvidia"     ( set "FETCH_ACE=0" & set "FETCH_MARK=0" & set "FETCH_JAMES=0" & set "FETCH_CLAIRE=0" & shift & goto :parse_args )
if /I "%~1"=="--minimal"       ( set "FETCH_GPU=0" & set "FETCH_DEPS=0" & set "FETCH_ACE=0" & set "FETCH_MARK=0" & set "FETCH_JAMES=0" & set "FETCH_CLAIRE=0" & shift & goto :parse_args )
if /I "%~1"=="--enable-plugins" ( set "ENABLE_PLUGINS=1" & shift & goto :parse_args )
if /I "%~1"=="--force"         ( set "FORCE=1" & shift & goto :parse_args )
echo Unknown argument: %~1
echo Usage: bootstrap.bat [--no-model^|--no-gpu^|--no-deps^|--no-ace^|--no-mark^|--no-james^|--no-claire^|--no-characters^|--no-nvidia^|--minimal] [--enable-plugins] [--force]
exit /b 1
:args_done

set RES_DIR=%SCRIPT_DIR%Resources
set BIN_DIR=%SCRIPT_DIR%Binaries\Win64
set TP_DIR=%SCRIPT_DIR%ThirdParty
set NV_DIR=%SCRIPT_DIR%NVIDIA-UE57-Bundle

if not exist "%RES_DIR%" mkdir "%RES_DIR%"
if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%TP_DIR%"  mkdir "%TP_DIR%"

where curl >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: curl.exe not found on PATH. Install curl or upgrade to Windows 10 1803+.
    exit /b 1
)
REM Pin tar to the Windows-bundled bsdtar at %SystemRoot%\System32\tar.exe
REM instead of relying on PATH order. Git Bash's GNU tar usually wins the
REM PATH lookup on developer machines, and GNU tar interprets `-C D:\path`
REM (any drive-letter argument) as remote `host:path` syntax -- so the
REM extraction step fails with "tar: Cannot connect to D: resolve failed".
REM bsdtar parses the same path correctly. The bundled bsdtar has been in
REM Windows 10 since build 17063 (April 2018), so this path is reliable.
set TAR=%SystemRoot%\System32\tar.exe
if not exist "%TAR%" (
    echo ERROR: %TAR% not found. Install bsdtar/libarchive or upgrade to Windows 10 17063+.
    exit /b 1
)

REM ── Fetch whisper model ──────────────────────────────────────────────────
if "%FETCH_MODEL%"=="1" goto :do_model
goto :after_model

:do_model
set MODEL_PATH=%RES_DIR%\ggml-large-v3-turbo-q5_0.bin
if "%FORCE%"=="0" if exist "%MODEL_PATH%" (
    REM Sanity-check the size: the q5_0 model is ~574 MB. Anything below
    REM 100 MB is almost certainly a partial / interrupted prior fetch.
    for %%A in ("%MODEL_PATH%") do set MODEL_SIZE=%%~zA
    if !MODEL_SIZE! GTR 104857600 (
        echo Whisper model already present at %MODEL_PATH% ^(skipping^).
        goto :after_model
    )
    echo Existing model at %MODEL_PATH% looks incomplete ^(!MODEL_SIZE! bytes^); re-downloading.
)

echo.
echo Downloading whisper model %MODEL_ASSET% from %RELEASE_TAG% ...
echo   ^(this is ~574 MB; first run takes a few minutes on a decent link^)
curl -L --fail --progress-bar -o "%MODEL_PATH%" "%RELEASE_BASE%/%MODEL_ASSET%"
if %errorlevel% neq 0 (
    echo ERROR: failed to download %MODEL_ASSET%.
    del /F /Q "%MODEL_PATH%" 2>nul
    exit /b 1
)
echo Model downloaded to %MODEL_PATH%

:after_model

REM ── Fetch GPU stack ──────────────────────────────────────────────────────
if "%FETCH_GPU%"=="1" goto :do_gpu
goto :after_gpu

:do_gpu
set GPU_MARKER=%BIN_DIR%\fantasia_whisper_cuda.dll
if "%FORCE%"=="0" if exist "%GPU_MARKER%" (
    echo GPU stack already present at %BIN_DIR% ^(skipping^).
    goto :after_gpu
)

set GPU_TGZ=%TEMP%\%GPU_ASSET%
echo.
echo Downloading GPU whisper stack %GPU_ASSET% from %RELEASE_TAG% ...
echo   ^(this is ~540 MB^)
curl -L --fail --progress-bar -o "%GPU_TGZ%" "%RELEASE_BASE%/%GPU_ASSET%"
if %errorlevel% neq 0 (
    echo ERROR: failed to download %GPU_ASSET%.
    del /F /Q "%GPU_TGZ%" 2>nul
    exit /b 1
)

echo Extracting into %BIN_DIR% ...
"%TAR%" -xf "%GPU_TGZ%" -C "%BIN_DIR%"
if %errorlevel% neq 0 (
    echo ERROR: failed to extract %GPU_ASSET%.
    exit /b 1
)
del /F /Q "%GPU_TGZ%" 2>nul
echo GPU stack staged in %BIN_DIR%

:after_gpu

REM ── Fetch build-time static libs ─────────────────────────────────────────
if "%FETCH_DEPS%"=="1" goto :do_deps
goto :after_deps

:do_deps
set DEPS_MARKER=%TP_DIR%\aGrUM\Libraries\agrumBASE.lib
if "%FORCE%"=="0" if exist "%DEPS_MARKER%" (
    echo Build-time static libs already present under %TP_DIR% ^(skipping^).
    goto :after_deps
)

set DEPS_TGZ=%TEMP%\%DEPS_ASSET%
echo.
echo Downloading Win64 build deps %DEPS_ASSET% from %RELEASE_TAG% ...
echo   ^(this is ~60 MB compressed, expands to ~330 MB^)
curl -L --fail --progress-bar -o "%DEPS_TGZ%" "%RELEASE_BASE%/%DEPS_ASSET%"
if %errorlevel% neq 0 (
    echo ERROR: failed to download %DEPS_ASSET%.
    del /F /Q "%DEPS_TGZ%" 2>nul
    exit /b 1
)

echo Extracting into %SCRIPT_DIR% ...
REM %SCRIPT_DIR% always ends with a backslash. Quoting it directly to tar
REM gives `"...\"` which tar parses as an unterminated quote. Tacking on
REM `.` resolves to the same directory while breaking the escape.
"%TAR%" -xf "%DEPS_TGZ%" -C "%SCRIPT_DIR%."
if %errorlevel% neq 0 (
    echo ERROR: failed to extract %DEPS_ASSET%.
    exit /b 1
)
del /F /Q "%DEPS_TGZ%" 2>nul
echo Build deps staged under %TP_DIR%

:after_deps

REM ── Fetch NVIDIA plugins (ACE + Mark + James) into NVIDIA-UE57-Bundle/ ────
set ANY_NV=0
if "%FETCH_ACE%"=="1"   set ANY_NV=1
if "%FETCH_MARK%"=="1"  set ANY_NV=1
if "%FETCH_JAMES%"=="1" set ANY_NV=1
if "%FETCH_CLAIRE%"=="1" set ANY_NV=1
if "%ANY_NV%"=="0" goto :done
REM Each fetch below skips a plugin only when the staged copy carries this
REM release's stamp, so this line's build replaces whatever another engine
REM line left in the staging directory. The move step further down cannot do
REM the same: a plugin already installed in the project's Plugins\ folder
REM belongs to the user, so it is left alone and only mentioned here.
echo.
echo  NOTE: an NVIDIA plugin already in your project's Plugins\ folder is left
echo        exactly as it is. If this clone installed it for a DIFFERENT engine
echo        version, delete it there before this run, or it will stay on the
echo        wrong engine version. The staging directory is checked
echo        automatically: a plugin in it from another release is replaced.
if not exist "%NV_DIR%" mkdir "%NV_DIR%"

REM -- ACE A2F runtime --
if "%FETCH_ACE%"=="1" goto :do_ace
goto :after_ace
:do_ace
call :nv_prepare NV_ACE_Reference
if errorlevel 1 exit /b 1
if "%NV_SKIP%"=="1" (
    echo NVIDIA ACE runtime already present for %RELEASE_TAG% ^(skipping^).
    goto :after_ace
)
set NV_TGZ=%TEMP%\%ACE_ASSET%
echo.
echo Downloading NVIDIA ACE A2F runtime %ACE_ASSET% ^(~656 MB^) ...
curl -L --fail --progress-bar -o "%NV_TGZ%" "%RELEASE_BASE%/%ACE_ASSET%"
if %errorlevel% neq 0 ( echo ERROR: failed to download %ACE_ASSET%. & del /F /Q "%NV_TGZ%" 2>nul & exit /b 1 )
echo Extracting into %NV_DIR% ...
"%TAR%" -xf "%NV_TGZ%" -C "%NV_DIR%"
if %errorlevel% neq 0 ( echo ERROR: failed to extract %ACE_ASSET%. & exit /b 1 )
del /F /Q "%NV_TGZ%" 2>nul
call :nv_stamp NV_ACE_Reference
echo NVIDIA ACE runtime staged.
:after_ace

REM -- "Mark" character --
if "%FETCH_MARK%"=="1" goto :do_mark
goto :after_mark
:do_mark
call :nv_prepare NvAudio2FaceMark
if errorlevel 1 exit /b 1
if "%NV_SKIP%"=="1" (
    echo NVIDIA "Mark" character already present for %RELEASE_TAG% ^(skipping^).
    goto :after_mark
)
set NV_TGZ=%TEMP%\%MARK_ASSET%
echo.
echo Downloading NVIDIA "Mark" A2F character %MARK_ASSET% ^(~1.5 GB^) ...
curl -L --fail --progress-bar -o "%NV_TGZ%" "%RELEASE_BASE%/%MARK_ASSET%"
if %errorlevel% neq 0 ( echo ERROR: failed to download %MARK_ASSET%. & del /F /Q "%NV_TGZ%" 2>nul & exit /b 1 )
echo Extracting into %NV_DIR% ...
"%TAR%" -xf "%NV_TGZ%" -C "%NV_DIR%"
if %errorlevel% neq 0 ( echo ERROR: failed to extract %MARK_ASSET%. & exit /b 1 )
del /F /Q "%NV_TGZ%" 2>nul
call :nv_stamp NvAudio2FaceMark
echo NVIDIA "Mark" character staged.
:after_mark

REM -- "James" character --
if "%FETCH_JAMES%"=="1" goto :do_james
goto :after_james
:do_james
call :nv_prepare NvAudio2FaceJames
if errorlevel 1 exit /b 1
if "%NV_SKIP%"=="1" (
    echo NVIDIA "James" character already present for %RELEASE_TAG% ^(skipping^).
    goto :after_james
)
set NV_TGZ=%TEMP%\%JAMES_ASSET%
echo.
echo Downloading NVIDIA "James" A2F character %JAMES_ASSET% ^(~1.5 GB^) ...
curl -L --fail --progress-bar -o "%NV_TGZ%" "%RELEASE_BASE%/%JAMES_ASSET%"
if %errorlevel% neq 0 ( echo ERROR: failed to download %JAMES_ASSET%. & del /F /Q "%NV_TGZ%" 2>nul & exit /b 1 )
echo Extracting into %NV_DIR% ...
"%TAR%" -xf "%NV_TGZ%" -C "%NV_DIR%"
if %errorlevel% neq 0 ( echo ERROR: failed to extract %JAMES_ASSET%. & exit /b 1 )
del /F /Q "%NV_TGZ%" 2>nul
call :nv_stamp NvAudio2FaceJames
echo NVIDIA "James" character staged.
:after_james

REM -- "Claire" character (Audio2Face-3D diffusion model) --
if "%FETCH_CLAIRE%"=="1" goto :do_claire
goto :after_claire
:do_claire
call :nv_prepare NvAudio2FaceClaire
if errorlevel 1 exit /b 1
if "%NV_SKIP%"=="1" (
    echo NVIDIA "Claire" character already present for %RELEASE_TAG% ^(skipping^).
    goto :after_claire
)
set NV_TGZ=%TEMP%\%CLAIRE_ASSET%
echo.
echo Downloading NVIDIA "Claire" A2F character %CLAIRE_ASSET% ^(~1.4 GB^) ...
curl -L --fail --progress-bar -o "%NV_TGZ%" "%RELEASE_BASE%/%CLAIRE_ASSET%"
if %errorlevel% neq 0 ( echo ERROR: failed to download %CLAIRE_ASSET%. & del /F /Q "%NV_TGZ%" 2>nul & exit /b 1 )
echo Extracting into %NV_DIR% ...
"%TAR%" -xf "%NV_TGZ%" -C "%NV_DIR%"
if %errorlevel% neq 0 ( echo ERROR: failed to extract %CLAIRE_ASSET%. & exit /b 1 )
del /F /Q "%NV_TGZ%" 2>nul
call :nv_stamp NvAudio2FaceClaire
echo NVIDIA "Claire" character staged.
:after_claire

REM ── Auto-place the NVIDIA plugins into the project's Plugins/ folder ──────
REM Walk up from SCRIPT_DIR to the nearest ancestor named "Plugins" (UE plugins
REM always live under one). If found, move the fetched NVIDIA plugins there as
REM siblings of FANTASIA; otherwise leave them staged in NV_DIR with a manual
REM note. PowerShell does the ancestor walk (robust vs. batch path math).
set PLUGINS_DIR=
for /f "usebackq delims=" %%P in (`powershell -NoProfile -Command "$d=[System.IO.DirectoryInfo]'%SCRIPT_DIR%'; while($d -ne $null){ if($d.Name -ieq 'Plugins'){ $d.FullName; break }; $d=$d.Parent }"`) do set "PLUGINS_DIR=%%P"

if not defined PLUGINS_DIR goto :nv_instructions

echo.
echo Installing NVIDIA plugins into %PLUGINS_DIR% ...
for %%G in (NV_ACE_Reference NvAudio2FaceMark NvAudio2FaceJames NvAudio2FaceClaire) do (
    if exist "%NV_DIR%\%%G\" (
        if exist "%PLUGINS_DIR%\%%G\" (
            echo   %%G already in Plugins ^(leaving the existing copy in place^).
        ) else (
            move "%NV_DIR%\%%G" "%PLUGINS_DIR%\" >nul
            if errorlevel 1 ( echo   WARNING: could not move %%G; it remains in %NV_DIR% ) else ( echo   installed %%G )
        )
    )
)
set PLACED=1
REM Remove the staging dir if it's now empty.
rmdir "%NV_DIR%" 2>nul

if "%ENABLE_PLUGINS%"=="1" (
    echo.
    echo Enabling the FANTASIA stack in the project .uproject ...
    powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%enable-plugins.ps1" -PluginsDir "%PLUGINS_DIR%"
)

:nv_instructions
echo.
if "%PLACED%"=="1" goto :instr_auto
REM -- staged (no Plugins/ ancestor found) --
echo  [Interactive MetaHuman setup — manual steps]
echo    1. Move the plugin folders from:
echo         %NV_DIR%
echo       into your UE project's Plugins\ folder (alongside FANTASIA).
echo    2. Enable FANTASIA, FANTASIAACE, NV_ACE_Reference (+ the character plugins).
goto :instr_common
:instr_auto
echo  [Interactive MetaHuman setup]
echo    NVIDIA plugins installed into your Plugins\ folder.
if "%ENABLE_PLUGINS%"=="1"     echo    The FANTASIA stack was enabled in your .uproject (backup: *.uproject.bak).
if not "%ENABLE_PLUGINS%"=="1" echo    Enable FANTASIA, FANTASIAACE, NV_ACE_Reference (+ characters) in the
if not "%ENABLE_PLUGINS%"=="1" echo    editor's Plugins panel, or re-run with --enable-plugins.
:instr_common
echo    - "Mark" / "James" / "Claire" are ready-to-use A2F characters; or import
echo      your own MetaHuman via MetaHuman Creator / Fab / Quixel Bridge.
echo    - (Optional) DLSS / Streamline performance: install from the Epic
echo      Marketplace (NOT bundled; not required for lip-sync).
echo    - Drive the face with UACERESTTTSComponent / UACEElevenLabsTTSComponent
echo      (from FANTASIAACE).
echo    Licensing + provenance: NV_ACE_Reference\README-FANTASIA-UE5.7-bundle.txt

:done
echo.
echo ─────────────────────────────────────────────────
echo  Bootstrap complete.
echo ─────────────────────────────────────────────────
echo  CPU users: nothing else needed; the plugin is ready.
echo  GPU users: set UWhisperSubsystem.Backend = GPU on the
echo  subsystem and reopen the editor. The runtime dispatcher
echo  will load fantasia_whisper_cuda.dll automatically.
exit /b 0

REM ── Subroutines ────────────────────────────────────────────────────────

REM  :nv_prepare <PluginFolder>
REM  Answers the question each NVIDIA fetch has to ask: is the plugin for
REM  THIS release already extracted in the staging directory?
REM     NV_SKIP=1  yes -- this release's build is staged, skip the download
REM     NV_SKIP=0  no  -- download it
REM  A staged plugin is identified by the stamp beside it, never by the
REM  staging directory's name: that name is fixed across engine lines, so a
REM  plugin left there by another line carries no matching stamp, reads as
REM  "from another release", and is deleted before this release's build is
REM  extracted in its place. Only the staging directory -- which bootstrap
REM  creates and git ignores -- is ever deleted here; a plugin the move step
REM  has already installed into the project's Plugins\ folder is the user's,
REM  and is never touched.
REM  Exits 1 when a plugin from another release cannot be removed, so that
REM  the caller stops instead of extracting onto a half-deleted tree.
:nv_prepare
set "NV_SKIP=0"
if not exist "%NV_DIR%\%~1\" exit /b 0
set "NV_STAMP="
if exist "%NV_DIR%\%~1\%NV_STAMP_FILE%" set /p NV_STAMP=<"%NV_DIR%\%~1\%NV_STAMP_FILE%"
if /I not "!NV_STAMP!"=="%RELEASE_TAG%" goto :nv_replace
if not exist "%NV_DIR%\%~1\%~1.uplugin" goto :nv_replace
if "%FORCE%"=="0" set "NV_SKIP=1"
exit /b 0

:nv_replace
echo.
echo  This staged NVIDIA plugin is not from %RELEASE_TAG% ^(another engine
echo  line left it there^), so this release's build replaces it:
echo      %NV_DIR%\%~1
rmdir /S /Q "%NV_DIR%\%~1" 2>nul
if not exist "%NV_DIR%\%~1\" exit /b 0
echo.
echo  ERROR: this directory holds an NVIDIA plugin from another release and
echo         could not be removed:
echo             %NV_DIR%\%~1
echo         Something is holding a file open in it. Close the editor and
echo         anything browsing that folder, delete that one directory by
echo         hand, then run bootstrap.bat again.
exit /b 1

REM  :nv_stamp <PluginFolder>
REM  Records the release that populated a freshly extracted plugin. Written
REM  only after the extraction succeeded, so an interrupted run leaves no
REM  stamp and the next run replaces what it staged. The redirection comes
REM  before `echo` on purpose: `echo %RELEASE_TAG%>file` would read the tag's
REM  trailing digit as a stream number and write a truncated tag.
:nv_stamp
>"%NV_DIR%\%~1\%NV_STAMP_FILE%" echo %RELEASE_TAG%
exit /b 0
