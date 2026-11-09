param(
    [ValidateSet('build', 'test', 'serveur', 'client')]
    [string]$Action = 'build',
    [string]$Hote = 'localhost',
    [ValidateRange(1, 65535)]
    [int]$Port = 5000
)
$ErrorActionPreference = 'Stop'
$cygwinBin = Join-Path $PSScriptRoot '.outils/cygwin/bin'
if (!(Test-Path (Join-Path $cygwinBin 'gcc.exe')) -or
    !(Test-Path (Join-Path $cygwinBin 'make.exe'))) {
    throw 'Executez d abord : powershell -ExecutionPolicy Bypass -File .\installer-windows.ps1'
}
$ancienPath = $env:PATH
Push-Location $PSScriptRoot
try {
    $env:PATH = "$cygwinBin;$ancienPath"
    # Les chemins du Makefile sont relatifs : aucun chemin Windows dans les recettes.
    if ($Action -eq 'test') {
        & (Join-Path $cygwinBin 'make.exe') test
    } else {
        & (Join-Path $cygwinBin 'make.exe') all
    }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    if ($Action -eq 'serveur') { & './bin/awale_serveur.exe' $Port }
    if ($Action -eq 'client') { & './bin/awale_client.exe' $Hote $Port }
    exit $LASTEXITCODE
} finally {
    $env:PATH = $ancienPath
    Pop-Location
}
