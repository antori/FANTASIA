<#
.SYNOPSIS
    Security checks on the gate's own attack surface (FANTASIA/run-tests.ps1) and on the delivered
    form of the install entry point. One check per threat, each named after it.

.DESCRIPTION
    The gate is PowerShell, so it is attacked with PowerShell: every function and block under test is
    extracted from run-tests.ps1 by AST and executed, never retyped, so a change to the gate is a change
    to what these checks run. No check makes a network request (Invoke-RestMethod is shadowed by a
    recording mock), compiles anything, or writes inside this repository: scratch state lives in a
    fresh directory under %TEMP% and is removed at the end.

    These checks are not run by the gate: the project has no PowerShell test harness, and adding one is
    a dependency decision (P-06). Run them by hand from Windows PowerShell 5.1:

        powershell -NoProfile -ExecutionPolicy Bypass -File FANTASIA/Source/FANTASIATests/Private/Security/GateSurfaceSecurityChecks.ps1

    Each check prints PASS or FAIL with its name; the exit code is the number of failed checks.

    Each refusal check carries its dual, so that a check cannot pass against code that does nothing:
    the hostile URLs beside the legitimate one, the junction retargeted beside the junction left alone,
    the planted tool ignored from the current directory beside the same tool found once PATH names it,
    the CRLF delivery under this tree's attributes beside the LF delivery under the committed ones.
#>
$ErrorActionPreference = 'Stop'
Set-StrictMode -Off

$plugin = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$repoRoot = Split-Path -Parent $plugin
$gatePath = Join-Path $plugin 'run-tests.ps1'
$realBat = Join-Path $plugin 'bootstrap.bat'
$scratch = Join-Path $env:TEMP ('fantasia-gate-sec-' + [guid]::NewGuid().ToString('N').Substring(0, 8))
New-Item -ItemType Directory -Path $scratch | Out-Null

$ast = [Management.Automation.Language.Parser]::ParseFile($gatePath, [ref] $null, [ref] $null)
function Get-GateFunction([string] $Name) {
    $fn = $ast.FindAll({ param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq $Name }, $true) |
        Select-Object -First 1
    if ($null -eq $fn) { throw "run-tests.ps1 defines no function '$Name'" }
    return $fn.Extent.Text
}
foreach ($name in 'Invoke-ToolChecked', 'Invoke-Tool', 'Invoke-Git', 'Get-TreeIdentity', 'Find-Dumpbin', 'Get-ReleaseAssetLiveness') {
    . ([scriptblock]::Create((Get-GateFunction $name)))
}
# Stop-Gate exits the process in the gate; here it is a catchable refusal carrying its code.
function Stop-Gate([int] $Code, [string] $Message) { throw "STOP-GATE $Code : $Message" }

$script:Failures = 0
function Report([string] $Check, [string[]] $Problems) {
    if ($Problems.Count -eq 0) { Write-Host "PASS $Check" }
    else {
        $script:Failures++
        Write-Host "FAIL $Check"
        foreach ($p in $Problems) { Write-Host "     - $p" }
    }
}

# --- recording mock of the one network call ------------------------------------------------------
# Every parameter the real cmdlet accepts that could carry a credential, a proxy or a body is
# declared, so binding records it rather than failing; the check then asserts none was used.
$script:Calls = New-Object Collections.ArrayList
$script:Answer = { [pscustomobject] @{ assets = @() } }
function Invoke-RestMethod {
    [CmdletBinding()]
    param(
        [Uri] $Uri, $Headers, $UserAgent, $TimeoutSec, $Method, $Body, $ContentType,
        $Credential, [switch] $UseDefaultCredentials, $Certificate, $CertificateThumbprint,
        $Proxy, $ProxyCredential, [switch] $ProxyUseDefaultCredentials, $WebSession, $SessionVariable,
        $MaximumRedirection, $InFile, $OutFile, [switch] $DisableKeepAlive, $TransferEncoding)
    [void] $script:Calls.Add([pscustomobject] @{ Uri = $Uri; Bound = @($PSBoundParameters.Keys); Headers = $Headers })
    & $script:Answer
}
$sevenNames = @()
foreach ($line in (Get-Content -LiteralPath $realBat)) {
    if ($line -match '^\s*set\s+[A-Za-z0-9_]+_ASSET=(\S+)\s*$') { $sevenNames += $Matches[1] }
}
function All-Present { [pscustomobject] @{ assets = @($sevenNames | ForEach-Object { [pscustomobject] @{ name = $_; size = 1000 } }) } }
function Http-Error([int] $Code) {
    $ex = New-Object Exception "HTTP $Code"
    $ex | Add-Member -NotePropertyName Response -NotePropertyValue ([pscustomobject] @{ StatusCode = $Code }) -Force
    return $ex
}
$batText = [IO.File]::ReadAllText($realBat)
function Write-Variant([string] $Label, [string] $Text) {
    $f = Join-Path $scratch ('bat-' + ($Label -replace '\W', '_') + '.bat')
    [IO.File]::WriteAllText($f, $Text)
    return $f
}
function With-Tag([string] $Tag) { $batText.Replace('set RELEASE_TAG=runtime-v3.0', "set RELEASE_TAG=$Tag") }
function With-Owner([string] $Owner, [string] $Repo = 'FANTASIA') {
    $batText.Replace('https://github.com/antori82/FANTASIA/releases/download', "https://github.com/$Owner/$Repo/releases/download")
}

