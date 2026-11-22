<#
.SYNOPSIS
    FANTASIA's mechanical gate: run the plugin's automation suite against an installed
    engine -- either the source just compiled, or the binaries the repository ships --
    then say pass or fail.

.DESCRIPTION
    This repository is a plugin and holds no .uproject, so the suite cannot be run
    from it as it stands. The gate therefore brings its own host project (ADR-0016):
    it generates, and then reuses, a minimal project outside this repository's
    working tree, reaches the plugin from there, and enables FANTASIA only.

    There are two modes, and a green run means a different thing in each, so no reader
    has to guess which they hold: the mode is in every line this script prints and in
    every record it writes.

      compiled  (the default, and the gate 'gates.task_completed' runs) compiles the
                FANTASIA and FANTASIATests modules into the plugin's own Binaries\Win64
                and runs the suite against what it compiled. A green run says the
                SOURCE passes.
      prebuilt  compiles nothing. Its host project is content-only -- no Source
                directory, no Target.cs, and the build tool is never invoked -- so the
                editor loads the committed module DLLs, which is what a Blueprint-only
                user receives (US-019, NFR-005). A green run says the SHIPPED ARTIFACT
                passes its own suite. That is mechanically possible because the committed
                'UnrealEditor.modules' names FANTASIATests as well as FANTASIA: the
                automation suite ships with the plugin.

    Neither mode substitutes for the other. The compiled mode cannot tell a user whether
    the DLLs in the clone work, and the prebuilt mode cannot tell a developer whether the
    change they just wrote compiles.

    The two modes generate different host projects, under
    '%LOCALAPPDATA%\FANTASIA-Gate\FantasiaGate' and '...\FantasiaGatePrebuilt', but they
    share one plugin tree: the compiled mode WRITES the plugin's Binaries\Win64 while the
    prebuilt mode READS it, so they must never run at once. The lock below therefore sits
    one directory above both harnesses and is held by whichever mode runs.

    Either mode reaches the plugin through a directory junction rather than through
    AdditionalPluginDirectories, which is the one place this script departs from
    ADR-0016's wording. The reason is in the comment beside the junction: an external
    plugin's modules are linked into the host project while the editor still loads them
    from the plugin's own Binaries, so the suite would have run the committed prebuilt
    DLLs and never the code just compiled (ADR-0017).

    That is also why the two modes keep two separate module-load guards and why they must
    not be collapsed into one. They assert opposite things about the same observation: the
    compiled mode requires the editor to have loaded FANTASIATests from the directory it
    compiled into, and the prebuilt mode requires it to have loaded the committed DLL and
    for nothing to have rewritten it. One guard covering both readings would accept the
    stale-prebuilt-DLL run that ADR-0016 rejected and ADR-0017 measured.

    FANTASIAACE is outside the HARNESS on purpose: it has no test module, and its NVIDIA
    dependency is installed beside a user's project rather than in this repository, so the
    junction names the FANTASIA plugin alone and the harness cannot see ACE at all rather
    than appearing to test it. Its committed binaries are a different matter -- they are in
    this repository and they are what a user receives -- so the pre-flight below reads them
    with the core plugin's (US-021). The division is the one a reader has to keep: this gate
    knows nothing about how FANTASIAACE behaves, and everything about what it ships.

    The exit code comes from the exported report, never from the editor's exit status:
    UnrealEditor-Cmd.exe exits zero in cases that are not successes.

    One gate run at a time, in either mode, and the report must be this run's own
    (F-035). The gate holds a single exclusive lock for its whole length, so a second run
    refuses with exit 11 instead of racing: two runs would otherwise share the plugin's
    Intermediate and its tracked Binaries -- which one mode rewrites and the other reads
    as the artifact under test -- and isolating only the report would hide that corruption
    rather than remove it. The lock is one file beside the run history, not one per
    harness, because the thing being protected is the plugin tree both modes share. It is
    a file handle, so it cannot go stale -- the operating system releases it when the
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

    A verdict is about the tree it was reached on and about no other, so every run says
    which tree that was. It writes 'gate-run.json' beside the exported report, and
    appends one line describing itself to 'gate-history.jsonl', which sits beside the
    harness rather than inside it. The placement is the whole point: the report root is
    deleted at the start of every run, so a record kept only there cannot be read a
    second time, and a verdict that cannot be re-read is the defect this record exists to
    remove. The history file is outside this repository too, so neither a wiped harness
    nor 'git clean' takes it, and nothing tracks it in git.

    Both are the same record written twice, so they cannot disagree, and every fact in it
    starts as 'unknown'. A run that cannot establish a fact says so and carries on: a ZIP
    download of this repository has no '.git', and 'git rev-parse' then fails for an
    honest reason. An omitted field, an empty string or a zero would instead let a later
    reader infer a tree that was never checked, which is the failure this record is for.
    A call refused for its arguments (exit 8) records nothing, because it stops before
    there is a tree to name.

    Before any of that, and before the lock, the run checks what a user actually
    receives. Five things are decidable from the repository and one HTTP request, so they
    are decided first and cost seconds rather than a compile (P-05): every committed
    '.modules' manifest in both plugins carries this engine's effective compatible
    changelist; the committed module binaries of each plugin were committed no earlier
    than the source they are built from; every module DLL those manifests name imports
    nothing outside a closed allow-list; every script beside 'bootstrap.bat' is delivered
    with the line endings its interpreter can read; and every release asset
    'bootstrap.bat' fetches is present, with a non-zero size, on the release that script
    points at. None of it needs a harness, so none of it waits for the lock.

    The second of those five -- the binaries against the source they ship for -- belongs to
    the prebuilt mode and only refuses there (exit 19). The prebuilt mode's whole claim is
    that the committed DLLs ARE the product, and source that landed after the last
    binaries refresh makes that claim false, so the run refuses rather than report green on
    an artifact that does not match the tree it sits in. The compiled mode records the same
    observation and fails on nothing, because it is about to rebuild those modules anyway.
    The defect this catches is a half-finished refresh: source committed, or one of the two
    plugins' binary sets refreshed and not the other. It is checked in two frames, because
    a tree can break the claim in two ways -- the committed frame compares the last commit
    touching a plugin's Source with the last commit touching its Binaries, and the working
    frame asks whether any file under either Source tree is modified or untracked here and
    now. A modified binary in the working copy is recorded and does not refuse, so that a
    refresh can be verified before it is committed; the record says which artifact the
    verdict is therefore about.

    The asset check is an availability check and says nothing about integrity: no digest is
    compared anywhere in this project (F-026, open). It reads the tag and the asset names
    out of 'bootstrap.bat' rather than keeping a copy of them, because that script is the
    only place that knows where the assets live (P-17), and the two release buckets would
    otherwise make a stale copy report a missing asset that is not missing.

.PARAMETER Mode
    'compiled' (the default) or 'prebuilt'. See the two modes above. 'prebuilt' invokes no
    build tool at all: its host project has no Source directory and no Target.cs, and the
    run refuses with exit 20 if the harness turns out not to be content-only or if the
    plugin's tracked binaries change while it runs.

.PARAMETER Tests
    The automation test prefix to run. Default 'FANTASIA', the whole suite.

