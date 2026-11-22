<#
.SYNOPSIS
    Drives the gate's own Get-ReleaseAssetLiveness (extracted from run-tests.ps1 by AST, not retyped)
    against a mocked GitHub API and against crafted copies of bootstrap.bat, and prints the state it
    reaches for each. Writes nothing inside the repository.

.DESCRIPTION
    Without -Real it makes no network request. With -Real it also calls the real cmdlet once against the
    live release and once through a dead proxy (the offline path); the live call spends one of GitHub's
    60 unauthenticated requests per hour per address, the same budget a gate run draws on.

    Expected, and what each row shows: a renamed asset, a zero-length or null-size asset, a release with
    no assets, names differing only in case or by a trailing space: failed. HTTP 404: failed. HTTP 403,
    500, no route: unchecked, never passed. Crafted bootstrap.bat files: an asset named with a quoted
    set "NAME_ASSET=value", or followed by a trailing comment, is NOT requested, so the check passes
    having looked at one fewer asset.
#>
param([switch] $Real)
# Drive the gate's own Get-ReleaseAssetLiveness (extracted by AST, not retyped) against a
# mocked GitHub API and against crafted bootstrap.bat files.
$ErrorActionPreference = 'Stop'
$sp = Join-Path $env:TEMP 'fantasia-liveness'
New-Item -ItemType Directory -Force $sp | Out-Null
$plugin = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$gatePath = Join-Path $plugin 'run-tests.ps1'
$ast = [Management.Automation.Language.Parser]::ParseFile($gatePath, [ref]$null, [ref]$null)
$fn = $ast.FindAll({ param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Get-ReleaseAssetLiveness' }, $true) | Select-Object -First 1
. ([scriptblock]::Create($fn.Extent.Text))
function Stop-Gate([int] $Code, [string] $Message) { throw "STOP-GATE $Code : $Message" }

$realBat = Join-Path $plugin 'bootstrap.bat'
$names = @('whisper-large-v3-turbo-q5_0.bin','fantasia-gpu-cuda13-win64.tar.gz','fantasia-build-deps-win64.tar.gz','nv-ace-reference-ue58-full.tar.gz','nv-audio2face-mark-ue58.tar.gz','nv-audio2face-james-ue58.tar.gz','nv-audio2face-claire-ue58.tar.gz')
function A($n, $size = 1000) { [pscustomobject]@{ name = $n; size = $size } }
$script:Api = { $null }
function Invoke-RestMethod { & $script:Api }
function Mock-Api([scriptblock] $Body) { $script:Api = $Body }
function Http-Error([int] $Code) {
  $ex = New-Object Exception "HTTP $Code"
  $ex | Add-Member -NotePropertyName Response -NotePropertyValue ([pscustomobject]@{ StatusCode = $Code }) -Force
  return $ex
}
function Show($label, $r) { "{0,-46} state={1,-9} req={2} missing=[{3}] zero=[{4}]" -f $label, $r.state, @($r.requested).Count, ($r.missing -join ','), ($r.zeroLength -join ',') }
function Run($label, $bat = $realBat) { try { Show $label (Get-ReleaseAssetLiveness $bat) } catch { "{0,-46} {1}" -f $label, ($_.Exception.Message.Substring(0, [Math]::Min(150, $_.Exception.Message.Length))) } }

# --- mocked API, real bootstrap.bat
Mock-Api { [pscustomobject]@{ assets = @($names | ForEach-Object { A $_ }) } };               Run 'all present'
Mock-Api { [pscustomobject]@{ assets = @($names | ForEach-Object { A $_ } | Where-Object { $_.name -ne 'nv-audio2face-james-ue58.tar.gz' }) + (A 'nv-audio2face-james-ue57.tar.gz') } }; Run 'one asset renamed (ue57)'
Mock-Api { [pscustomobject]@{ assets = @($names | ForEach-Object { A $_ $(if ($_ -like 'nv-ace*') { 0 } else { 1000 }) }) } }; Run 'ACE asset present, zero-length'
Mock-Api { [pscustomobject]@{ assets = @($names | ForEach-Object { A $_ $(if ($_ -like 'nv-ace*') { $null } else { 1000 }) }) } }; Run 'ACE asset present, size null'
Mock-Api { [pscustomobject]@{ assets = @() } };                                                  Run 'release exists, assets = []'
Mock-Api { [pscustomobject]@{ tag_name = 'runtime-v3.0' } };                                    Run 'release exists, no assets property'
Mock-Api { [pscustomobject]@{ assets = @($names | ForEach-Object { A $_.ToUpper() }) } };      Run 'names match only in case (all upper)'
Mock-Api { [pscustomobject]@{ assets = @($names | ForEach-Object { A ($_ + ' ') }) } };         Run 'names with trailing space'
Mock-Api { [pscustomobject]@{ assets = @(A $names[0]) } };                                      Run 'only one of seven present'
Mock-Api { [pscustomobject]@{ assets = @($names | ForEach-Object { A $_ }) + (A 'extra.zip') } }; Run 'extra unrequested asset (should pass)'
Mock-Api { throw (Http-Error 404) };                                                             Run 'HTTP 404 (release missing)'
Mock-Api { throw (Http-Error 403) };                                                             Run 'HTTP 403 (rate limit)'
Mock-Api { throw (Http-Error 500) };                                                             Run 'HTTP 500'
Mock-Api { throw (New-Object Net.WebException 'The remote name could not be resolved') };       Run 'offline (WebException, no response)'
Mock-Api { throw 'boom' };                                                                       Run 'arbitrary exception'
Mock-Api { $null };                                                                              Run 'API returns $null body'
Mock-Api { 'not json' };                                                                         Run 'API returns a string body'

# --- the REAL cmdlet, with and without a route to GitHub
if ($Real) {
  Remove-Item function:Invoke-RestMethod
  Run 'REAL network, real bootstrap.bat'
  [Net.WebRequest]::DefaultWebProxy = New-Object Net.WebProxy('http://127.0.0.1:9')
  Run 'REAL cmdlet, dead proxy (offline)'
  [Net.WebRequest]::DefaultWebProxy = $null
  function Invoke-RestMethod { & $script:Api }
}
# --- crafted bootstrap.bat files (API says everything is present, so only name extraction matters)
Mock-Api { [pscustomobject]@{ assets = @($names | ForEach-Object { A $_ }) } }
$txt = Get-Content $realBat -Raw
function Variant($label, [scriptblock] $edit) { $f = Join-Path $sp ("v-" + ($label -replace '\W','_') + '.bat'); [IO.File]::WriteAllText($f, (& $edit $txt)); Run $label $f }
Variant 'bat: ACE asset line in quoted set form' { param($t) $t.Replace('set ACE_ASSET=nv-ace-reference-ue58-full.tar.gz', 'set "ACE_ASSET=nv-ace-reference-ue58-full.tar.gz"') }
Variant 'bat: new quoted asset never published'   { param($t) $t.Replace('set DEPS_ASSET=', "set `"NEW_ASSET=does-not-exist.tar.gz`"`r`nset DEPS_ASSET=") }
Variant 'bat: new unquoted asset never published' { param($t) $t.Replace('set DEPS_ASSET=', "set NEW_ASSET=does-not-exist.tar.gz`r`nset DEPS_ASSET=") }
Variant 'bat: asset line with trailing comment'   { param($t) $t.Replace('set ACE_ASSET=nv-ace-reference-ue58-full.tar.gz', 'set ACE_ASSET=nv-ace-reference-ue58-full.tar.gz & REM x') }
Variant 'bat: RELEASE_TAG quoted'                 { param($t) $t.Replace('set RELEASE_TAG=runtime-v3.0', 'set "RELEASE_TAG=runtime-v3.0"') }
Variant 'bat: RELEASE_TAG trailing space'         { param($t) $t.Replace('set RELEASE_TAG=runtime-v3.0', 'set RELEASE_TAG=runtime-v3.0 ') }
Variant 'bat: no RELEASE_TAG'                     { param($t) $t.Replace('set RELEASE_TAG=runtime-v3.0', 'REM gone') }
Variant 'bat: no assets'                          { param($t) ($t -split "`r?`n" | Where-Object { $_ -notmatch '_ASSET=' }) -join "`r`n" }
Variant 'bat: no github url'                      { param($t) $t.Replace('https://github.com/antori82/FANTASIA/releases/download', 'https://example.com/x') }
Variant 'bat: github url for another repository'  { param($t) $t.Replace('antori82/FANTASIA/releases', 'someone/else/releases') }
Variant 'bat: empty file'                         { param($t) '' }
Run 'bat: file absent' (Join-Path $sp 'no-such.bat')