# =================================================================================================
# Injection: a release tag or repository read out of bootstrap.bat cannot move the request off
# https://api.github.com:443, carry user-info, or be split by whitespace into something else.
# Dual: the legitimate script yields exactly the documented URL.
$problems = @()
$script:Answer = { All-Present }
$script:Calls.Clear()
[void] (Get-ReleaseAssetLiveness $realBat)
$expected = 'https://api.github.com/repos/antori82/FANTASIA/releases/tags/runtime-v3.0'
if ($script:Calls.Count -ne 1 -or "$($script:Calls[0].Uri)" -ne $expected) {
    $problems += "dual: the legitimate bootstrap.bat requested '$(@($script:Calls | ForEach-Object { $_.Uri }) -join ', ')', expected exactly '$expected'"
}
$hostile = [ordered] @{
    'tag ../ traversal'          = With-Tag '../../../../users/attacker'
    'tag query'                  = With-Tag 'x?per_page=1&redirect=https://evil.example/'
    'tag fragment'               = With-Tag 'x#@evil.example'
    'tag userinfo'               = With-Tag '@evil.example:8443'
    'tag encoded slashes'        = With-Tag 'x%2F..%2F..%2F..%2Fevil'
    'tag backslash UNC'          = With-Tag '\\evil.example\share'
    'owner userinfo and port'    = With-Owner 'a@evil.example:8443'
    'owner and repo dot-dot'     = With-Owner '..' '..'
    'owner query'                = With-Owner 'evil.example?' 'x'
}
foreach ($label in $hostile.Keys) {
    $script:Calls.Clear()
    $stopped = $false
    try { [void] (Get-ReleaseAssetLiveness (Write-Variant $label $hostile[$label])) } catch { $stopped = "$_" -like 'STOP-GATE 17*' }
    # Not vacuous: each variant either reaches the request or is refused before it.
    if ($script:Calls.Count -eq 0 -and -not $stopped) { $problems += "$label -> neither a request nor a refusal" }
    foreach ($call in $script:Calls) {
        Write-Host ("     {0,-24} -> {1}" -f $label, $call.Uri.AbsoluteUri)
        $u = $call.Uri
        if ($u.Scheme -ne 'https' -or $u.Host -ne 'api.github.com' -or $u.Port -ne 443 -or $u.UserInfo -ne '') {
            $problems += "$label -> request to scheme '$($u.Scheme)' host '$($u.Host)' port $($u.Port) userinfo '$($u.UserInfo)' ($u)"
        }
    }
}
foreach ($tag in 'runtime-v3.0 evil', "runtime-v3.0`tx") {
    $script:Calls.Clear()
    $stopped = $false
    try { [void] (Get-ReleaseAssetLiveness (Write-Variant 'ws' (With-Tag $tag))) } catch { $stopped = "$_" -like 'STOP-GATE 17*' }
    if (-not $stopped -or $script:Calls.Count -ne 0) {
        $problems += "a tag containing whitespace was not refused with exit 17 before any request ($($script:Calls.Count) request(s) made)"
    }
}
Report 'Injection.ReleaseRequestHostConfinedUnderHostileTagAndRepository' $problems