.PARAMETER MinTests
    The run fails if the report names fewer tests than this. Default 214, the whole suite
    on the supported configuration (Win64 with the aGrUM binding), so that a single test
    missing from it turns the default run red. A build without the aGrUM binding is the
    exception and states its own floor: the default less the 16 tests compiled only under
    WITH_AGRUM_BINDING (all of BayesianNetworkTests.cpp and InfluenceDiagTests.cpp, the
    suite's only conditional compilation), today -MinTests 198. The default is reset to
    the new count in the same change that adds or removes tests. How many tests a run
    actually found is in that run's record.

    The floor is the same in both modes, and deliberately: it describes the suite, not how
    fresh the committed DLLs are. The prebuilt mode runs whatever suite the committed DLL
    holds, which is fewer tests than the source holds whenever source has landed since the
    last refresh -- and that gap is what exit 19 is for, not something to encode as a
    second floor here.

.OUTPUTS
    Exit 0 only when nothing the mode requires went wrong (the compile succeeded in the
    compiled mode; nothing compiled in the prebuilt mode), the BuildId matches the engine,
    the exported report was stamped by the editor this run started, and that report names
    at least MinTests tests with no failure and nothing not run. Every other outcome exits
    non-zero and says which:
      1 no engine resolved, or its version is not the plugin's
      2 the compile failed (compiled mode only)
      3 no report, or a report another editor wrote
      5 the report names failures, or tests that did not run
      6 the report names fewer tests than MinTests, zero included
      7 the emitted .modules BuildId is not the engine's CompatibleChangelist (compiled
        mode only; the prebuilt mode compiles nothing and reads the committed manifests
        in the pre-flight instead, which is exit 13)
      8 the script was called with an argument it cannot use
      9 the plugin's Intermediate directory is not writable (a sandboxed shell), so the
        compile would fail misleadingly inside cl.exe rather than honestly here (compiled
        mode only)
     10 the editor loaded the test module from outside the mode's acceptable set, so the
        suite did not test the artifact that mode is about: in the compiled mode, from
        somewhere this run did not compile into; in the prebuilt mode, from anywhere but
        the plugin's own committed Binaries\Win64
     11 another gate run, in either mode, holds the gate lock
     12 the suite passed but the run could not be appended to the history file, so
        nothing durable names the tree it passed on. The verdict is withheld rather
        than left unrecorded.
     13 a committed '.modules' manifest does not describe this engine: it is missing, it
        is unreadable, it names a module DLL that is not beside it, or its BuildId is not
        the engine's effective compatible changelist
     14 the import check could not be made: dumpbin was not found, or it named no
        dependency of a module DLL. The check is refused, never skipped
     15 a committed module DLL imports a library outside the allow-list
     16 an asset 'bootstrap.bat' fetches is absent from the release it points at, or is
        published with a zero size
     17 'bootstrap.bat' could not be read, or names no release tag, no repository or no
        asset, so there was nothing to check the release against
     18 a first-party script beside 'bootstrap.bat' would be delivered with the wrong line
        endings: the 'eol' attribute is missing or wrong, or the working copy does not
        match it
     19 prebuilt mode only: the committed binaries are not the product of the source
        beside them -- source was committed after the last binaries refresh, or source is
        modified or untracked in this working tree -- so a green run would say the shipped
        artifact passes a suite that is not the one in this tree
     20 prebuilt mode only: the run did not stay a no-compile run. Either its host project
        is not content-only (a Source directory or a Target.cs is present), or the
        plugin's tracked binaries changed while the editor ran. Both mean the mode proved
        something other than what it claims
    Code 4 is retired. It meant 'the report names zero tests', and no run can reach it:
    when the prefix matches nothing the editor exits without exporting a report at all,
    which is exit 3. A zero-test report, if the engine ever wrote one, is fewer tests
    than MinTests and so is exit 6. The numbers of the other codes are left alone,
    because they are quoted outside this script.
#>

[CmdletBinding()]
param(
    [string] $Mode = 'compiled',
    [string] $Tests = 'FANTASIA',
    [int]    $MinTests = 214
)

$ErrorActionPreference = 'Stop'
$started = [Diagnostics.Stopwatch]::StartNew()

$DefaultEngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
$HistoryFileName = 'gate-history.jsonl'
$LockFileName = 'gate.lock'
# One harness per mode, because a content-only project and a code project cannot be the
# same directory: the second would leave a Source tree behind and the prebuilt mode's
# first claim is that there is none. Both sit under the same parent, which is where the
# lock and the run history live.
$HarnessNames = @{ compiled = 'FantasiaGate'; prebuilt = 'FantasiaGatePrebuilt' }

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

# ----------------------------------------------------- this run's own record ----
# What this run checked, and on which tree, written where it can be read again. The
# fields are filled in as facts become known; $script:Run does not exist until the
# provenance section below has a tree to describe, which is why Stop-Gate tolerates its
# absence rather than inventing one.

$script:Run = $null
$script:GitExe = $null
$script:ReportDir = $null
$script:HistoryFile = $null

# Runs a native tool and returns whether it ran, the code it exited with, and the lines it
# wrote. Every tool this gate consults about its surroundings -- git, vswhere, dumpbin --
# is consulted through here, because every one of them can fail for an honest reason that
# must not stop the run.
#
# The exit code and the output come back together, and that is the whole reason this
# function exists under the one below it: PowerShell unrolls 'no output' to $null, so
# output alone cannot tell a command that SUCCEEDED SILENTLY from one that failed. Two
# questions this gate asks are answered silently -- 'git merge-base --is-ancestor' replies
# in its exit code and nothing else, and 'git status --porcelain' on a clean tree says
# nothing at all, which is the answer 'clean'.
function Invoke-ToolChecked([string] $Exe, [string[]] $Arguments) {
    $result = [ordered] @{ ran = $false; code = -1; lines = @() }
    $previous = $ErrorActionPreference
    # Windows PowerShell 5.1 turns a native command's stderr into terminating errors
    # while $ErrorActionPreference is 'Stop', and these tools write to stderr on exactly
    # the ordinary failures this function exists to absorb.
    $ErrorActionPreference = 'Continue'
    try {
        $output = & $Exe @Arguments 2>$null
        $result.ran = $true
        $result.code = $LASTEXITCODE
        # Nulls are filtered rather than kept, so that 'no output' is an empty list and not
        # a list holding one nothing.
        $result.lines = @($output | Where-Object { $null -ne $_ })
    }
    catch { }
    finally { $ErrorActionPreference = $previous }
    return $result
}

# The lines a tool wrote when it succeeded, or $null when it did not: for the callers that
# want what a tool SAID. A caller that has to know whether a tool SUCCEEDED uses
# Invoke-ToolChecked, because a silent success is indistinguishable here.
#
# The leading comma keeps the array an array. Without it PowerShell unrolls a one-line
# result into a bare string, whose [0] is its first character.
function Invoke-Tool([string] $Exe, [string[]] $Arguments) {
    $result = Invoke-ToolChecked $Exe $Arguments
    if (-not $result.ran -or $result.code -ne 0) { return $null }
    return ,@($result.lines)
}

# Runs git inside the repository and returns its output lines, or $null when git is
# absent or the command failed. Nothing here is fatal by design: a ZIP download has no
# '.git', and the gate must still run and still report what it could not establish.
function Invoke-Git([string[]] $Arguments) {
    if ($null -eq $script:GitExe) { return $null }
    return (Invoke-Tool $script:GitExe $Arguments)
}

# The identity of the tree the gate is about to test: the commit, and how that commit
# differs from what is on disk.
#
# Two dirty lists, not one. A modified tracked file and an untracked file both make this
# tree something other than the commit named beside them, and both are compiled: an
# untracked .cpp under Source/ enters the artifact under test and moves the test count.
# So both count as dirty. They are kept apart because only the tracked list describes a
# difference from the commit's own content -- someone checking that commit out would get
# the tracked files back and would not get the untracked ones at all -- and a reader has
# to be able to tell which kind of difference they are looking at.
function Get-TreeIdentity([string] $Root) {
    $tree = [ordered] @{
        state           = 'unknown'
        commit          = 'unknown'
        branch          = 'unknown'
        source          = 'unknown'
        trackedModified = @()
        untracked       = @()
    }
    if ($null -eq $script:GitExe) {
        $tree.source = 'not established: no git on PATH, so this tree names no commit'
        return $tree
    }
    $commit = Invoke-Git @('-C', $Root, 'rev-parse', 'HEAD')
    if ($null -eq $commit -or $commit.Count -eq 0) {
        $tree.source = "not established: 'git rev-parse HEAD' failed in '$Root' (an archive or ZIP download of this repository has no .git)"
        return $tree
    }
    $tree.commit = "$($commit | Select-Object -First 1)".Trim()
    $tree.source = "git rev-parse HEAD in '$Root'"

    $branch = Invoke-Git @('-C', $Root, 'rev-parse', '--abbrev-ref', 'HEAD')
    if ($null -ne $branch -and $branch.Count -gt 0) {
        $tree.branch = "$($branch | Select-Object -First 1)".Trim()
    }

    # --untracked-files=all lists every untracked file by name; ignored files are not
    # listed at all, so the two sets stay bounded by what someone actually edited.
    #
    # Asked through the checked form, because a clean tree is the SILENT answer here: git
    # prints nothing and exits zero. Output alone cannot tell that apart from a git that
    # failed -- PowerShell delivers an empty result as $null either way -- and reading it as
    # a failure would record the one tree this gate most needs to describe, a clean
    # checkout, as 'cleanliness unknown'.
    $status = Invoke-ToolChecked $script:GitExe @('-C', $Root, 'status', '--porcelain=v1', '--untracked-files=all')
    if (-not $status.ran -or $status.code -ne 0) {
        $tree.source += "; 'git status' failed (exit $($status.code)), so the working tree's cleanliness is unknown"
        return $tree
    }
    $modified = New-Object Collections.ArrayList
    $untracked = New-Object Collections.ArrayList
    foreach ($line in $status.lines) {
        $text = "$line"
        if ($text.Length -lt 4) { continue }
        $code = $text.Substring(0, 2)
        $path = $text.Substring(3).Trim()
        # A rename reports 'old -> new'; the new name is the one this tree holds.
        $arrow = $path.IndexOf(' -> ')
        if ($arrow -ge 0) { $path = $path.Substring($arrow + 4) }
        $path = $path.Trim('"')
        if ($code -eq '??') { [void] $untracked.Add($path) } else { [void] $modified.Add($path) }
    }
    $tree.trackedModified = @($modified.ToArray())
    $tree.untracked = @($untracked.ToArray())
    if ($modified.Count -eq 0 -and $untracked.Count -eq 0) { $tree.state = 'clean' } else { $tree.state = 'dirty' }
    return $tree
}

