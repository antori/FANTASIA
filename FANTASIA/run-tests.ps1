<#
.SYNOPSIS
    FANTASIA's mechanical gate: compile the plugin and run its automation suite
    against an installed engine, then say pass or fail.

.DESCRIPTION
    This repository is a plugin and holds no .uproject, so the suite cannot be run
    from it as it stands. The gate therefore brings its own host project (ADR-0016):
    it generates, and then reuses, a minimal code project outside this repository's
    working tree, reaches the plugin from there, and enables FANTASIA only.

    It reaches the plugin through a directory junction rather than through
    AdditionalPluginDirectories, which is the one place this script departs from
    ADR-0016's wording. The reason is in the comment beside the junction: an external
    plugin's modules are linked into the host project while the editor still loads them
    from the plugin's own Binaries, so the suite would have run the committed prebuilt
    DLLs and never the code just compiled.

    FANTASIAACE is outside this gate's reach on purpose. Its 5.8 binaries do not exist
    (F-016), its NVIDIA dependency has no 5.8 build (F-014), and it has no test module,
    so the junction names the FANTASIA plugin alone and the harness cannot see ACE at
    all, rather than appearing to cover it.

    The exit code comes from the exported report, never from the editor's exit status:
    UnrealEditor-Cmd.exe exits zero in cases that are not successes.

    One gate run at a time, and the report must be this run's own (F-035). The gate
    holds an exclusive lock on the harness for its whole length, so a second run refuses
    with exit 11 instead of racing: the two runs would otherwise share the harness, the
    plugin's Intermediate and the tracked Binaries that the link step rewrites, and
    isolating only the report would hide that corruption rather than remove it. The lock
    is a file handle, so it cannot go stale -- the operating system releases it when the
    holding process ends, however it ends.

    The lock makes a race impossible between two runs of this script; it cannot make the
    report trustworthy on its own, because a report can also be left behind by a run that
    was killed, or written by an editor someone started by hand. So the report's
    provenance is proved rather than arranged, to the same standard the module-load guard
    holds the DLL to: the report names the instance that wrote it as '<computer>-<pid>',
    and the gate requires that pid to be the editor it started itself. The report and log
    paths carry this run's id as well, which keeps runs out of each other's evidence, but
    the verdict does not rest on that separation. A run also clears every earlier run's
    report and log as it starts, so the run that finished last is the only one that has
    left any: read a failed run's log before running the gate again.

.PARAMETER Tests
    The automation test prefix to run. Default 'FANTASIA', the whole suite.

.PARAMETER MinTests
    The run fails if the report names fewer tests than this. Default 134, the floor the
    suite cannot fall below in any supported build configuration: 150 tests run today
    (152 are declared, of which two are shadowed by being internal nodes of the
    automation tree as well -- F-037), and 16 of those 150 are compiled only when the
    aGrUM binding is (WITH_AGRUM_BINDING, all of BayesianNetworkTests.cpp and
    InfluenceDiagTests.cpp), which is the only conditional compilation in the suite.
    The floor is therefore 150 - 16. It does not assume F-037 stays unfixed: fixing it
    raises the count to 152 and a floor is only a lower bound.

.OUTPUTS
    Exit 0 only when the compile succeeded, the emitted BuildId matches the engine, the
    exported report was stamped by the editor this run started, and that report names at
    least MinTests tests with no failure and nothing not run. Every other outcome exits
    non-zero and says which:
      1 no engine resolved, or its version is not the plugin's
      2 the compile failed
      3 no report, or a report another editor wrote
      5 the report names failures, or tests that did not run
      6 the report names fewer tests than MinTests, zero included
      7 the emitted .modules BuildId is not the engine's CompatibleChangelist
      8 the script was called with an argument it cannot use
      9 the plugin's Intermediate directory is not writable (a sandboxed shell), so the
        compile would fail misleadingly inside cl.exe rather than honestly here
     10 the editor loaded the test module from somewhere this run did not compile into,
        so the suite did not test the code that was just built
     11 another gate run holds the harness
    Code 4 is retired. It meant 'the report names zero tests', and no run can reach it:
    when the prefix matches nothing the editor exits without exporting a report at all,
    which is exit 3. A zero-test report, if the engine ever wrote one, is fewer tests
    than MinTests and so is exit 6. The numbers of the other codes are left alone,
    because they are quoted outside this script.
#>

