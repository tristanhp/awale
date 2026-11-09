$ErrorActionPreference = 'Stop'
$racine = $PSScriptRoot
$outils = Join-Path $racine '.outils'
$cygwin = Join-Path $outils 'cygwin'
$setup = Join-Path $outils 'setup-x86_64.exe'
New-Item -ItemType Directory -Force $outils | Out-Null

if (!(Test-Path (Join-Path $cygwin 'bin/gcc.exe')) -or
    !(Test-Path (Join-Path $cygwin 'bin/make.exe'))) {
    Write-Host 'Telechargement de Cygwin (sans WSL, sans droits administrateur)...'
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest 'https://cygwin.com/setup-x86_64.exe' -OutFile $setup
    $signature = Get-AuthenticodeSignature -LiteralPath $setup
    if ($signature.Status -ne 'Valid') {
        throw 'La signature du programme Cygwin ne peut pas etre validee.'
    }
    $parametres = @('--quiet-mode', '--no-admin', '--no-desktop', '--no-startmenu',
        '--root', ('"' + $cygwin + '"'),
        '--local-package-dir', ('"' + (Join-Path $outils 'paquets') + '"'),
        '--site', 'https://mirrors.kernel.org/sourceware/cygwin/',
        '--packages', 'gcc-core,make,bash')
    $processus = Start-Process -FilePath $setup -ArgumentList $parametres -Wait -PassThru -WindowStyle Hidden
    if ($processus.ExitCode -ne 0) {
        throw "Installation Cygwin echouee (code $($processus.ExitCode))."
    }
}

& (Join-Path $racine 'windows.ps1') test
if ($LASTEXITCODE -ne 0) { throw 'Compilation ou tests en echec.' }
Write-Host 'Installation terminee. Lancez .\serveur.cmd puis .\client.cmd dans deux autres terminaux.'