# The tree in one readable phrase, for the lines a person reads rather than parses.
function Get-TreeLabel {
    if ($null -eq $script:Run) { return 'unknown' }
    $commit = "$($script:Run.tree.commit)"
    if ($commit -ne 'unknown' -and $commit.Length -ge 12) { $commit = $commit.Substring(0, 12) }
    if ($script:Run.tree.state -eq 'clean') { return $commit }
    if ($script:Run.tree.state -eq 'dirty') {
        return "$commit plus $($script:Run.tree.trackedModified.Count) modified and $($script:Run.tree.untracked.Count) untracked file(s)"
    }
    return "$commit, working tree unknown"
}

# Writes the record twice: pretty, beside the exported report, and as a single line
# appended to the history file. Returns $true only when that line was appended, because
# the line is the half of the record that outlives the run.
function Write-GateRecord {
    if ($null -eq $script:Run) { return $false }
    $script:Run.finishedUtc = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    $script:Run.elapsedSeconds = [math]::Round($started.Elapsed.TotalSeconds, 1)

    # Depth 6 reaches the nested tree, engine and tests objects; 5.1 defaults to 2 and
    # would serialise them as type names.
    try {
        if (-not (Test-Path -LiteralPath $script:ReportDir)) {
            New-Item -ItemType Directory -Path $script:ReportDir -Force | Out-Null
        }
        [IO.File]::WriteAllText(
            (Join-Path $script:ReportDir 'gate-run.json'),
            (ConvertTo-Json $script:Run -Depth 6))
    }
    catch {
        Write-Host "[gate] note: the run manifest could not be written beside the report: $($_.Exception.Message)"
    }

    $line = ConvertTo-Json $script:Run -Depth 6 -Compress
    # Append sharing readers only, so two writers wait for each other instead of
    # interleaving half a line. A run refused by the harness lock still records itself,
    # so this file does not have a single writer the way the harness does.
    for ($attempt = 1; $attempt -le 10; $attempt++) {
        try {
            $stream = [IO.File]::Open(
                $script:HistoryFile, [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::Read)
            try {
                $bytes = [Text.Encoding]::UTF8.GetBytes($line + "`r`n")
                $stream.Write($bytes, 0, $bytes.Length)
                $stream.Flush()
            }
            finally { $stream.Dispose() }
            return $true
        }
        catch { Start-Sleep -Milliseconds 100 }
    }
    Write-Host "[gate] note: this run could not be appended to '$script:HistoryFile', so nothing durable names the tree it ran on."
    return $false
}

# $SkipRecord is for the one caller that has already written the record and must not
# write a second, contradicting one: the passing run that could not append its line.
function Stop-Gate([int] $Code, [string] $Message, [switch] $SkipRecord) {
    if (-not $SkipRecord -and $null -ne $script:Run) {
        $script:Run.verdict = 'FAILED'
        $script:Run.exitCode = $Code
        $script:Run.reason = $Message
        [void] (Write-GateRecord)
    }
    Remove-GateLock
    Write-Host ''
    # The mode is in the verdict line as well as in the record: the two modes fail for
    # different reasons about different artifacts, and a reader of a console transcript
    # has only this line.
    Write-Host "[gate] FAILED ($Code, mode '$Mode'): $Message"
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

# Validated here rather than by a ValidateSet attribute, so that a bad mode exits 8 with
# this script's own message instead of PowerShell's parameter-binding error.
if (-not $HarnessNames.ContainsKey($Mode)) {
    Stop-Gate 8 "-Mode must be 'compiled' or 'prebuilt'; got '$Mode'."
}
$Prebuilt = ($Mode -eq 'prebuilt')
$HarnessName = $HarnessNames[$Mode]

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
$AceBinariesDir = Join-Path $RepoRoot 'FANTASIAACE\Binaries\Win64'
$BootstrapScript = Join-Path $PluginDir 'bootstrap.bat'

if (-not (Test-Path -LiteralPath $PluginDescriptor)) {
    Stop-Gate 8 "no FANTASIA.uplugin beside this script: expected $PluginDescriptor."
}

if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    Stop-Gate 8 'LOCALAPPDATA is empty, so there is nowhere outside this repository to put the harness and the run history. Run the gate from a shell with the ordinary user environment.'
}

# --------------------------------------------------------------- provenance ----
# Everything this run will be able to say about itself, decided before it does anything,
# so that even a run that fails at its first check names the tree it failed on.

# The two harnesses' common parent: one directory above either harness, and so above
# everything a run deletes -- the report root goes at the start of every run and a harness
# project is deleted by hand to force a cold run. Outside this repository as well, so 'git
# clean' cannot reach it and git tracks nothing of it. The run history and the one lock
# both live here, for the same reason in two different senses: the history must outlive
# every harness, and the lock must cover both of them.
$gateHome = Join-Path $env:LOCALAPPDATA 'FANTASIA-Gate'
$harnessRoot = Join-Path $gateHome $HarnessName
$script:HistoryFile = Join-Path $gateHome $HistoryFileName
$lockFile = Join-Path $gateHome $LockFileName
try {
    $historyDir = Split-Path -Parent $script:HistoryFile
    if (-not (Test-Path -LiteralPath $historyDir)) {
        New-Item -ItemType Directory -Path $historyDir -Force | Out-Null
    }
}
catch {
    Stop-Gate 12 "cannot create '$(Split-Path -Parent $script:HistoryFile)', so this run could not be recorded and the gate refuses before compiling anything. Underlying error: $($_.Exception.Message)"
}

$runId = ([guid]::NewGuid().ToString('N')).Substring(0, 12)
# This run's report and log carry its id, so that runs cannot land in each other's
# evidence. That separation is hygiene, not the proof: what the verdict rests on is the
# pid the report names, checked after the editor has run.
$reportRoot = Join-Path $harnessRoot 'Saved\GateReport'
$script:ReportDir = Join-Path $reportRoot $runId
$reportFile = Join-Path $script:ReportDir 'index.json'
$logDir = Join-Path $harnessRoot 'Saved\Logs'
$editorLog = Join-Path $logDir "gate-$runId.log"

# Select-Object -First 1 is load-bearing: a Git for Windows install puts git.exe on PATH
# twice, and Get-Command then returns two commands whose joined Source resolves to
# nothing.
$gitCommand = Get-Command 'git' -CommandType Application -ErrorAction SilentlyContinue |
    Select-Object -First 1
if ($null -ne $gitCommand) { $script:GitExe = $gitCommand.Source }

$script:Run = [ordered] @{
    schema         = 'fantasia-gate-run/1'
    runId          = $runId
    startedUtc     = (Get-Date).ToUniversalTime().ToString('yyyy-MM-ddTHH:mm:ssZ')
    finishedUtc    = 'unknown'
    elapsedSeconds = 'unknown'
    host           = $env:COMPUTERNAME
    pid            = $PID
    # 'compiled' runs the suite against what this run built; 'prebuilt' runs it against
    # the committed module DLLs and builds nothing. A green run means a different thing in
    # each, so this field is how a reader tells which verdict they are holding, and it is
    # the first thing the record says after the run's identity.
    mode           = $Mode
    # What the mode claims about itself, recorded as facts rather than asserted in prose.
    # In the prebuilt mode 'buildTool' stays 'not invoked' and 'hostProjectHasSource' stays
    # $false for the whole run, and both are checked again after the editor has gone: that
    # pair is the evidence that nothing compiled.
    harness        = [ordered] @{
        root                 = $harnessRoot
        project              = 'unknown'
        hostProjectHasSource = 'unknown'
        buildTool            = 'unknown'
        lockFile             = $lockFile
    }
    tree           = (Get-TreeIdentity $RepoRoot)
    engine         = [ordered] @{
        root                 = 'unknown'
        version              = 'unknown'
        compatibleChangelist = 'unknown'
        emittedBuildId       = 'unknown'
    }
    tests          = [ordered] @{
        prefix    = $Tests
        minTests  = $MinTests
        reported  = 'unknown'
        notPassed = 'unknown'
    }
    # What the pre-flight found about the distribution: the committed module manifests, the
    # imports of the module DLLs they name, the line endings the first-party scripts are
    # delivered with, and the liveness of the release assets bootstrap.bat fetches. Each
    # starts as 'unknown', so a run that stopped before a check cannot be read as having
    # made it; two of them can also end as 'unchecked', which is what an unreachable
    # github.com and an absent git respectively leave behind.
    modules        = 'unknown'
    binaries       = 'unknown'
    imports        = 'unknown'
    scripts        = 'unknown'
    releaseAssets  = 'unknown'
    verdict        = 'unknown'
    exitCode       = 'unknown'
    reason         = 'unknown'
    reportDir      = $script:ReportDir
    editorLog      = $editorLog
    historyFile    = $script:HistoryFile
}

Write-Step "mode '$Mode': the suite runs against $(if ($Prebuilt) { 'the committed module binaries, and nothing is compiled' } else { 'the modules this run compiles' })"
Write-Step "tree $(Get-TreeLabel) ($($script:Run.tree.source))"

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
# Required by the compiled mode and by nothing else. The prebuilt mode must not even
# depend on a build tool being present: an engine installation that can boot the editor is
# all a Blueprint-only user has, and the mode exists to run as that user runs.
if ($Prebuilt) {
    $script:Run.harness.buildTool = 'not invoked'
}
elseif (-not (Test-Path -LiteralPath $buildBat)) {
    Stop-Gate 1 "engine at '$engineRoot', read from $engineSource, has no build tool: '$buildBat' does not exist."
}
else {
    $script:Run.harness.buildTool = $buildBat
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

$script:Run.engine.root = $engineRoot
$script:Run.engine.version = "$($engineVersion.MajorVersion).$($engineVersion.MinorVersion).$($engineVersion.PatchVersion)"
# The effective changelist, after the fallback above: a licensee or source build carries
# no CompatibleChangelist, and it is this number, not the raw field, that a committed
# .modules BuildId has to equal.
$script:Run.engine.compatibleChangelist = $expectedBuildId

Write-Step "engine $engineLine at '$engineRoot' (from $engineSource), BuildId $expectedBuildId"
Write-Step "the suite runs FANTASIA only; FANTASIAACE has no test module and is not reachable from this harness, so nothing here tests how it behaves -- its committed binaries are checked below"

# ----------------------------------------------- distribution conformance ----
# What a user who compiles nothing receives, checked before anything is compiled.
#
# This runs here, after the engine is resolved and before the harness lock, for two
# reasons. It needs the engine's effective compatible changelist and nothing else the
# gate builds, and it needs no harness at all -- so a run that is about to be refused for
# a distribution defect is refused in seconds and never waits behind another run's
# compile (P-05).
#
# The three checks answer US-019 and US-021 from the repository: the BuildId a user is
# told about, the imports a user's loader has to resolve, and the assets a user's install
# downloads.

# Every committed module manifest of one plugin, with the BuildId it carries and the
# module DLLs it names. The set is enumerated rather than listed: a plugin ships one
# manifest per build configuration (Development and DebugGame today), and a manifest
# names as many modules as that configuration built, so there are more modules than
# manifests -- FANTASIA's names both FANTASIA and FANTASIATests.
function Get-CommittedModules([string] $PluginName, [string] $Directory) {
    $found = New-Object Collections.ArrayList
    if (-not (Test-Path -LiteralPath $Directory)) { return ,@() }
    foreach ($file in (Get-ChildItem -LiteralPath $Directory -Filter '*.modules' -File)) {
        try {
            $manifest = Get-Content -LiteralPath $file.FullName -Raw | ConvertFrom-Json
        }
        catch {
            Stop-Gate 13 "the committed manifest '$($file.FullName)' is not readable JSON, so the BuildId $PluginName tells a user about cannot be compared with the engine's. Underlying error: $($_.Exception.Message)"
        }
        $modules = New-Object Collections.ArrayList
        foreach ($property in $manifest.Modules.PSObject.Properties) {
            [void] $modules.Add([ordered] @{
                name = $property.Name
                dll  = Join-Path $Directory $property.Value
            })
        }
        [void] $found.Add([ordered] @{
            plugin   = $PluginName
            manifest = $file.FullName
            buildId  = "$($manifest.BuildId)"
            modules  = @($modules.ToArray())
        })
    }
    return ,@($found.ToArray())
}

# Whether each plugin's committed module binaries can be the product of the source beside
# them. This is the prebuilt mode's precondition: that mode's claim is that the committed
# DLLs ARE the product, and the claim is false the moment source has moved past them.
#
# Two frames, because a tree can break the claim in two ways and only one of them is in
# the commit graph.
#
#   committed  the last commit touching '<plugin>/Source' must be the last commit touching
#              '<plugin>/Binaries', or an ancestor of it. Equality is the ordinary case and
#              not an exception: the documented recipe ships a refresh in the same commit
#              as its source. 'merge-base --is-ancestor' rather than a date comparison,
#              because commit dates are not ordered and a rebase rewrites them.
#   working    no file under '<plugin>/Source' may be modified or untracked here and now.
#              An untracked .cpp is compiled into the next build and is therefore source
#              the committed DLL cannot contain; a modified one is source it contains a
#              different version of. Either way the DLL beside it is not its product.
#
# A modified committed binary is a third observation and deliberately not a refusal: it is
# what a refresh in progress looks like, and refusing would make the mode unusable for
# verifying the refresh it exists to justify. It is recorded, and it changes what the
# verdict is about -- the files on disk rather than the commit -- which 'artifact' says.
#
# Returns a record and never stops the run; the caller decides, because the same
# observation refuses in the prebuilt mode and is a note in the compiled one.
function Get-BinariesFreshness([string] $Root, [string[]] $Plugins, $Tree) {
    $result = [ordered] @{
        state    = 'unknown'
        artifact = 'unknown'
        checked  = @()
        stale    = @()
        note     = 'unknown'
    }
    if ($null -eq $script:GitExe -or "$($Tree.commit)" -eq 'unknown') {
        # Not a skipped check: with no repository there is no commit order to be wrong,
        # and the binaries beside the source are simply the bytes the user already has.
        $result.state = 'unchecked'
        $result.artifact = 'the files on disk: this tree names no commit, so the binaries in it cannot be placed against one'
        $result.note = 'no commit could be established for this tree, so the order in which source and binaries were committed is unknowable here (an archive or ZIP download has no .git)'
        return $result
    }

    $records = New-Object Collections.ArrayList
    $stale = New-Object Collections.ArrayList
    $dirtyBinaries = New-Object Collections.ArrayList

    foreach ($plugin in $Plugins) {
        $sourcePath = "$plugin/Source"
        $binariesPath = "$plugin/Binaries"
        $sourceCommit = Invoke-Git @('-C', $Root, 'log', '-1', '--format=%H', '--', $sourcePath)
        $binariesCommit = Invoke-Git @('-C', $Root, 'log', '-1', '--format=%H', '--', $binariesPath)
        $record = [ordered] @{
            plugin         = $plugin
            sourceCommit   = 'unknown'
            binariesCommit = 'unknown'
            committedFrame = 'unknown'
            dirtySource    = @()
        }
        if ($null -eq $sourceCommit -or $null -eq $binariesCommit -or
            $sourceCommit.Count -eq 0 -or $binariesCommit.Count -eq 0) {
            $record.committedFrame = 'unchecked: git named no commit for one of the two paths'
        }
        else {
            $record.sourceCommit = "$($sourceCommit | Select-Object -First 1)".Trim()
            $record.binariesCommit = "$($binariesCommit | Select-Object -First 1)".Trim()
            if ($record.sourceCommit -eq $record.binariesCommit) {
                $record.committedFrame = 'fresh: the same commit carries both, which is a refresh shipped with its source'
            }
            else {
                # 'merge-base --is-ancestor' answers in its exit code and writes nothing:
                # 0 yes, 1 no, anything else an error it could not decide. The third case
                # is recorded as undecided rather than read as 'no', because a refusal has
                # to rest on git having answered.
                $ancestor = Invoke-ToolChecked $script:GitExe @(
                    '-C', $Root, 'merge-base', '--is-ancestor',
                    $record.sourceCommit, $record.binariesCommit)
                if ($ancestor.ran -and $ancestor.code -eq 0) {
                    $record.committedFrame = 'fresh: the binaries were committed after the source'
                }
                elseif ($ancestor.ran -and $ancestor.code -eq 1) {
                    $record.committedFrame = 'stale: source was committed after the last binaries refresh'
                    [void] $stale.Add("$plugin's source moved past its binaries: '$sourcePath' was last committed in $($record.sourceCommit.Substring(0, 12)), which is not an ancestor of $($record.binariesCommit.Substring(0, 12)), the last commit touching '$binariesPath'")
                }
                else {
                    $record.committedFrame = "unchecked: 'git merge-base --is-ancestor' could not decide (exit $($ancestor.code))"
                }
            }
        }

        # The working frame, read off the tree identity this run already established rather
        # than by asking git a second time.
        $dirty = New-Object Collections.ArrayList
        foreach ($path in (@($Tree.trackedModified) + @($Tree.untracked))) {
            if ("$path" -like "$plugin/Source/*") { [void] $dirty.Add("$path") }
            elseif ("$path" -like "$plugin/Binaries/*") { [void] $dirtyBinaries.Add("$path") }
        }
        $record.dirtySource = @($dirty.ToArray())
        if ($dirty.Count -gt 0) {
            [void] $stale.Add("$plugin has $($dirty.Count) source file(s) modified or untracked in this working tree, which the committed binaries cannot contain: '$(($dirty | Select-Object -First 4) -join "', '")'$(if ($dirty.Count -gt 4) { ", and $($dirty.Count - 4) more" })")
        }
        [void] $records.Add($record)
    }

    $result.checked = @($records.ToArray())
    $result.stale = @($stale.ToArray())
    if ($dirtyBinaries.Count -eq 0) {
        $result.artifact = 'as committed'
    }
    else {
        $result.artifact = "working tree: $($dirtyBinaries.Count) committed binary file(s) are modified here, so a verdict is about the files on disk and not about the commit"
    }
    if ($stale.Count -gt 0) {
        $result.state = 'failed'
        $result.note = "the committed binaries are not the product of the source beside them -- $($stale -join '; ')"
    }
    else {
        $result.state = 'passed'
        $result.note = "each of the $($Plugins.Count) plugin(s) carries binaries committed no earlier than its source, and no source file is modified or untracked here"
    }
    return $result
}

# dumpbin is located, never assumed: it is not on PATH on an ordinary developer machine,
# and vswhere -- which Visual Studio's installer leaves at a fixed path -- is how the
# toolchain carrying it is found. Returns $null when there is none, which the caller
# turns into a refusal rather than a skipped check.
function Find-Dumpbin {
    $onPath = Get-Command 'dumpbin.exe' -CommandType Application -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -ne $onPath) { return $onPath.Source }
    foreach ($programFiles in @(${env:ProgramFiles(x86)}, $env:ProgramFiles)) {
        if ([string]::IsNullOrWhiteSpace($programFiles)) { continue }
        $vswhere = Join-Path $programFiles 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) { continue }
        $installs = Invoke-Tool $vswhere @(
            '-latest', '-products', '*',
            '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
            '-property', 'installationPath')
        foreach ($install in @($installs)) {
            $toolRoot = Join-Path "$install".Trim() 'VC\Tools\MSVC'
            if (-not (Test-Path -LiteralPath $toolRoot)) { continue }
            # Newest toolchain first, compared as versions: '14.9' sorts above '14.44'
            # as text and below it as a version.
            $toolchains = Get-ChildItem -LiteralPath $toolRoot -Directory |
                Sort-Object -Descending -Property @{
                    Expression = { try { [version] $_.Name } catch { [version] '0.0' } }
                }
            foreach ($toolchain in $toolchains) {
                $candidate = Join-Path $toolchain.FullName 'bin\Hostx64\x64\dumpbin.exe'
                if (Test-Path -LiteralPath $candidate) { return $candidate }
            }
        }
    }
    return $null
}