[CmdletBinding()]
param(
    [string] $Tests = 'FANTASIA',
    [int]    $MinTests = 134
)

$ErrorActionPreference = 'Stop'
$started = [Diagnostics.Stopwatch]::StartNew()

$DefaultEngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
$HarnessName = 'FantasiaGate'

function Write-Step([string] $Message) {
    Write-Host "[gate] $Message"
}

# The handle on the harness lock, held for the whole run. The operating system closes it
# when this process ends, so the lock can never be left stale; the file is deleted too,
# only to keep a dead holder's identity from being quoted at the next run.
$script:GateLock = $null

function Remove-GateLock {
    if ($null -eq $script:GateLock) { return }
    $path = $script:GateLock.Name
    $script:GateLock.Dispose()
    $script:GateLock = $null
    # Deleting the file is tidiness, not the release: the handle above was the lock, so a
    # file that outlives this process locks nothing and the next run overwrites it.
    try { Remove-Item -LiteralPath $path -Force -ErrorAction Stop } catch { }
}

function Stop-Gate([int] $Code, [string] $Message) {
    Remove-GateLock
    Write-Host ''
    Write-Host "[gate] FAILED ($Code): $Message"
    Write-Host ("[gate] elapsed {0:n1} s" -f $started.Elapsed.TotalSeconds)
    exit $Code
}

# Writes $Content to $Path only when it differs, so a warm run does not touch a
# timestamp UnrealBuildTool watches.
function Set-FileIfChanged([string] $Path, [string] $Content) {
    if (Test-Path -LiteralPath $Path) {
        $current = [IO.File]::ReadAllText($Path)
        if ($current -eq $Content) { return }
    }
    $parent = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    [IO.File]::WriteAllText($Path, $Content)
}

# The newest write time among the plugin's tracked module binaries, as ticks, or 0 when
# none exist yet. Used to tell the maintainer whether a run touched them (P-12).
# Measure-Object -Maximum takes only numeric input on Windows PowerShell 5.1, which is
# why this compares ticks rather than the DateTime objects themselves.
function Get-BinariesStamp([string] $Directory) {
    if (-not (Test-Path -LiteralPath $Directory)) { return [long] 0 }
    $newest = [long] 0
    foreach ($file in (Get-ChildItem -LiteralPath $Directory -File)) {
        if ($file.Name -like 'UnrealEditor-FANTASIA*.dll' -or $file.Extension -eq '.modules') {
            if ($file.LastWriteTimeUtc.Ticks -gt $newest) { $newest = $file.LastWriteTimeUtc.Ticks }
        }
    }
    return $newest
}

# ---------------------------------------------------------------- arguments ----

if ($Tests -notmatch '^[A-Za-z0-9_.]+$') {
    Stop-Gate 8 "-Tests must be a bare automation prefix (letters, digits, '_' and '.'); got '$Tests'."
}
if ($MinTests -lt 1) {
    Stop-Gate 8 "-MinTests must be at least 1; got $MinTests."
}

# ------------------------------------------------------------------- layout ----

$PluginDir = $PSScriptRoot
$RepoRoot = Split-Path -Parent $PluginDir
$PluginDescriptor = Join-Path $PluginDir 'FANTASIA.uplugin'
$AceDescriptor = Join-Path $RepoRoot 'FANTASIAACE\FANTASIAACE.uplugin'
$BinariesDir = Join-Path $PluginDir 'Binaries\Win64'

if (-not (Test-Path -LiteralPath $PluginDescriptor)) {
    Stop-Gate 8 "no FANTASIA.uplugin beside this script: expected $PluginDescriptor."
}

# ------------------------------------------------------------------- engine ----

