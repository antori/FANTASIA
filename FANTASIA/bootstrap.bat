@echo off
setlocal EnableDelayedExpansion

REM ─────────────────────────────────────────────────────────────────────────────
REM  FANTASIA artifact fetcher (Windows)
REM
REM  Downloads, from the runtime-v3.0 GitHub Release, everything the FANTASIA
REM  core plugin needs. The UE 5.8 line ships CORE-ONLY: by default this
REM  installs the three core artifacts and nothing else.
REM
REM  Installed by default (core):
REM    Whisper ASR model            -> Resources/                  (~574 MB)
REM    Prebuilt GPU whisper stack   -> Binaries/Win64/             (~540 MB)
REM    Win64 build-time static libs -> ThirdParty/                 (~330 MB)
REM
REM  Opt-in (NVIDIA interactive-MetaHuman lip-sync, NOT part of this line):
REM    NVIDIA ACE (A2F runtime)     -> NVIDIA-UE57-Bundle/         (~656 MB)
REM    NVIDIA "Mark" A2F character  -> NVIDIA-UE57-Bundle/         (~1.5 GB)
REM    NVIDIA "James" A2F character -> NVIDIA-UE57-Bundle/         (~1.5 GB)
REM    NVIDIA "Claire" A2F character-> NVIDIA-UE57-Bundle/         (~1.4 GB)
REM
REM  The NVIDIA bundles have not been rebuilt for UE 5.8 yet, so they are not
REM  published in %RELEASE_TAG% and asking for them is refused with an
REM  explanation (see NVIDIA_AVAILABLE below). They remain available on the
REM  UE 5.7 line (the `5.7` branch, runtime-v2.0).
REM
REM  When they are available, the NVIDIA ACE/Mark/James/Claire plugins (MIT +
REM  NVIDIA Community Model License) are fetched into NVIDIA-UE57-Bundle/ and
REM  then, if this clone sits inside a UE project's Plugins/ folder, MOVED
REM  automatically next to FANTASIA there.
REM  (If no Plugins/ ancestor is found, they're left staged with instructions.)
REM  DLSS / Streamline are NOT bundled — NVIDIA's RTX SDK license forbids
REM  standalone redistribution; install them from the Epic Marketplace if you
REM  want them (not needed for lip-sync).
REM
REM  Idempotent: files already present are skipped (size-checked); --force redoes.
REM
REM  Usage:
REM    bootstrap.bat                 install the core artifacts (default)
REM    bootstrap.bat --no-gpu        skip the CUDA GPU whisper stack
REM    bootstrap.bat --no-deps       skip build-time libs (Blueprint-only users)
REM    bootstrap.bat --minimal       Whisper model only (no GPU, no deps)
REM    bootstrap.bat --ace           also fetch the NVIDIA ACE runtime
REM    bootstrap.bat --mark          also fetch the "Mark" character
REM    bootstrap.bat --james         also fetch the "James" character
REM    bootstrap.bat --claire        also fetch the "Claire" character
REM    bootstrap.bat --characters    also fetch Mark, James and Claire
REM    bootstrap.bat --nvidia        also fetch ACE + Mark + James + Claire
REM    ( --no-ace / --no-mark / --no-james / --no-claire / --no-characters /
REM      --no-nvidia are still accepted, for old command lines, and are
REM      no-ops now that the NVIDIA bundles are opt-in. )
REM    bootstrap.bat --enable-plugins  edit the project's .uproject to enable the
REM                                    FANTASIA stack (writes a .uproject.bak first)
REM    bootstrap.bat --force         re-download even if present
REM ─────────────────────────────────────────────────────────────────────────────

set RELEASE_TAG=runtime-v3.0
set RELEASE_BASE=https://github.com/antori82/FANTASIA/releases/download/%RELEASE_TAG%
set MODEL_ASSET=whisper-large-v3-turbo-q5_0.bin
set GPU_ASSET=fantasia-gpu-cuda13-win64.tar.gz
set DEPS_ASSET=fantasia-build-deps-win64.tar.gz
set ACE_ASSET=nv-ace-reference-ue58-full.tar.gz
set MARK_ASSET=nv-audio2face-mark-ue58.tar.gz
set JAMES_ASSET=nv-audio2face-james-ue58.tar.gz
set CLAIRE_ASSET=nv-audio2face-claire-ue58.tar.gz

REM Are the NVIDIA bundles published in %RELEASE_TAG% for this engine line?
REM They are rebuilt by hand for every engine version (NVIDIA ships nothing
REM past UE 5.6), and the UE 5.8 rebuild does not exist yet, so runtime-v3.0
REM carries the three core assets only. Flip this to 1 in the same commit that
REM publishes nv-ace-reference-ue58-full.tar.gz and the three
REM nv-audio2face-*-ue58.tar.gz bundles; everything below is already wired.
set NVIDIA_AVAILABLE=0

REM Default: the core artifacts. The --no-* flags turn parts of the core off;
REM the NVIDIA bundles are opt-in via --ace/--mark/--james/--claire/--nvidia.
set FETCH_MODEL=1
set FETCH_GPU=1
set FETCH_DEPS=1
set FETCH_ACE=0
set FETCH_MARK=0
set FETCH_JAMES=0
set FETCH_CLAIRE=0
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
if /I "%~1"=="--ace"           ( set "FETCH_ACE=1" & shift & goto :parse_args )
if /I "%~1"=="--mark"          ( set "FETCH_MARK=1" & shift & goto :parse_args )
if /I "%~1"=="--james"         ( set "FETCH_JAMES=1" & shift & goto :parse_args )
if /I "%~1"=="--claire"        ( set "FETCH_CLAIRE=1" & shift & goto :parse_args )
if /I "%~1"=="--characters"    ( set "FETCH_MARK=1" & set "FETCH_JAMES=1" & set "FETCH_CLAIRE=1" & shift & goto :parse_args )
if /I "%~1"=="--nvidia"        ( set "FETCH_ACE=1" & set "FETCH_MARK=1" & set "FETCH_JAMES=1" & set "FETCH_CLAIRE=1" & shift & goto :parse_args )
REM The NVIDIA bundles are opt-in, so every --no-* form below is already the
REM default. Accepted so that command lines and docs written for the opt-out
REM era keep working.
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
echo Usage: bootstrap.bat [--no-model^|--no-gpu^|--no-deps^|--minimal] [--ace^|--mark^|--james^|--claire^|--characters^|--nvidia] [--enable-plugins] [--force]
exit /b 1
:args_done

REM ── Refuse an NVIDIA request up front, before downloading anything ───────
REM The bundles are requested explicitly, so a clear refusal now beats a
REM 404 from curl after a gigabyte of core artifacts has landed.
if "%NVIDIA_AVAILABLE%"=="1" goto :nvidia_ok
set ANY_NV_REQ=0
if "%FETCH_ACE%"=="1"    set ANY_NV_REQ=1
if "%FETCH_MARK%"=="1"   set ANY_NV_REQ=1
if "%FETCH_JAMES%"=="1"  set ANY_NV_REQ=1
if "%FETCH_CLAIRE%"=="1" set ANY_NV_REQ=1
if "%ANY_NV_REQ%"=="0" goto :nvidia_ok
echo.
echo  The NVIDIA ACE / Mark / James / Claire bundles are NOT available on this
echo  line, so nothing was downloaded.
echo.
echo  Why: NVIDIA publishes its ACE plugins for UE 5.6 only, so every later
echo  engine version needs a hand-port. The UE 5.8 port does not exist yet,
echo  and %RELEASE_TAG% therefore carries the three core assets only.
echo.
echo  What you can do:
echo    - Run bootstrap.bat with no NVIDIA flag for a complete core install.
echo      Core FANTASIA needs none of these: ASR, LLM, TTS, Neo4j, Prolog and
echo      the Bayesian stack all work without them. Only Audio2Face lip-sync
echo      (the FANTASIAACE companion plugin) does, and FANTASIAACE is not
echo      available for UE 5.8 either.
echo    - For lip-sync today, use the UE 5.7 line: the `5.7` branch of this
echo      repository, whose bootstrap fetches the bundles from runtime-v2.0.
exit /b 1
:nvidia_ok

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
REM A core-only run still needs the steps after the fetches: --enable-plugins
REM switches the FANTASIA stack on in the .uproject whether or not any NVIDIA
REM bundle was asked for.
if "%ANY_NV%"=="0" goto :nv_fetch_done
if not exist "%NV_DIR%" mkdir "%NV_DIR%"

REM -- ACE A2F runtime --
if "%FETCH_ACE%"=="1" goto :do_ace
goto :after_ace
:do_ace
if "%FORCE%"=="0" if exist "%NV_DIR%\NV_ACE_Reference\NV_ACE_Reference.uplugin" (
    echo NVIDIA ACE runtime already present ^(skipping^).
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
echo NVIDIA ACE runtime staged.
:after_ace

REM -- "Mark" character --
if "%FETCH_MARK%"=="1" goto :do_mark
goto :after_mark
:do_mark
if "%FORCE%"=="0" if exist "%NV_DIR%\NvAudio2FaceMark\NvAudio2FaceMark.uplugin" (
    echo NVIDIA "Mark" character already present ^(skipping^).
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
echo NVIDIA "Mark" character staged.
:after_mark

REM -- "James" character --
if "%FETCH_JAMES%"=="1" goto :do_james
goto :after_james
:do_james
if "%FORCE%"=="0" if exist "%NV_DIR%\NvAudio2FaceJames\NvAudio2FaceJames.uplugin" (
    echo NVIDIA "James" character already present ^(skipping^).
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
echo NVIDIA "James" character staged.
:after_james

REM -- "Claire" character (Audio2Face-3D diffusion model) --
if "%FETCH_CLAIRE%"=="1" goto :do_claire
goto :after_claire
:do_claire
if "%FORCE%"=="0" if exist "%NV_DIR%\NvAudio2FaceClaire\NvAudio2FaceClaire.uplugin" (
    echo NVIDIA "Claire" character already present ^(skipping^).
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
echo NVIDIA "Claire" character staged.
:after_claire
:nv_fetch_done

REM ── Locate the project's Plugins/ folder ─────────────────────────────────
REM Walk up from SCRIPT_DIR to the nearest ancestor named "Plugins" (UE plugins
REM always live under one). If found, move any fetched NVIDIA plugins there as
REM siblings of FANTASIA; otherwise leave them staged in NV_DIR with a manual
REM note. PowerShell does the ancestor walk (robust vs. batch path math).
set PLUGINS_DIR=
for /f "usebackq delims=" %%P in (`powershell -NoProfile -Command "$d=[System.IO.DirectoryInfo]'%SCRIPT_DIR%'; while($d -ne $null){ if($d.Name -ieq 'Plugins'){ $d.FullName; break }; $d=$d.Parent }"`) do set "PLUGINS_DIR=%%P"

if "%ANY_NV%"=="0" goto :after_place
if not defined PLUGINS_DIR goto :after_place

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

:after_place
if not "%ENABLE_PLUGINS%"=="1" goto :after_enable
if not defined PLUGINS_DIR (
    echo.
    echo WARNING: --enable-plugins needs this clone to sit inside a UE project's
    echo          Plugins\ folder; no such ancestor was found, so no .uproject
    echo          was edited.
    goto :after_enable
)
echo.
echo Enabling the FANTASIA stack in the project .uproject ...
powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%enable-plugins.ps1" -PluginsDir "%PLUGINS_DIR%"
:after_enable

REM A core-only run has no MetaHuman steps left to explain.
if "%ANY_NV%"=="0" goto :done

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