# The DLL names one image imports, delay-loaded ones included, or $null when dumpbin
# could not say. The dash form of the switch is deliberate: a MinGW shell rewrites
# '/dependents' into a path and dumpbin then reports a file it cannot open.
function Get-DllImports([string] $Dumpbin, [string] $Path) {
    $output = Invoke-Tool $Dumpbin @('-dependents', $Path)
    if ($null -eq $output) { return $null }
    $imports = New-Object Collections.ArrayList
    $collecting = $false
    foreach ($line in $output) {
        $text = "$line"
        if ($text -match 'following\s+(delay-load\s+)?dependencies') { $collecting = $true; continue }
        # The Summary section that follows lists section names, not libraries.
        if ($text -match '^\s*Summary\s*$') { break }
        if (-not $collecting) { continue }
        $name = $text.Trim()
        if ($name -match '^\S+\.dll$') { [void] $imports.Add($name) }
    }
    return ,@($imports.ToArray())
}

# The allow-list, as five classes: $true when an import belongs to one of them, $false
# when it is an offender.
#
# The set is closed, and nothing in it is a special case for a particular library.
# 'whisper.dll' is the import this check exists to catch: a stray 'whisper.lib' under
# ThirdParty/whisper_cpp/build/ flips the build to a shared whisper, and every clone then
# has to resolve a DLL nobody ships (C-build). It is caught because it belongs to no
# class, not because it is named here.
#
# Two of the classes ask a provider instead of keeping a list, which is what keeps them
# closed without going stale: an engine module is one the installed engine actually
# ships, and a Windows library is one System32 answers to. Both are exactly the question
# a user's loader will ask.
function Test-ImportAllowed {
    param(
        [string]   $Import,
        [string[]] $FirstParty,
        [string[]] $EngineModules,
        [switch]   $AceModule,
        [string]   $Configuration = ''
    )
    # A module this plugin ships, named by one of its own committed manifests.
    if ($FirstParty -contains $Import) { return $true }
    # An engine module, which is to say one the installed engine actually ships.
    if ($EngineModules -contains $Import) { return $true }
    # The Microsoft C and C++ runtime, from the Visual C++ redistributable every
    # Unreal install already requires.
    foreach ($pattern in @('api-ms-win-*.dll', 'ucrtbase*.dll', 'vcruntime140*.dll', 'msvcp140*.dll', 'concrt140*.dll')) {
        if ($Import -like $pattern) { return $true }
    }
    # A Windows library, which is to say a name System32 answers to.
    if (Test-Path -LiteralPath (Join-Path (Join-Path "$env:SystemRoot" 'System32') $Import)) {
        return $true
    }
    # The SWI-Prolog runtime, committed beside the modules. The packs beside it are loaded
    # by libswipl itself at runtime and so never appear as imports, which is why one name
    # covers the whole Prolog runtime.
    if ($Import -eq 'libswipl.dll') { return $true }
    # FANTASIAACE alone. The core plugin is ACE-agnostic, and an ACE import appearing in
    # it would mean the split had been undone.
    #
    # $Configuration is the decoration Unreal puts on a module DLL built for anything but
    # Development ('-Win64-DebugGame'), carried over from the importing module's own name:
    # a DebugGame build of FANTASIAACE links the DebugGame build of ACE. Deriving it from
    # the importer rather than accepting a wildcard keeps these two names exact.
    $aceModules = @(
        "UnrealEditor-ACECore$Configuration.dll",
        "UnrealEditor-ACERuntime$Configuration.dll")
    if ($AceModule -and ($aceModules -contains $Import)) { return $true }
    return $false
}