$engineRoot = $env:FANTASIA_UE_ROOT
$engineSource = 'FANTASIA_UE_ROOT'
if ([string]::IsNullOrWhiteSpace($engineRoot)) {
    $engineRoot = $DefaultEngineRoot
    $engineSource = 'the documented default install path (FANTASIA_UE_ROOT is unset)'
}
$engineRoot = $engineRoot.TrimEnd('\', '/')

$versionFile = Join-Path $engineRoot 'Engine\Build\Build.version'
$editorExe = Join-Path $engineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$buildBat = Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'

if (-not (Test-Path -LiteralPath $versionFile)) {
    Stop-Gate 1 "no engine at '$engineRoot', read from ${engineSource}: '$versionFile' does not exist."
}
if (-not (Test-Path -LiteralPath $editorExe)) {
    Stop-Gate 1 "engine at '$engineRoot', read from $engineSource, has no editor: '$editorExe' does not exist."
}
if (-not (Test-Path -LiteralPath $buildBat)) {
    Stop-Gate 1 "engine at '$engineRoot', read from $engineSource, has no build tool: '$buildBat' does not exist."
}

$engineVersion = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
$engineLine = "$($engineVersion.MajorVersion).$($engineVersion.MinorVersion)"

# The version the plugin descriptors name is the version the gate demands. Both
# descriptors must agree, because the .uplugin EngineVersion and the committed
# .modules BuildId describe the same engine (C-build).
$required = (Get-Content -LiteralPath $PluginDescriptor -Raw | ConvertFrom-Json).EngineVersion
if (Test-Path -LiteralPath $AceDescriptor) {
    $aceRequired = (Get-Content -LiteralPath $AceDescriptor -Raw | ConvertFrom-Json).EngineVersion
    if ($aceRequired -ne $required) {
        Stop-Gate 1 "the two plugin descriptors name different engines: FANTASIA.uplugin says '$required', FANTASIAACE.uplugin says '$aceRequired'."
    }
}
$requiredParts = $required.Split('.')
$requiredLine = "$($requiredParts[0]).$($requiredParts[1])"

if ($engineLine -ne $requiredLine) {
    Stop-Gate 1 "engine version mismatch: the descriptors name $required (line $requiredLine), the engine at '$engineRoot', read from $engineSource, is $engineLine (Build.version says $($engineVersion.MajorVersion).$($engineVersion.MinorVersion).$($engineVersion.PatchVersion)). Nothing was compiled."
}

$expectedBuildId = $engineVersion.CompatibleChangelist
if ($engineVersion.IsLicenseeVersion -eq 1 -or $expectedBuildId -eq 0) {
    $expectedBuildId = $engineVersion.Changelist
}

Write-Step "engine $engineLine at '$engineRoot' (from $engineSource), BuildId $expectedBuildId"
Write-Step "FANTASIA only; FANTASIAACE is not reachable from this harness, so the gate says nothing about it (F-014, F-016: no 5.8 binaries, no 5.8 NVIDIA dependency, no test module)"

# ------------------------------------------------------------------ harness ----
# Outside this repository's working tree, so a gate run never dirties anything but
# the binaries a compile produces.

$harnessRoot = Join-Path $env:LOCALAPPDATA "FANTASIA-Gate\$HarnessName"
$uproject = Join-Path $harnessRoot "$HarnessName.uproject"
$sourceDir = Join-Path $harnessRoot 'Source'

if (-not (Test-Path -LiteralPath $harnessRoot)) {
    New-Item -ItemType Directory -Path $harnessRoot -Force | Out-Null
}

# One gate run at a time (F-035). Two runs share the harness, the plugin's Intermediate
# and the tracked Binaries the link step rewrites, so the second refuses here rather
# than corrupting the first's compile or reading its report.
#
# The lock is this file handle, not the file's existence: it is opened for writing and
# shared for reading only, so a second run's identical open is denied while a reader can
# still see who holds it. Nothing has to clean it up, because the operating system closes
# the handle whenever this process ends, crash included.
$lockFile = Join-Path $harnessRoot 'gate.lock'
try {
    $script:GateLock = [IO.File]::Open(
        $lockFile, [IO.FileMode]::Create, [IO.FileAccess]::Write, [IO.FileShare]::Read)
}
catch {
    $holder = 'unknown (the lock file could not be read)'
    try {
        $reader = [IO.File]::Open(
            $lockFile, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        try {
            $bytes = New-Object byte[] $reader.Length
            [void] $reader.Read($bytes, 0, $bytes.Length)
            $holder = [Text.Encoding]::UTF8.GetString($bytes).Trim()
        }
        finally { $reader.Dispose() }
    }
    catch { }
    Stop-Gate 11 "another gate run holds the harness '$harnessRoot', so this run refuses rather than race it. The holder reports: $holder. Wait for it to finish and run again."
}

$runId = ([guid]::NewGuid().ToString('N')).Substring(0, 12)
$identity = "pid $PID, run $runId, started $((Get-Date).ToString('s'))"
$identityBytes = [Text.Encoding]::UTF8.GetBytes($identity)
$script:GateLock.Write($identityBytes, 0, $identityBytes.Length)
$script:GateLock.Flush()

# This run's report and log carry its id, so that runs cannot land in each other's
# evidence. That separation is hygiene, not the proof: what the verdict rests on is the
# pid the report names, checked after the editor has run.
$reportRoot = Join-Path $harnessRoot 'Saved\GateReport'
$reportDir = Join-Path $reportRoot $runId
$reportFile = Join-Path $reportDir 'index.json'
$logDir = Join-Path $harnessRoot 'Saved\Logs'
$editorLog = Join-Path $logDir "gate-$runId.log"

Write-Step "run $runId holds the harness lock"

# The harness reaches the plugin through a directory junction at Plugins\FANTASIA, so
# that the plugin is an ordinary project plugin.
#
# This is not AdditionalPluginDirectories, and the difference is the whole gate. A
# plugin reached that way is an external plugin: UnrealBuildTool links its modules into
# the HOST PROJECT's Binaries, while the editor's plugin loader still loads them from
# the PLUGIN's own Binaries\Win64 -- which holds the committed prebuilt DLLs, whose
# BuildId matches. The editor therefore runs the committed binaries and never the code
# just compiled, which is exactly the stale-DLL trap ADR-0016 set out to avoid.
# As a project plugin, the compile's output and the editor's load path are the same
# file by construction, so a green run cannot be a stale one.
#
# The junction names the FANTASIA plugin directory alone, so FANTASIAACE is not merely
# disabled: it is not discoverable from the harness at all.
$junctionDir = Join-Path $harnessRoot 'Plugins'
$junction = Join-Path $junctionDir 'FANTASIA'
if (-not (Test-Path -LiteralPath $junctionDir)) {
    New-Item -ItemType Directory -Path $junctionDir -Force | Out-Null
}
$junctionTarget = $null
if (Test-Path -LiteralPath $junction) {
    $junctionTarget = (Get-Item -LiteralPath $junction -Force).Target | Select-Object -First 1
}
if ($junctionTarget -ne $PluginDir) {
    if (Test-Path -LiteralPath $junction) {
        # A junction is removed, never recursed into: deleting its contents would delete
        # the plugin itself.
        [IO.Directory]::Delete($junction)
    }
    & "$env:SystemRoot\System32\cmd.exe" /c mklink /J "$junction" "$PluginDir" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Stop-Gate 8 "could not create the directory junction '$junction' -> '$PluginDir' (mklink exited $LASTEXITCODE)."
    }
}

Set-FileIfChanged $uproject @"
{
	"FileVersion": 3,
	"EngineAssociation": "$requiredLine",
	"Description": "Generated host project for FANTASIA's gate (FANTASIA/run-tests.ps1). Regenerated on demand; do not edit.",
	"Modules": [
		{
			"Name": "$HarnessName",
			"Type": "Runtime",
			"LoadingPhase": "Default"
		}
	],
	"Plugins": [
		{
			"Name": "FANTASIA",
			"Enabled": true
		}
	]
}
"@

Set-FileIfChanged (Join-Path $sourceDir "$HarnessName.Target.cs") @"
using UnrealBuildTool;

public class ${HarnessName}Target : TargetRules
{
	public ${HarnessName}Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("$HarnessName");
	}
}
"@

