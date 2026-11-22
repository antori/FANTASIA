<#
.SYNOPSIS
    Mutation check of FANTASIA.Install.Bootstrap.* : breaks a scratch copy of the install surface on
    purpose and shows which contract test goes red. A test never seen to fail is not known to work.

.DESCRIPTION
    Runs the suite out of the COMMITTED DLL (no compile) in the gate's prebuilt harness, with the plugin
    reached through a junction at a scratch copy whose bootstrap.bat, bootstrap.sh, enable-plugins.ps1 or
    FANTASIA.uplugin carries the mutation. Needs a gate harness (run the gate once first). Never writes
    inside the repository; the two harness junctions are restored afterwards.

    Each run uses a fresh scratch directory under %TEMP%antasia-mutation, because the editor leaves a
    service behind that holds the redirected output files open, so a directory cannot be reused.

    -DropCompiled removes the compiled harness' junction for the run, which is what a machine that never
    compiled the shipped DLL looks like (use -Set del-enable or any single mutation, or none that matters).

    -PointCompiledToo (default true) matters: the DLL reads the scripts through the path it was COMPILED through (the
    compiled harness' junction), not the one the editor loaded it from (see FANTASIA.Quality.Install.ContractTestsReadTheLoadedPlugin). Without it every
    mutant is judged against the real tree and nothing goes red.

    Mutations: none (control) infix scriptdir space-set guard-gpu guard-ace tag flag exit0 tar nobackup sh-nv (each
    expected red); p-amp p-paren p-qspace p-remguard p-rembackup p-lateuse (probes: p-amp, p-paren,
    p-remguard, p-rembackup are expected red and are green today); del-bat empty-bat blank-bat del-sh
    empty-sh del-enable ver-bad (expected: every dependent test red).

.EXAMPLE
    powershell -File BootstrapMutationCheck.ps1 -Set space-set
    powershell -File BootstrapMutationCheck.ps1 -Set p-remguard,p-rembackup -PointCompiledToo:$false
#>
param([Parameter(Mandatory)][string[]] $Set, [string] $Name = ('run-' + [guid]::NewGuid().ToString('N').Substring(0, 6)), [string] $Prefix = 'FANTASIA.Install', [bool] $PointCompiledToo = $true, [switch] $DropCompiled)
$ErrorActionPreference = 'Stop'
$real = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$sp = Join-Path $env:TEMP 'fantasia-mutation'
New-Item -ItemType Directory -Force $sp | Out-Null
$root = Join-Path $sp $Name
$dir = Join-Path $root 'FANTASIA'
$gate = Join-Path $env:LOCALAPPDATA 'FANTASIA-Gate'
$jc = Join-Path $gate 'FantasiaGate\Plugins\FANTASIA'
$jp = Join-Path $gate 'FantasiaGatePrebuilt\Plugins\FANTASIA'
function Get-JT($j) { if (Test-Path -LiteralPath $j) { (Get-Item -LiteralPath $j -Force).Target | Select-Object -First 1 } else { $null } }
function Set-J($j, $t) {
  if (Test-Path -LiteralPath $j) { [IO.Directory]::Delete($j) }
  if ($t) { & "$env:SystemRoot\System32\cmd.exe" /c mklink /J "$j" "$t" | Out-Null }
}
# Remove a previous scratch copy: its junctions first, by name and non-recursively. A recursive delete
# must never reach the plugin behind them (Windows PowerShell 5.1 follows junctions).
if (Test-Path $root) {
  foreach ($j in Get-ChildItem -LiteralPath $dir -Force -Directory -ErrorAction SilentlyContinue) {
    if ($j.Attributes -band [IO.FileAttributes]::ReparsePoint) { [IO.Directory]::Delete($j.FullName) }
  }
  Remove-Item $root -Recurse -Force
}
New-Item -ItemType Directory -Force $dir | Out-Null
foreach ($d in 'Binaries','Config','Content','Resources','Source','ThirdParty') {
  if (Test-Path (Join-Path $real $d)) { & "$env:SystemRoot\System32\cmd.exe" /c mklink /J "$dir\$d" "$real\$d" | Out-Null }
}
foreach ($f in 'FANTASIA.uplugin','bootstrap.bat','bootstrap.sh','enable-plugins.ps1') { Copy-Item (Join-Path $real $f) $dir }
function Read-Text($f) { [IO.File]::ReadAllText((Join-Path $dir $f)) }
function Write-Text($f, $t) { [IO.File]::WriteAllText((Join-Path $dir $f), $t) }
function Sub($f, $old, $new) {
  $t = Read-Text $f
  $i = $t.IndexOf($old)
  if ($i -lt 0 -or $t.IndexOf($old, $i + 1) -ge 0) { throw "mutation anchor not unique in ${f}: $old" }
  Write-Text $f $t.Replace($old, $new)
}
foreach ($m in $Set) {
  switch ($m) {
    'infix'        { Sub 'bootstrap.bat' 'set ACE_ASSET=nv-ace-reference-ue58-full.tar.gz' 'set ACE_ASSET=nv-ace-reference-ue57-full.tar.gz' }
    'scriptdir'    { Sub 'bootstrap.bat' "set SCRIPT_DIR=%~dp0`r`n" ''; Sub 'bootstrap.bat' "`r`n:args_done`r`n" "`r`n:args_done`r`nset SCRIPT_DIR=%~dp0`r`n" }
    'space-set'    { Sub 'bootstrap.bat' '( set "FETCH_MODEL=0" & shift' '( set FETCH_MODEL=0 & shift' }
    'guard-gpu'    { Sub 'bootstrap.bat' 'if "%FORCE%"=="0" if exist "%GPU_MARKER%" (' 'if exist "%GPU_MARKER%" (' }
    'guard-ace'    { Sub 'bootstrap.bat' ":do_ace`r`ncall :nv_prepare NV_ACE_Reference`r`nif errorlevel 1 exit /b 1`r`n" ":do_ace`r`n" }
    'tag'          { Sub 'bootstrap.bat' 'set RELEASE_TAG=runtime-v3.0' 'set RELEASE_TAG=runtime-v4.0' }
    'flag'         { $t = Read-Text 'bootstrap.bat'; $l = ($t -split "`r`n") | Where-Object { $_ -notmatch '^if /I "%~1"=="--no-mark"' }; Write-Text 'bootstrap.bat' ($l -join "`r`n") }
    'exit0'        { Sub 'bootstrap.bat' "exit /b 1`r`n:args_done" "exit /b 0`r`n:args_done" }
    'tar'          { Sub 'bootstrap.bat' '"%TAR%" -xf "%GPU_TGZ%"' 'tar -xf "%GPU_TGZ%"' }
    'nobackup'     { Sub 'enable-plugins.ps1' 'Copy-Item -LiteralPath $path -Destination $bak' 'Write-Verbose skipped' }
    'sh-nv'        { Sub 'bootstrap.sh' 'FETCH_DEPS=1' ('FETCH_DEPS=1' + "`n" + 'ACE_ASSET=nv-ace-reference-ue58-full.tar.gz') }
    # Probes: regressions the tests are meant to stop; each is expected red.
    'p-amp'        { Sub 'bootstrap.bat' '( set "FETCH_MODEL=0" & shift' '( set "FETCH_MODEL=0" &set FETCH_GPU=0 & shift' }
    'p-paren'      { Sub 'bootstrap.bat' '( set "FETCH_MODEL=0" & shift' '(set FETCH_MODEL=0 & shift' }
    'p-qspace'     { Sub 'bootstrap.bat' 'set "FETCH_MODEL=0" &' 'set "FETCH_MODEL=0 " &' }
    'p-remguard'   { Sub 'bootstrap.bat' 'if "%FORCE%"=="0" if exist "%GPU_MARKER%" (' 'REM if "%FORCE%"=="0" if exist "%GPU_MARKER%" (' }
    'p-rembackup'  { Sub 'enable-plugins.ps1' 'Copy-Item -LiteralPath $path -Destination $bak' '# Copy-Item -LiteralPath $path -Destination $bak $bak' }
    'p-lateuse'    { Sub 'bootstrap.bat' 'echo Extracting into %SCRIPT_DIR% ...' 'echo Extracting into %~dp0 ...' }
    # Absent and empty subjects.
    'ver-57'       { Sub 'FANTASIA.uplugin' '"EngineVersion": "5.8.0"' '"EngineVersion": "5.7.0"' }
    'ver-bad'      { Sub 'FANTASIA.uplugin' '"EngineVersion": "5.8.0"' '"EngineVersion": "banana"' }
    'del-bat'      { Remove-Item (Join-Path $dir 'bootstrap.bat') }
    'empty-bat'    { Write-Text 'bootstrap.bat' '' }
    'blank-bat'    { Write-Text 'bootstrap.bat' "`r`n" }
    'del-sh'       { Remove-Item (Join-Path $dir 'bootstrap.sh') }
    'empty-sh'     { Write-Text 'bootstrap.sh' '' }
    'del-uplugin'  { Remove-Item (Join-Path $dir 'FANTASIA.uplugin') }
    'empty-uplugin'{ Write-Text 'FANTASIA.uplugin' '' }
    'del-enable'   { Remove-Item (Join-Path $dir 'enable-plugins.ps1') }
    'none'         { }   # control: the unmutated copy
    default        { throw "unknown mutation $m" }
  }
}
"mutations: $($Set -join ', ')"
$origC = Get-JT $jc; $origP = Get-JT $jp
$report = Join-Path $sp "$Name\report"; $log = Join-Path $sp "$Name\editor.log"
try {
  Set-J $jp $dir
  if ($PointCompiledToo) { Set-J $jc $dir }
  if ($DropCompiled) { Set-J $jc $null }
  if (Test-Path $report) { Remove-Item $report -Recurse -Force }
  $uproject = Join-Path $gate 'FantasiaGatePrebuilt\FantasiaGatePrebuilt.uproject'
  $args = @("`"$uproject`"", "-ExecCmds=`"Automation RunTests $Prefix`"", '-TestExit="Automation Test Queue Empty"', "-ReportExportPath=`"$report`"", '-unattended','-nopause','-nosplash','-nullrhi','-nosound','-NoP4',"-abslog=`"$log`"",'-nocompile')
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $p = Start-Process -FilePath 'C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' -ArgumentList ($args -join ' ') -NoNewWindow -PassThru -RedirectStandardOutput (Join-Path $sp "$Name\out.txt") -RedirectStandardError (Join-Path $sp "$Name\err.txt")
  [void]$p.Handle; $p.WaitForExit()
  "editor exit $($p.ExitCode) in $([math]::Round($sw.Elapsed.TotalSeconds,1)) s"
}
finally {
  Set-J $jp $origP
  if ($PointCompiledToo -or $DropCompiled) { Set-J $jc $origC }
}
$ri = Join-Path $report 'index.json'
if (-not (Test-Path $ri)) { "NO REPORT"; return }
$r = Get-Content $ri -Raw | ConvertFrom-Json
$t = @($r.tests)
"tests reported: $($t.Count); not passed: $(@($t | Where-Object { $_.state -ne 'Success' -and $_.state -ne 'SucceededWithWarnings' }).Count)"
foreach ($x in $t | Where-Object { $_.state -ne 'Success' }) {
  "  $($x.state): $($x.fullTestPath)"
  foreach ($e in @($x.entries | Where-Object { $_.event.type -eq 'Error' }) | Select-Object -First 3) { "      E: $($e.event.message)" }
}