# Every asset name bootstrap.bat fetches, against the release it points at.
#
# The tag, the repository and the asset names are read out of the script, because that
# script is the only place that knows where the assets live (P-17). Reading the tag
# matters as much as reading the names: the 5.8 line fetches 'runtime-v3.0' and the
# 5.6/5.7 lines 'runtime-v2.0', so a copy kept here would eventually report an asset
# missing from a release it was never on.
#
# bootstrap.bat and not bootstrap.sh. The Linux GPU asset bootstrap.sh names has never
# been published (F-015, minor, open, and not this sprint's), and NG-2 with P-09 make
# Win64 the platform this project supports, so checking it would make the gate red on a
# known finding rather than on a defect.
#
# This is availability and nothing else. No digest is compared here or anywhere else in
# the project (F-026, major, security, open): a name that resolves says nothing about
# what the bytes are.
function Get-ReleaseAssetLiveness([string] $BootstrapPath) {
    $result = [ordered] @{
        state      = 'unknown'
        script     = $BootstrapPath
        tag        = 'unknown'
        repository = 'unknown'
        requested  = @()
        missing    = @()
        zeroLength = @()
        note       = 'unknown'
    }
    if (-not (Test-Path -LiteralPath $BootstrapPath)) {
        Stop-Gate 17 "there is no '$BootstrapPath', so the assets a user's install downloads cannot be read and nothing can be checked against the release."
    }

    $tag = $null
    $repository = $null
    $requested = New-Object Collections.ArrayList
    foreach ($line in (Get-Content -LiteralPath $BootstrapPath)) {
        if ($line -match '^\s*set\s+RELEASE_TAG=(\S+)\s*$') { $tag = $Matches[1]; continue }
        if ($line -match 'github\.com/([^/\s]+)/([^/\s]+)/releases/download') {
            $repository = "$($Matches[1])/$($Matches[2])"
            continue
        }
        if ($line -match '^\s*set\s+[A-Za-z0-9_]+_ASSET=(\S+)\s*$') { [void] $requested.Add($Matches[1]) }
    }
    if ($null -eq $tag -or $null -eq $repository -or $requested.Count -eq 0) {
        Stop-Gate 17 "'$BootstrapPath' names release tag '$tag', repository '$repository' and $($requested.Count) asset(s); all three are needed to check the release, so this run cannot tell whether a user's install would find its downloads."
    }
    $result.tag = $tag
    $result.repository = $repository
    $result.requested = @($requested.ToArray())

    # One request for the whole release rather than one per asset: it answers with every
    # published name and size at once, which is also the only way to see that a name is
    # published with nothing behind it.
    $api = "https://api.github.com/repos/$repository/releases/tags/$tag"
    # Windows PowerShell 5.1 still negotiates SSL 3.0 and TLS 1.0 by default, which
    # github.com refuses outright.
    try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch { }
    $release = $null
    try {
        $release = Invoke-RestMethod -Uri $api -Headers @{ 'Accept' = 'application/vnd.github+json' } `
            -UserAgent 'release-asset-liveness-check' -TimeoutSec 25 -ErrorAction Stop
    }
    catch {
        $status = $null
        if ($null -ne $_.Exception.Response) { $status = [int] $_.Exception.Response.StatusCode }
        if ($status -eq 404) {
            $result.state = 'failed'
            $result.missing = @($requested.ToArray())
            $result.note = "the release '$tag' does not exist on $repository (HTTP 404 from $api), so not one of the $($requested.Count) assets bootstrap.bat fetches can be downloaded."
            return $result
        }
        # Everything else taught this run nothing about the release: no network, a proxy,
        # an unauthenticated rate limit (HTTP 403), a GitHub outage. It is recorded as
        # unchecked and never as a pass, so the record cannot imply a check that was not
        # made, and the gate stays usable offline.
        $result.state = 'unchecked'
        if ($null -eq $status) {
            $result.note = "$api could not be reached, so the $($requested.Count) asset name(s) were not checked: $($_.Exception.Message)"
        }
        else {
            $result.note = "$api answered HTTP $status, which says nothing about the assets, so the $($requested.Count) asset name(s) were not checked."
        }
        return $result
    }

    $missing = New-Object Collections.ArrayList
    $zeroLength = New-Object Collections.ArrayList
    foreach ($name in $requested) {
        # -ceq, because a release asset's name is case-sensitive in the download URL
        # bootstrap.bat builds: a name matching only in case would still 404 there.
        $asset = @($release.assets | Where-Object { $_.name -ceq $name }) | Select-Object -First 1
        if ($null -eq $asset) { [void] $missing.Add($name); continue }
        if ([long] $asset.size -le 0) { [void] $zeroLength.Add($name) }
    }
    $result.missing = @($missing.ToArray())
    $result.zeroLength = @($zeroLength.ToArray())
    $published = @($release.assets).Count

    if ($missing.Count -gt 0 -or $zeroLength.Count -gt 0) {
        $result.state = 'failed'
        $parts = New-Object Collections.ArrayList
        if ($missing.Count -gt 0) { [void] $parts.Add("absent from it: '$($missing -join "', '")'") }
        if ($zeroLength.Count -gt 0) { [void] $parts.Add("published with a zero size: '$($zeroLength -join "', '")'") }
        $result.note = "of the $($requested.Count) asset(s) bootstrap.bat fetches from '$tag' on $repository, which publishes $published, the following would not install -- $($parts -join '; ')."
        return $result
    }
    $result.state = 'passed'
    $result.note = "all $($requested.Count) asset(s) bootstrap.bat fetches are present on '$tag' with a non-zero size, among the $published published there."
    return $result
}

# --- the committed module manifests, against this engine -----------------------

$pluginBinaries = [ordered] @{ 'FANTASIA' = $BinariesDir }
# FANTASIAACE is unreachable from the harness, but its committed binaries are in this
# repository and are what a user receives, so the conformance checks cover it (US-021)
# even though no test of it can run here.
if (Test-Path -LiteralPath $AceDescriptor) { $pluginBinaries['FANTASIAACE'] = $AceBinariesDir }

$manifests = New-Object Collections.ArrayList
foreach ($plugin in @($pluginBinaries.Keys)) {
    $set = Get-CommittedModules $plugin $pluginBinaries[$plugin]
    if ($set.Count -eq 0) {
        Stop-Gate 13 "'$($pluginBinaries[$plugin])' holds no '.modules' manifest, so $plugin ships no prebuilt module and a user who compiles nothing receives nothing of it (US-019)."
    }
    foreach ($entry in $set) { [void] $manifests.Add($entry) }
}

$moduleRecords = New-Object Collections.ArrayList
$firstPartyDlls = New-Object Collections.ArrayList
foreach ($entry in $manifests) {
    if ($entry.buildId -ne "$expectedBuildId") {
        Stop-Gate 13 "BuildId mismatch in the committed binaries: '$($entry.manifest)' says $($entry.buildId), the engine's effective compatible changelist is $expectedBuildId. A user enabling $($entry.plugin) would be told it was built with a different engine version, and no compile can fix a committed file."
    }
    foreach ($module in $entry.modules) {
        if (-not (Test-Path -LiteralPath $module.dll)) {
            Stop-Gate 13 "'$($entry.manifest)' names module '$($module.name)' as '$(Split-Path -Leaf $module.dll)', which is not beside it. The manifest promises a module the plugin does not ship, so the editor would fail to load $($entry.plugin)."
        }
        [void] $firstPartyDlls.Add((Split-Path -Leaf $module.dll))
    }
    [void] $moduleRecords.Add([ordered] @{
        plugin   = $entry.plugin
        manifest = (Split-Path -Leaf $entry.manifest)
        buildId  = $entry.buildId
        modules  = @($entry.modules | ForEach-Object { $_.name })
    })
}
$script:Run.modules = @($moduleRecords.ToArray())
Write-Step "$($manifests.Count) committed manifest(s) in $($pluginBinaries.Count) plugin(s) carry BuildId $expectedBuildId, naming $($firstPartyDlls.Count) module DLL(s)"

# --- those binaries against the source they ship for ---------------------------
# The prebuilt mode's precondition, and the compiled mode's observation. See the function
# above for the two frames; the asymmetry is the point and is in the exit-code list.

$freshness = Get-BinariesFreshness $RepoRoot @($pluginBinaries.Keys) $script:Run.tree
$script:Run.binaries = $freshness
if ($freshness.state -eq 'failed') {
    if ($Prebuilt) {
        Stop-Gate 19 "$($freshness.note). In this mode the committed DLLs are the artifact under test, so a green run would claim the shipped artifact passes the suite in this tree when it holds a different one. Refresh the binaries and commit them with their source, then run the prebuilt mode again; to test the source as it stands, run the compiled mode."
    }
    Write-Step "note: $($freshness.note). The compiled mode is about to rebuild those modules, so this does not fail here -- but the committed binaries a user receives are behind this tree, and the prebuilt mode refuses until they are refreshed (P-12)."
}
elseif ($freshness.state -eq 'unchecked') {
    Write-Step "committed binaries against their source NOT CHECKED: $($freshness.note)"
}
else {
    Write-Step "committed binaries against their source: $($freshness.note)"
}
Write-Step "artifact under test: $($freshness.artifact)"

# --- the imports of every module DLL those manifests name ----------------------

$dumpbin = Find-Dumpbin
if ($null -eq $dumpbin) {
    Stop-Gate 14 "dumpbin.exe was not found, so what the committed module DLLs import is unknown and this run refuses rather than pass a check it did not make (P-04). It ships with the Visual C++ toolchain, and vswhere found no installation carrying one. Install the 'Desktop development with C++' workload, or put dumpbin.exe on PATH."
}

$engineModuleDir = Join-Path $engineRoot 'Engine\Binaries\Win64'
$engineModules = @(
    Get-ChildItem -LiteralPath $engineModuleDir -Filter 'UnrealEditor-*.dll' -File -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Name })

$importRecords = New-Object Collections.ArrayList
foreach ($entry in $manifests) {
    $isAce = ($entry.plugin -eq 'FANTASIAACE')
    foreach ($module in $entry.modules) {
        $imports = Get-DllImports $dumpbin $module.dll
        if ($null -eq $imports -or $imports.Count -eq 0) {
            Stop-Gate 14 "dumpbin named no dependency of '$($module.dll)', so its imports are unknown. Every module DLL imports at least the C runtime, so this is the tool failing rather than an image with nothing to resolve, and the check is refused rather than skipped (P-04)."
        }
        # The build configuration this DLL is for, as Unreal decorates the file name, so
        # that the modules it links can be recognised in the same configuration.
        $configuration = ''
        if ((Split-Path -Leaf $module.dll) -match '(-Win64-[A-Za-z]+)\.dll$') { $configuration = $Matches[1] }
        $offenders = New-Object Collections.ArrayList
        foreach ($import in $imports) {
            # A name is all a loader gets, so a name is all this accepts: anything
            # carrying a path separator is not a library name and is not looked up.
            if ($import -match '[\\/:]') { [void] $offenders.Add($import); continue }
            $allowed = Test-ImportAllowed -Import $import -FirstParty @($firstPartyDlls.ToArray()) `
                -EngineModules $engineModules -AceModule:$isAce -Configuration $configuration
            if (-not $allowed) { [void] $offenders.Add($import) }
        }
        if ($offenders.Count -gt 0) {
            Stop-Gate 15 "'$($module.dll)' imports '$($offenders -join "', '")', which this plugin neither ships nor stages and neither the engine nor Windows provides. A clone would fail to load $($entry.plugin) with no way to fix it but a rebuild (C-build). The full import list of every module checked is in this run's line in '$script:HistoryFile'."
        }
        [void] $importRecords.Add([ordered] @{
            plugin  = $entry.plugin
            module  = $module.name
            dll     = (Split-Path -Leaf $module.dll)
            imports = @($imports)
        })
    }
}
$script:Run.imports = [ordered] @{
    tool    = $dumpbin
    checked = @($importRecords.ToArray())
}
Write-Step "$($importRecords.Count) module DLL(s) import only what is allowed; no library outside the engine, Windows, the C/C++ runtime and what the plugins ship"