Set-FileIfChanged (Join-Path $sourceDir "${HarnessName}Editor.Target.cs") @"
using UnrealBuildTool;

public class ${HarnessName}EditorTarget : TargetRules
{
	public ${HarnessName}EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("$HarnessName");
	}
}
"@

Set-FileIfChanged (Join-Path $sourceDir "$HarnessName\$HarnessName.Build.cs") @"
using UnrealBuildTool;

public class $HarnessName : ModuleRules
{
	public $HarnessName(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
"@

Set-FileIfChanged (Join-Path $sourceDir "$HarnessName\$HarnessName.cpp") @"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, $HarnessName, "$HarnessName");
"@

Write-Step "harness project $uproject"

# Both the report and the log are evidence about THIS run, so none may survive from an
# earlier one: a stale log would let the module-load check below pass on a run during
# which the editor never started. Holding the lock means no other gate run exists, so
# every earlier run's evidence is dead and all of it goes, rather than accumulating a
# directory per run for something later to collect.
if (Test-Path -LiteralPath $reportRoot) {
    Remove-Item -LiteralPath $reportRoot -Recurse -Force
}
foreach ($stale in (Get-ChildItem -LiteralPath $logDir -Filter 'gate*.log' -File -ErrorAction SilentlyContinue)) {
    Remove-Item -LiteralPath $stale.FullName -Force
}

# ------------------------------------------------------------------ compile ----

# UnrealBuildTool and UnrealHeaderTool write response files and generated sources into
# the plugin's own Intermediate directory. When that directory is not writable -- most
# often because the gate was launched from a sandboxed shell -- the compile fails deep
# inside cl.exe with 'D8022: cannot open ...obj.rsp' and a missing .gen.cpp, which reads
# like a source error and is not one. Probe for it first so that this fourth way of
# verifying nothing is named rather than disguised.
$intermediateDir = Join-Path $PluginDir 'Intermediate'
$probe = Join-Path $intermediateDir '.gate-write-probe'
try {
    if (-not (Test-Path -LiteralPath $intermediateDir)) {
        New-Item -ItemType Directory -Path $intermediateDir -Force | Out-Null
    }
    [IO.File]::WriteAllText($probe, 'probe')
    Remove-Item -LiteralPath $probe -Force
}
catch {
    Stop-Gate 9 "cannot write into '$intermediateDir', so UnrealBuildTool cannot either and the compile would fail with a misleading 'D8022: cannot open ...obj.rsp'. Run the gate from a shell that may write there: a sandboxed shell is the usual cause. Underlying error: $($_.Exception.Message)"
}

# UnrealBuildTool caches its makefile, which holds the list of source files, and on a
# warm run it does not always notice a source file that has been ADDED since. A gate
# that silently skips a new test file would report green on code it never compiled, so
# the makefile is discarded every run and the source list is rediscovered. Only
# discovery is repeated: unchanged objects are still reused, which is what keeps a warm
# run short.
$makefile = Join-Path $harnessRoot "Intermediate\Build\Win64\x64\${HarnessName}Editor\Development\Makefile.bin"
if (Test-Path -LiteralPath $makefile) {
    Remove-Item -LiteralPath $makefile -Force
}

$binariesBefore = Get-BinariesStamp $BinariesDir

Write-Step "compiling ${HarnessName}Editor Win64 Development (FANTASIA and FANTASIATests)"
& $buildBat "${HarnessName}Editor" Win64 Development -Project="$uproject" -WaitMutex -NoHotReload
$compileExit = $LASTEXITCODE

if ((Get-BinariesStamp $BinariesDir) -ne $binariesBefore) {
    Write-Step "NOTE: this run rewrote the tracked binaries under '$BinariesDir'. P-12 governs those files -- they are the users' build -- so refresh and commit them deliberately."
}

if ($compileExit -ne 0) {
    Stop-Gate 2 "the compile failed (Build.bat exited $compileExit). No test ran, so this run verified nothing."
}

# ------------------------------------------------------------- BuildId check ----
# C-build's last invariant, and the error users actually hit: an engine whose
# CompatibleChangelist differs from the committed .modules BuildId makes the editor
# refuse the prebuilt plugin.

$modulesFile = Join-Path $BinariesDir 'UnrealEditor.modules'
if (-not (Test-Path -LiteralPath $modulesFile)) {
    Stop-Gate 7 "the compile succeeded but emitted no '$modulesFile', so its BuildId cannot be compared with the engine's."
}
$emittedBuildId = (Get-Content -LiteralPath $modulesFile -Raw | ConvertFrom-Json).BuildId
if ("$emittedBuildId" -ne "$expectedBuildId") {
    Stop-Gate 7 "BuildId mismatch: '$modulesFile' says $emittedBuildId, the engine's CompatibleChangelist is $expectedBuildId. Users would be told the plugin was built with a different engine version."
}
Write-Step "BuildId $emittedBuildId matches the engine"

# -------------------------------------------------------------------- tests ----

Write-Step "running 'Automation RunTests $Tests' headless"

$editorArgs = @(
    "$uproject",
    "-ExecCmds=`"Automation RunTests $Tests`"",
    "-TestExit=`"Automation Test Queue Empty`"",
    "-ReportExportPath=`"$reportDir`"",
    '-unattended',
    '-nopause',
    '-nosplash',
    '-nullrhi',
    '-nosound',
    '-NoP4',
    "-abslog=`"$editorLog`""
)
# Started rather than invoked, for the one thing an invocation cannot give: the editor's
# process id. The report names the instance that wrote it as '<computer>-<pid>'
# (FApp::GetInstanceName), so knowing this pid turns the report from something found at
# an agreed path into something stamped by a process this run started.
#
# The arguments go as one verbatim command line, because Start-Process quotes any array
# element containing a space and would wrap '-ExecCmds="Automation RunTests X"' again.
# WaitForExit waits on this process alone; -Wait would also wait on its descendants, and
# the editor leaves a Zen storage service running behind it.
$editor = Start-Process -FilePath $editorExe -ArgumentList ($editorArgs -join ' ') -NoNewWindow -PassThru
# Reading the handle makes .NET keep it, without which ExitCode is empty after the
# process has gone. The exit code is reported, never acted on: the report is the verdict.
[void] $editor.Handle
$editor.WaitForExit()
$editorExit = $editor.ExitCode
$editorPid = $editor.Id
Write-Step "editor (pid $editorPid) exited $editorExit (not the verdict; the report is)"

# Guard the one failure mode that makes a gate worse than none: the editor loading a
# prebuilt module instead of the one just compiled. It loads by BuildId, not by
# timestamp, so a stale DLL whose BuildId still matches would report green.
if (Test-Path -LiteralPath $editorLog) {
    $loadLine = Select-String -LiteralPath $editorLog -Pattern "InternalLoadLibrary: 'FANTASIATests' \('(.+?)'\)" |
        Select-Object -Last 1
    if ($loadLine) {
        $loadedFrom = $loadLine.Matches[0].Groups[1].Value.Replace('/', '\')
        $loadedDir = (Split-Path -Parent $loadedFrom).TrimEnd('\')
        $acceptable = @($BinariesDir.TrimEnd('\'), (Join-Path $junction 'Binaries\Win64'))
        if ($acceptable -notcontains $loadedDir) {
            Stop-Gate 10 "the editor loaded FANTASIATests from '$loadedFrom', which is not the directory this run compiled into ('$BinariesDir'). The suite therefore did not test the code that was just built."
        }
        Write-Step "editor loaded FANTASIATests from the directory this run compiled into"
    }
}

if (-not (Test-Path -LiteralPath $reportFile)) {
    Stop-Gate 3 "the editor wrote no report at '$reportFile', so no test is known to have run. Editor log: '$editorLog'."
}

$report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json

# The report's provenance, checked rather than inferred (F-035). Each device in the
# report names the instance that ran its tests as '<computer>-<pid>', and that pid is
# the editor this run started. A report inherited from any other editor -- a run that
# was killed, a hand-started editor, a concurrent run -- names a different one, and a
# green verdict read from it would be a verdict on tests this run never ran.
# The gate runs one device, so one device carrying this run's pid is the whole report
# accounted for; a report naming several, of which only one is this editor, is recorded
# as an observation to settle if the gate ever starts more than one device (J-0146).
$instanceNames = @($report.devices | ForEach-Object { $_.instanceName })
$ownInstances = @($instanceNames | Where-Object { $_ -like "*-$editorPid" })
if ($ownInstances.Count -eq 0) {
    Stop-Gate 3 "the report at '$reportFile' was written by instance(s) '$($instanceNames -join "', '")', none of which is the editor this run started (pid $editorPid). It is someone else's evidence, so this run concludes nothing from it. Editor log: '$editorLog'."
}
Write-Step "report stamped by this run's own editor (pid $editorPid)"

$reported = @($report.tests)

$notPassed = @($reported | Where-Object { $_.state -ne 'Success' -and $_.state -ne 'SucceededWithWarnings' })
if ($notPassed.Count -gt 0) {
    Write-Host ''
    Write-Host "[gate] $($notPassed.Count) of $($reported.Count) tests did not pass:"
    foreach ($test in $notPassed) {
        Write-Host "[gate]   $($test.state): $($test.fullTestPath)"
    }
    Stop-Gate 5 "$($notPassed.Count) of $($reported.Count) tests did not pass. Editor log: '$editorLog'."
}

if ($reported.Count -lt $MinTests) {
    Stop-Gate 6 "the report names only $($reported.Count) tests, fewer than the $MinTests expected. A suite that shrank silently is not a pass."
}

Remove-GateLock
Write-Host ''
Write-Step "PASSED: $($reported.Count) tests, 0 failures, prefix '$Tests'"
Write-Host ("[gate] elapsed {0:n1} s" -f $started.Elapsed.TotalSeconds)
exit 0