# =================================================================================================
# Secret leakage: the request carries no credential of any kind, sends only an Accept header, and the
# gate overrides no certificate validation and reads no token from the environment.
$problems = @()
$script:Answer = { All-Present }
$script:Calls.Clear()
$env:GITHUB_TOKEN = 'ghp_' + 'SECURITYCHECKCANARY' + ('0' * 20)
try { [void] (Get-ReleaseAssetLiveness $realBat) } finally { Remove-Item Env:\GITHUB_TOKEN -ErrorAction SilentlyContinue }
$allowed = @('Uri', 'Headers', 'UserAgent', 'TimeoutSec', 'ErrorAction')
foreach ($call in $script:Calls) {
    $extra = @($call.Bound | Where-Object { $allowed -notcontains $_ })
    if ($extra.Count -gt 0) { $problems += "the request bound '$($extra -join "', '")'" }
    $headerKeys = @($call.Headers.Keys)
    if ($headerKeys.Count -ne 1 -or $headerKeys[0] -ne 'Accept') { $problems += "the request sent header(s) '$($headerKeys -join "', '")', expected Accept alone" }
    if (($call.Headers.Values -join ' ') -like '*ghp_SECURITYCHECKCANARY*') { $problems += 'the canary token in GITHUB_TOKEN reached a header' }
}
if ($script:Calls.Count -ne 1) { $problems += "expected one request, saw $($script:Calls.Count)" }
$gateText = [IO.File]::ReadAllText($gatePath)
foreach ($forbidden in 'ServerCertificateValidationCallback', 'SkipCertificateCheck', 'CheckCertificateRevocationList', 'GITHUB_TOKEN', 'GH_TOKEN', '-Credential', 'UseDefaultCredentials') {
    if ($gateText.IndexOf($forbidden, [StringComparison]::OrdinalIgnoreCase) -ge 0) { $problems += "run-tests.ps1 mentions '$forbidden'" }
}
Report 'SecretLeakage.ReleaseRequestCarriesNoCredentialAndKeepsCertificateValidation' $problems

# =================================================================================================
# Fail-open: a release that could not be asked about is 'unchecked', never 'passed'.
# Dual: the reachable release with every asset present is 'passed'.
$problems = @()
$cases = [ordered] @{
    'no route (WebException, no response)' = { throw (New-Object Net.WebException 'The remote name could not be resolved: api.github.com') }
    'rate limited (HTTP 403)'              = { throw (Http-Error 403) }
    'too many requests (HTTP 429)'         = { throw (Http-Error 429) }
    'outage (HTTP 503)'                    = { throw (Http-Error 503) }
}
foreach ($label in $cases.Keys) {
    $script:Answer = $cases[$label]
    $r = Get-ReleaseAssetLiveness $realBat
    if ($r.state -ne 'unchecked') { $problems += "$label -> state '$($r.state)', expected 'unchecked'" }
}
foreach ($label in 'captive portal (HTML body)', 'empty body') {
    $script:Answer = if ($label -like 'captive*') { { '<html>Sign in to the network</html>' } } else { { $null } }
    $r = Get-ReleaseAssetLiveness $realBat
    if ($r.state -eq 'passed') { $problems += "$label -> 'passed' on a body that names no asset" }
}
$script:Answer = { All-Present }
$r = Get-ReleaseAssetLiveness $realBat
if ($r.state -ne 'passed') { $problems += "dual: every asset present -> state '$($r.state)', expected 'passed'" }
Report 'FailOpen.UnreachableOrRateLimitedReleaseIsUncheckedNeverPassed' $problems