# --- the line endings a user's scripts arrive with ------------------------------
# bootstrap.bat is the first command a new user runs, and cmd reads a batch file in blocks
# and resumes at a byte offset: an LF-only .bat therefore makes it execute fragments of the
# script's own text. Measured on the committed blob, driven through successful extractions:
# the LF form exits 255 having run 18 fragments as commands and fetched nothing, three runs
# out of three, while the same bytes as CRLF complete and install everything (F-071).
#
# What stops that is an attribute, so an attribute is what this checks. The committed blob
# stays LF either way -- that is the normalised form for a text file and is not the thing
# under test -- so the assertion is on the 'eol' attribute, which decides what a checkout
# and 'git archive' deliver, and on the working copy matching it, which is what catches a
# rule added without renormalising.
#
# Scope: the scripts directly beside bootstrap.bat, enumerated from git rather than listed,
# so a script added there is covered the moment it is tracked. The nine vendored scripts
# under FANTASIA/ThirdParty/ are in the same state and are not ours to renormalize, and no
# .ps1 is checked: PowerShell reads either form.
$expectedEol = @{ '.bat' = 'crlf'; '.sh' = 'lf' }

# The condition is 'there is no index to read', not 'there is no git': an unpacked archive
# of this repository has git on PATH and no repository under it, and that is the tree a
# Blueprint-only user actually has. With no index there are no delivery rules that could
# have regressed, and the scripts on disk are simply the bytes the user already has -- so
# this is recorded as unchecked and is not a refusal. Exit 18 is kept for what it is for: a
# git that failed INSIDE a repository, where the rules exist and could not be read.
$noIndex = ($null -eq $script:GitExe) -or ("$($script:Run.tree.commit)" -eq 'unknown')
if ($noIndex) {
    $why = if ($null -eq $script:GitExe) { 'no git on PATH' } else { 'this tree is not a git repository' }
    $script:Run.scripts = "unchecked: $why, so there is no index whose delivery rules could regress"
    Write-Step "script line endings NOT CHECKED: $why, so there is no index whose delivery rules could regress"
}
else {
    # ':(glob)' is load-bearing. In a gitattributes pattern '*' does not cross a '/', but in
    # an ORDINARY PATHSPEC it does, so 'FANTASIA/*.sh' reaches the vendored scripts under
    # FANTASIA/ThirdParty/ and this check would then demand attributes on files that are not
    # ours to renormalize. The glob magic gives the pathspec the attribute pattern's meaning,
    # which is the one the rule in .gitattributes has.
    $pluginName = Split-Path -Leaf $PluginDir
    $tracked = Invoke-Git @(
        '-C', $RepoRoot, 'ls-files', '--eol', '--',
        ":(glob)$pluginName/*.bat", ":(glob)$pluginName/*.sh")
    if ($null -eq $tracked) {
        Stop-Gate 18 "'git ls-files --eol' failed in '$RepoRoot', so the line endings the first-party scripts are delivered with could not be read. The check is refused rather than reported as finding nothing."
    }
    $scriptRecords = New-Object Collections.ArrayList
    $wrong = New-Object Collections.ArrayList
    foreach ($line in @($tracked)) {
        # 'i/<index>\tw/<worktree>\tattr/<attrs>\t<path>', with the columns padded. The path
        # is taken from after the last tab rather than by a column regex, because the attr
        # field holds 'text eol=lf' -- two words -- and a regex expecting one silently
        # skipped every file that had an attribute at all.
        $columns = "$line" -split "`t"
        if ($columns.Count -lt 2) { continue }
        $path = $columns[$columns.Count - 1].Trim()
        if ($columns[0] -notmatch '^i/(\S*)\s+w/(\S*)') { continue }
        $worktree = $Matches[2]
        $extension = [IO.Path]::GetExtension($path).ToLowerInvariant()
        if (-not $expectedEol.ContainsKey($extension)) { continue }
        $want = $expectedEol[$extension]
        # check-attr rather than the attr/ column, because that column abbreviates.
        $attr = @(Invoke-Git @('-C', $RepoRoot, 'check-attr', 'eol', '--', $path))
        $eol = 'unspecified'
        if ($attr.Count -gt 0 -and "$($attr[0])" -match ':\s*eol:\s*(\S+)\s*$') { $eol = $Matches[1] }
        if ($eol -ne $want) {
            [void] $wrong.Add("'$path' is delivered with eol '$eol' where '$want' is required, so what a user receives depends on the user's own core.autocrlf")
        }
        elseif ($worktree -ne $want) {
            [void] $wrong.Add("'$path' carries eol '$want' but its working copy is '$worktree', so the rule was added without renormalising and this tree is not what a clone would get")
        }
        [void] $scriptRecords.Add([ordered] @{ path = $path; eol = $eol; worktree = $worktree })
    }
    if ($scriptRecords.Count -eq 0) {
        Stop-Gate 18 "git lists no .bat or .sh beside '$PluginDir', so the install path's own scripts are not in this repository and their line endings cannot be checked."
    }
    if ($wrong.Count -gt 0) {
        Stop-Gate 18 "the line endings a user receives are wrong: $($wrong -join '; '). Line endings decide whether cmd and bash can read these scripts at all, and bootstrap is the first command a new user runs (F-071)."
    }
    $script:Run.scripts = [ordered] @{
        expected = $expectedEol
        checked  = @($scriptRecords.ToArray())
    }
    Write-Step "$($scriptRecords.Count) script(s) beside bootstrap.bat are delivered CRLF for .bat and LF for .sh, and this tree matches"
}

