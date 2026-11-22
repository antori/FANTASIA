<#
.SYNOPSIS
    Builds a scratch git repository holding this working tree's install surface and committed-style
    binaries in ONE commit, so that `run-tests.ps1 -Mode prebuilt` can be run from it on a tree it
    accepts as clean, without committing anything in the real repository.

.DESCRIPTION
    The prebuilt mode refuses (exit 19) while source under FANTASIA/Source or FANTASIAACE/Source is
    modified or untracked, which is the state of a working tree before its binaries refresh is
    committed. This copies .gitattributes, FANTASIA and FANTASIAACE (not ThirdParty, Intermediate or
    Saved) into %TEMP%\fantasia-prebuilt-clone, makes one commit there, and prints the command to run.
    Nothing is written inside the repository. The copy holds no junction, so it can be deleted with
    Remove-Item -Recurse.

    The verdict of a prebuilt run from the clone is about the clone's binaries. Note F-080: the Install
    tests inside those binaries still read the tree the DLL was COMPILED through, not the clone.
#>
$ErrorActionPreference = 'Stop'
$real = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..\..')).Path
$c = Join-Path $env:TEMP 'fantasia-prebuilt-clone'
if (Test-Path $c) { throw "$c exists; delete it first (it holds no junction)" }
New-Item -ItemType Directory $c | Out-Null
Copy-Item (Join-Path $real '.gitattributes') $c
foreach ($p in 'FANTASIA', 'FANTASIAACE') {
    New-Item -ItemType Directory (Join-Path $c $p) | Out-Null
    robocopy (Join-Path $real $p) (Join-Path $c $p) /E /XD ThirdParty Intermediate Saved /NFL /NDL /NJH /NJS /NP | Out-Null
}
Push-Location $c
git init -q
git config core.autocrlf false
git add -A | Out-Null
git -c user.name=tester -c user.email=tester@example.invalid commit -q -m 'scratch clone for a prebuilt gate run'
Pop-Location
"Run: powershell -File `"$c\FANTASIA\run-tests.ps1`" -Mode prebuilt"