# =================================================================================================
# Secret leakage: the durable run record names the modified and untracked files and never carries
# their contents. Dual: the names are there, so the absence of the contents is not an empty record.
$problems = @()
$git = (Get-Command 'git' -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if ($null -eq $git) { $problems += 'no git on PATH; the record check needs one' }
else {
    $script:GitExe = $git
    $repo = Join-Path $scratch 'repo'
    New-Item -ItemType Directory -Path $repo | Out-Null
    $canaryTracked = 'sk-proj-TRACKEDCANARY' + ('0' * 32)
    $canaryUntracked = 'gsk_UNTRACKEDCANARY' + ('1' * 32)
    $null = & $git -C $repo init -q 2>&1
    [IO.File]::WriteAllText((Join-Path $repo 'DefaultGame.ini'), "[x]`r`nApiKey=`r`n")
    $null = & $git -C $repo add DefaultGame.ini 2>&1
    $null = & $git -C $repo -c user.name=sec -c user.email=sec@localhost -c commit.gpgsign=false commit -q -m init 2>&1
    [IO.File]::WriteAllText((Join-Path $repo 'DefaultGame.ini'), "[x]`r`nApiKey=$canaryTracked`r`n")
    [IO.File]::WriteAllText((Join-Path $repo 'secrets.env'), "GROQ_API_KEY=$canaryUntracked`r`n")
    $record = ConvertTo-Json (Get-TreeIdentity $repo) -Depth 6 -Compress
    if ($record -notmatch 'DefaultGame\.ini') { $problems += "dual: the record does not name the modified file: $record" }
    if ($record -notmatch 'secrets\.env') { $problems += "dual: the record does not name the untracked file: $record" }
    foreach ($canary in $canaryTracked, $canaryUntracked) {
        if ($record.Contains($canary)) { $problems += "the record carries file contents: '$canary'" }
    }
}
Report 'SecretLeakage.GateRecordNamesFilesNeverContents' $problems

# =================================================================================================
# Traversal / destructive retarget: a harness junction found pointing somewhere else is unlinked and
# re-pointed, never recursed into, so the directory it pointed at keeps its contents; a real
# directory found in its place is refused rather than emptied. Dual: a junction already on the plugin
# is left as it is, and the plugin is reachable through the re-pointed one.
$problems = @()
$block = $ast.EndBlock.Statements
$first = $block | Where-Object { $_.Extent.Text -match '^\$junctionTarget\s*=\s*\$null' } | Select-Object -First 1
$last = $block | Where-Object { $_ -is [Management.Automation.Language.IfStatementAst] -and $_.Extent.Text -match '^if\s*\(\s*\$junctionTarget\s+-ne\s+\$PluginDir\s*\)' } | Select-Object -First 1
if ($null -eq $first -or $null -eq $last) { $problems += 'the junction block was not found in run-tests.ps1' }
else {
    $retarget = [scriptblock]::Create($gateText.Substring($first.Extent.StartOffset, $last.Extent.EndOffset - $first.Extent.StartOffset))
    function New-Dir([string] $Path, [string] $File) {
        New-Item -ItemType Directory -Path $Path -Force | Out-Null
        if ($File) { [IO.File]::WriteAllText((Join-Path $Path $File), 'keep') }
    }
    $j = Join-Path $scratch 'junctions'
    $PluginDir = Join-Path $j 'RealPlugin'; New-Dir $PluginDir 'FANTASIA.uplugin'
    $victim = Join-Path $j 'Victim'; New-Dir $victim 'precious.txt'
    $junctionDir = Join-Path $j 'Harness\Plugins'; New-Dir $junctionDir $null
    $junction = Join-Path $junctionDir 'FANTASIA'

    # a. pointing elsewhere
    $null = & "$env:SystemRoot\System32\cmd.exe" /c mklink /J "$junction" "$victim"
    $LASTEXITCODE = 0
    try { & $retarget } catch { $problems += "retarget threw: $_" }
    if (-not (Test-Path -LiteralPath (Join-Path $victim 'precious.txt'))) { $problems += 'the directory the stale junction pointed at lost its contents' }
    $now = (Get-Item -LiteralPath $junction -Force).Target | Select-Object -First 1
    if ($now -ne $PluginDir) { $problems += "dual: the junction now points at '$now', expected '$PluginDir'" }
    if (-not (Test-Path -LiteralPath (Join-Path $junction 'FANTASIA.uplugin'))) { $problems += 'dual: the plugin is not reachable through the re-pointed junction' }

    # b. already right: untouched
    $before = (Get-Item -LiteralPath $junction -Force).CreationTimeUtc
    Start-Sleep -Milliseconds 20
    & $retarget
    if ((Get-Item -LiteralPath $junction -Force).CreationTimeUtc -ne $before) { $problems += 'dual: a junction already on the plugin was recreated' }

    # c. a real, non-empty directory where the junction belongs
    [IO.Directory]::Delete($junction)
    New-Dir $junction 'user-work.txt'
    $refused = $false
    try { & $retarget } catch { $refused = $true }
    if (-not $refused) { $problems += 'a real non-empty directory in the junction slot was not refused' }
    if (-not (Test-Path -LiteralPath (Join-Path $junction 'user-work.txt'))) { $problems += 'a real directory in the junction slot lost its contents' }
}
Report 'Traversal.HarnessJunctionRetargetUnlinksNeverRecurses' $problems

# =================================================================================================
# Destructive removal through a link: the prebuilt mode's Remove-Item -Recurse -Force of the
# harness's Source directory does not follow a junction, whether Source is one or contains one.
# This is the statement the gate runs; the check pins the PowerShell 5.1 behaviour it relies on.
$problems = @()
$removal = $ast.FindAll({ param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.Extent.Text -match '^Remove-Item\s+-LiteralPath\s+\$sourceDir\s+-Recurse\s+-Force' }, $true) | Select-Object -First 1
if ($null -eq $removal) { $problems += "run-tests.ps1 no longer removes `$sourceDir with 'Remove-Item -LiteralPath `$sourceDir -Recurse -Force'; re-read the statement and update this check" }
else {
    $remove = [scriptblock]::Create($removal.Extent.Text)
    $s = Join-Path $scratch 'source'
    $v1 = Join-Path $s 'victim1'; New-Item -ItemType Directory -Path $v1 -Force | Out-Null; [IO.File]::WriteAllText((Join-Path $v1 'keep.txt'), 'x')
    $sourceDir = Join-Path $s 'SourceIsJunction'
    $null = & "$env:SystemRoot\System32\cmd.exe" /c mklink /J "$sourceDir" "$v1"
    & $remove
    if (-not (Test-Path -LiteralPath (Join-Path $v1 'keep.txt'))) { $problems += 'a Source junction was followed and its target emptied' }
    if (Test-Path -LiteralPath $sourceDir) { $problems += 'dual: the Source junction itself was not removed' }
    $v2 = Join-Path $s 'victim2'; New-Item -ItemType Directory -Path $v2 -Force | Out-Null; [IO.File]::WriteAllText((Join-Path $v2 'keep.txt'), 'x')
    $sourceDir = Join-Path $s 'SourceHoldsJunction'
    New-Item -ItemType Directory -Path $sourceDir | Out-Null
    $null = & "$env:SystemRoot\System32\cmd.exe" /c mklink /J "$sourceDir\Inner" "$v2"
    & $remove
    if (-not (Test-Path -LiteralPath (Join-Path $v2 'keep.txt'))) { $problems += 'a junction inside Source was followed and its target emptied' }
    if (Test-Path -LiteralPath $sourceDir) { $problems += 'dual: the Source directory holding a junction was not removed' }
}
Report 'Traversal.PrebuiltSourceRemovalDoesNotFollowJunctions' $problems

# =================================================================================================
# Planting: git and dumpbin are not resolved from the current directory. Dual: the same planted
# binaries ARE found once PATH names their directory, which is the documented trust (PATH is the
# user's own environment) and shows the check can see a planting.
$problems = @()
$plant = Join-Path $scratch 'plant'
New-Item -ItemType Directory -Path $plant | Out-Null
Copy-Item "$env:SystemRoot\System32\hostname.exe" (Join-Path $plant 'git.exe')
Copy-Item "$env:SystemRoot\System32\hostname.exe" (Join-Path $plant 'dumpbin.exe')
$gitLine = $ast.FindAll({ param($n) $n -is [Management.Automation.Language.AssignmentStatementAst] -and $n.Extent.Text -match '^\$gitCommand\s*=' }, $true) | Select-Object -First 1
if ($null -eq $gitLine) { $problems += 'the git lookup was not found in run-tests.ps1' }
else {
    $resolveGit = [scriptblock]::Create($gitLine.Right.Extent.Text)
    Push-Location $plant
    try {
        $g = & $resolveGit; $d = Find-Dumpbin
        if ($null -ne $g -and $g.Source -like "$plant*") { $problems += "git resolved from the current directory: $($g.Source)" }
        if ($null -ne $d -and $d -like "$plant*") { $problems += "dumpbin resolved from the current directory: $d" }
    }
    finally { Pop-Location }
    $savedPath = $env:PATH
    try {
        $env:PATH = "$plant;$savedPath"
        $g = & $resolveGit; $d = Find-Dumpbin
        if ($null -eq $g -or $g.Source -notlike "$plant*") { $problems += "dual: git on PATH was not found there (got '$($g.Source)')" }
        if ($d -notlike "$plant*") { $problems += "dual: dumpbin on PATH was not found there (got '$d')" }
    }
    finally { $env:PATH = $savedPath }
}
Report 'Planting.GateToolsNotResolvedFromCurrentDirectory' $problems

# =================================================================================================
# Tampering with the delivered install entry point: bootstrap.bat as a ZIP or archive of this tree
# hands it out (server-side git archive, no core.autocrlf) has no bare LF, so cmd cannot resume
# mid-line and run fragments of its text; bootstrap.sh has no CR. Dual: the same blob under the
# attributes committed at HEAD is delivered with bare LFs, which is the form that ran 18 fragments.
$problems = @()
function Get-ArchivedFiles([string[]] $TreeAndPaths) {
    $tar = Join-Path $scratch ('a-' + [guid]::NewGuid().ToString('N').Substring(0, 6) + '.tar')
    # The archive goes to a file rather than through the pipeline, which would decode the bytes.
    $p = Start-Process -FilePath $git -ArgumentList ($common + @("--output=`"$tar`"") + $TreeAndPaths) -WorkingDirectory $repoRoot -NoNewWindow -Wait -PassThru
    if ($p.ExitCode -ne 0) { throw "git archive exited $($p.ExitCode)" }
    $bytes = [IO.File]::ReadAllBytes($tar); $files = @{}; $o = 0
    while ($o + 512 -le $bytes.Length) {
        $name = [Text.Encoding]::ASCII.GetString($bytes, $o, 100).Trim([char] 0)
        if ($name -eq '') { break }
        $size = [Convert]::ToInt64([Text.Encoding]::ASCII.GetString($bytes, $o + 124, 12).Trim([char] 0, ' '), 8)
        $type = [char] $bytes[$o + 156]
        if ($type -eq '0' -or $type -eq [char] 0) { $files[$name] = [byte[]] $bytes[($o + 512)..($o + 511 + $size)] }
        $o += 512 + [math]::Ceiling($size / 512) * 512
    }
    return $files
}
function Count-Bare([byte[]] $b) {
    $lf = 0; $cr = 0
    for ($i = 0; $i -lt $b.Length; $i++) {
        if ($b[$i] -eq 10 -and ($i -eq 0 -or $b[$i - 1] -ne 13)) { $lf++ }
        if ($b[$i] -eq 13) { $cr++ }
    }
    return @{ bareLf = $lf; cr = $cr }
}
if ($null -eq $git) { $problems += 'no git on PATH' }
else {
    $paths = @('FANTASIA/bootstrap.bat', 'FANTASIA/bootstrap.sh')
    $common = @('-c', 'core.autocrlf=false', '-c', 'core.eol=lf', 'archive', '--format=tar')
    $delivered = Get-ArchivedFiles (@('--worktree-attributes', 'HEAD', '--') + $paths)
    $bat = Count-Bare $delivered['FANTASIA/bootstrap.bat']; $sh = Count-Bare $delivered['FANTASIA/bootstrap.sh']
    if ($null -eq $delivered['FANTASIA/bootstrap.bat']) { $problems += 'bootstrap.bat missing from the archive' }
    elseif ($bat.bareLf -ne 0) { $problems += "bootstrap.bat is delivered with $($bat.bareLf) bare LF(s) under this tree's .gitattributes" }
    if ($null -eq $delivered['FANTASIA/bootstrap.sh']) { $problems += 'bootstrap.sh missing from the archive' }
    elseif ($sh.cr -ne 0) { $problems += "bootstrap.sh is delivered with $($sh.cr) CR(s) under this tree's .gitattributes" }
    $committed = Get-ArchivedFiles (@('HEAD', '--') + $paths)
    $old = Count-Bare $committed['FANTASIA/bootstrap.bat']
    $attrAtHead = & $git -C $repoRoot show 'HEAD:.gitattributes' 2>$null
    if (($attrAtHead -join "`n") -notmatch 'FANTASIA/\*\.bat') {
        if ($old.bareLf -eq 0) { $problems += 'dual: under the attributes committed at HEAD bootstrap.bat should be delivered LF, and has no bare LF, so this check cannot tell the two forms apart' }
    }
    Write-Host ("     bootstrap.bat delivered: {0} bare LF under this tree's attributes, {1} under HEAD's; bootstrap.sh: {2} CR" -f $bat.bareLf, $old.bareLf, $sh.cr)
}
Report 'Tampering.DeliveredBootstrapBatIsCrlfAndShIsLf' $problems

Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
exit $script:Failures