# --- the release assets a user's install downloads -----------------------------

$liveness = Get-ReleaseAssetLiveness $BootstrapScript
$script:Run.releaseAssets = $liveness
if ($liveness.state -eq 'failed') {
    Stop-Gate 16 "release assets: $($liveness.note) A default install would stop there, so this run refuses before compiling anything."
}
if ($liveness.state -eq 'unchecked') {
    Write-Step "release assets NOT CHECKED: $($liveness.note)"
}
else {
    Write-Step "release assets: $($liveness.note)"
}

# ------------------------------------------------------------------ harness ----
# Outside this repository's working tree, so a gate run never dirties anything but
# the binaries a compile produces -- and in the prebuilt mode, which compiles nothing,
# it writes nothing inside this repository at all.

$uproject = Join-Path $harnessRoot "$HarnessName.uproject"
$sourceDir = Join-Path $harnessRoot 'Source'

if (-not (Test-Path -LiteralPath $harnessRoot)) {
    New-Item -ItemType Directory -Path $harnessRoot -Force | Out-Null
}

# One gate run at a time, in either mode (F-035). One lock, and it sits above both
# harnesses rather than inside the one this run uses, because what two runs would corrupt
# is not a harness: it is the plugin tree they share. The compiled mode rewrites the
# plugin's Binaries\Win64 and its Intermediate; the prebuilt mode reads that same
# Binaries\Win64 as the artifact under test. Letting the two overlap would have the
# prebuilt mode report on a DLL being relinked underneath it -- a green verdict on bytes
# no user will ever receive -- so the second run refuses here instead.
#
# The lock is this file handle, not the file's existence: it is opened for writing and
# shared for reading only, so a second run's identical open is denied while a reader can
# still see who holds it. Nothing has to clean it up, because the operating system closes
# the handle whenever this process ends, crash included.
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
    Stop-Gate 11 "another gate run holds the gate lock '$lockFile', so this run refuses rather than race it. The lock covers both modes, because they share the plugin tree one of them rewrites. The holder reports: $holder. Wait for it to finish and run again."
}

# The mode is in the holder's identity, so that a refused run can tell whether it is
# waiting behind a compile or behind a read of the binaries.
$identity = "pid $PID, run $runId, mode $Mode, started $((Get-Date).ToString('s'))"
$identityBytes = [Text.Encoding]::UTF8.GetBytes($identity)
$script:GateLock.Write($identityBytes, 0, $identityBytes.Length)
$script:GateLock.Flush()

Write-Step "run $runId holds the gate lock, which covers both modes"

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

$script:Run.harness.project = $uproject

if ($Prebuilt) {
    # A CONTENT-ONLY host project, which is the mode's first claim and the reason it can
    # say anything about a Blueprint-only user at all. There is no "Modules" key, so the
    # project declares no C++ module of its own, and there is no Source directory for a
    # build tool to find: UnrealBuildTool is never invoked, and there is nothing it could
    # have been invoked on. The editor the engine already ships boots this project and
    # loads FANTASIA's committed module DLLs through the junction, exactly as a user's
    # project does.
    Set-FileIfChanged $uproject @"
{
	"FileVersion": 3,
	"EngineAssociation": "$requiredLine",
	"Description": "Generated content-only host project for FANTASIA's gate in prebuilt mode (FANTASIA/run-tests.ps1). It declares no module and has no Source directory, so nothing here is ever compiled. Regenerated on demand; do not edit.",
	"Plugins": [
		{
			"Name": "FANTASIA",
			"Enabled": true
		}
	]
}
"@

    # Proved rather than assumed, and in the two places the claim could fail: a Source tree
    # left behind by an earlier run or by a hand edit would make this a code project, and a
    # "Modules" key would make the editor demand a build of a module that does not exist.
    # The Source directory is removed rather than merely refused, because it is generated
    # and outside this repository -- then the absence is asserted, so that a removal which
    # did not take is a refusal and not a silent pass.
    if (Test-Path -LiteralPath $sourceDir) {
        Remove-Item -LiteralPath $sourceDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $sourceDir) {
        Stop-Gate 20 "'$sourceDir' still exists, so the host project is a code project and this mode cannot claim that nothing was compiled. Delete it and run again."
    }
    $declared = @((Get-Content -LiteralPath $uproject -Raw | ConvertFrom-Json).Modules)
    if (@($declared | Where-Object { $null -ne $_ }).Count -gt 0) {
        Stop-Gate 20 "'$uproject' declares $(@($declared).Count) module(s), so the editor would require them to be built. The prebuilt mode's host project must declare none."
    }
    $script:Run.harness.hostProjectHasSource = $false
    # '${uproject}:' and not '$uproject:', which PowerShell reads as a drive-qualified
    # variable and refuses to parse.
    Write-Step "content-only harness project ${uproject}: no Source directory, no declared module, no build tool invoked"
}
else {

$script:Run.harness.hostProjectHasSource = $true

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

Write-Step "code harness project $uproject"

}
# The compiled mode's generated files are written at column zero rather than indented
# inside the branch above: a here-string's closing '"@' must start the line, so the whole
# block cannot be indented without changing the file contents it writes.

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

# --------------------------------------------------------- compile, or not ----
# The stamp is taken in both modes and for opposite reasons: the compiled mode expects the
# tracked binaries to change and says so (P-12), and the prebuilt mode requires them not
# to, which is checked again after the editor has gone.
$binariesBefore = Get-BinariesStamp $BinariesDir

if ($Prebuilt) {
    # The whole of this mode's difference is here: no write probe, no makefile, no
    # Build.bat, no UnrealBuildTool. There is nothing to compile either, the host project
    # having no module and no Source directory, so this is not a step that was skipped --
    # it is a step that does not exist in this mode.
    #
    # One observation, so that nobody reading an editor log here mistakes it for a compile:
    # the editor starts UnrealBuildTool itself, in '-Mode=ValidatePlatforms', to report
    # which platform SDKs are installed. That mode builds nothing. What makes 'nothing
    # compiled' decidable is not the absence of that process -- which belongs to the engine
    # and not to this script -- but the four facts around it: no Source directory, no
    # declared module, Build.bat never invoked, and the plugin's tracked binaries unchanged
    # across the run (exit 20).
    #
    # The BuildId is not re-read from an emitted manifest, because nothing emits one. The
    # same invariant is already settled for all four committed manifests, in both plugins,
    # by the pre-flight above (exit 13), which is a stronger check than the compiled
    # mode's: it reads what a user receives rather than what this machine just produced.
    $script:Run.engine.emittedBuildId = 'not applicable: nothing was compiled, and the committed manifests were checked against this engine in the pre-flight'
    Write-Step "nothing is compiled in this mode; the suite will run from the committed DLLs in '$BinariesDir'"
}
else {
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
    $script:Run.engine.emittedBuildId = $emittedBuildId
    if ("$emittedBuildId" -ne "$expectedBuildId") {
        Stop-Gate 7 "BuildId mismatch: '$modulesFile' says $emittedBuildId, the engine's CompatibleChangelist is $expectedBuildId. Users would be told the plugin was built with a different engine version."
    }
    Write-Step "BuildId $emittedBuildId matches the engine"
}

# -------------------------------------------------------------------- tests ----

Write-Step "running 'Automation RunTests $Tests' headless"

$editorArgs = @(
    "$uproject",
    "-ExecCmds=`"Automation RunTests $Tests`"",
    "-TestExit=`"Automation Test Queue Empty`"",
    "-ReportExportPath=`"$script:ReportDir`"",
    '-unattended',
    '-nopause',
    '-nosplash',
    '-nullrhi',
    '-nosound',
    '-NoP4',
    "-abslog=`"$editorLog`""
)
# Belt and braces in the prebuilt mode: the editor is told not to build, as well as given
# a project with nothing it could build. A user who never installed a toolchain gets the
# same refusal from the engine, so this is also the path under test.
if ($Prebuilt) { $editorArgs += '-nocompile' }
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

# The prebuilt mode's second half of 'nothing compiled': the host project could not have
# been built, and the artifact under test was not rewritten while it was being tested. The
# compiled mode has no equivalent check, because there it is the opposite that is expected.
if ($Prebuilt -and (Get-BinariesStamp $BinariesDir) -ne $binariesBefore) {
    Stop-Gate 20 "the tracked binaries under '$BinariesDir' changed while this run was reading them, so something compiled or relinked during a run whose claim is that nothing did. The verdict would be about bytes no user receives. Editor log: '$editorLog'."
}

# Guard the one failure mode that makes a gate worse than none: the suite running against
# something other than the artifact the mode is about. The editor loads a module by
# BuildId, not by timestamp, so a DLL in the wrong place whose BuildId still matches would
# report green.
#
# The acceptable set is built per mode, and the two readings are opposites that must not be
# merged (ADR-0016, ADR-0017). Under the junction they happen to name the same physical
# directory today, which is exactly why the distinction has to be written down rather than
# left to coincide: the compiled mode accepts the directory IT COMPILED INTO, and would
# refuse the committed DLL if a future mechanism separated the two again; the prebuilt mode
# accepts the plugin's own committed Binaries\Win64 and nothing else -- not the host
# project's Binaries, which is where an external plugin's modules would land, and not an
# engine Plugins or Marketplace copy.
if (Test-Path -LiteralPath $editorLog) {
    $loadLine = Select-String -LiteralPath $editorLog -Pattern "InternalLoadLibrary: 'FANTASIATests' \('(.+?)'\)" |
        Select-Object -Last 1
    if ($loadLine) {
        $loadedFrom = $loadLine.Matches[0].Groups[1].Value.Replace('/', '\')
        $loadedDir = (Split-Path -Parent $loadedFrom).TrimEnd('\')
        # Both spellings of the one directory: the plugin's real path, and the same
        # directory as the editor sees it through the harness junction.
        $pluginBinariesSpellings = @(
            $BinariesDir.TrimEnd('\'),
            (Join-Path $junction 'Binaries\Win64'))
        if ($Prebuilt) {
            if ($pluginBinariesSpellings -notcontains $loadedDir) {
                Stop-Gate 10 "the editor loaded FANTASIATests from '$loadedFrom', which is not the plugin's own committed Binaries\Win64 ('$BinariesDir'). This mode's claim is about the DLLs this repository ships, so a suite run from anywhere else proves nothing about them."
            }
            Write-Step "editor loaded FANTASIATests from the plugin's own committed Binaries\Win64, and from nowhere else"
        }
        else {
            if ($pluginBinariesSpellings -notcontains $loadedDir) {
                Stop-Gate 10 "the editor loaded FANTASIATests from '$loadedFrom', which is not the directory this run compiled into ('$BinariesDir'). The suite therefore did not test the code that was just built."
            }
            Write-Step "editor loaded FANTASIATests from the directory this run compiled into"
        }
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
$script:Run.tests.reported = $reported.Count

$notPassed = @($reported | Where-Object { $_.state -ne 'Success' -and $_.state -ne 'SucceededWithWarnings' })
$script:Run.tests.notPassed = $notPassed.Count
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

$script:Run.verdict = 'PASSED'
$script:Run.exitCode = 0
$script:Run.reason = "$($reported.Count) tests, 0 failures, prefix '$Tests', mode '$Mode'"
if (-not (Write-GateRecord)) {
    Stop-Gate 12 "the suite passed on tree $(Get-TreeLabel), but the run could not be appended to '$script:HistoryFile'. A verdict no record names is the one this gate exists to stop being quoted later, so it is withheld." -SkipRecord
}

Remove-GateLock
Write-Host ''
Write-Step "PASSED ($Mode): $($reported.Count) tests, 0 failures, prefix '$Tests', on tree $(Get-TreeLabel)"
Write-Step $(if ($Prebuilt) { "this verdict is about the committed module binaries ($($freshness.artifact)), not about the source: nothing was compiled" } else { 'this verdict is about the source this run compiled, not about the committed binaries a user receives' })
Write-Step "recorded in '$script:HistoryFile' as run $runId"
Write-Host ("[gate] elapsed {0:n1} s" -f $started.Elapsed.TotalSeconds)
exit 0
