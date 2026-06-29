[CmdletBinding()]
param(
    [string]$CloudMusicPath = '',

    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'common.ps1')

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$SourceDll = Join-Path $ProjectRoot "build\$Configuration\msimg32.dll"
$CloudMusicDirectory = Resolve-CloudMusicDirectory -ExplicitPath $CloudMusicPath
$DestinationDll = Join-Path $CloudMusicDirectory 'msimg32.dll'
$ManifestPath = Join-Path $CloudMusicDirectory '.taskbar-lyrics.install.json'

if (-not (Test-Path -LiteralPath $SourceDll -PathType Leaf)) {
    throw "The built proxy DLL was not found. Run scripts\build.ps1 first: $SourceDll"
}

Assert-X64CloudMusic -CloudMusicDirectory $CloudMusicDirectory

$ClientVersion = (Get-Item -LiteralPath (Join-Path $CloudMusicDirectory 'cloudmusic.exe')).VersionInfo.ProductVersion

if (-not $DryRun) {
    Assert-CloudMusicStopped
}

$SourceHash = (Get-FileHash -LiteralPath $SourceDll -Algorithm SHA256).Hash
$ExistingIsOwned = $false

if (Test-Path -LiteralPath $DestinationDll -PathType Leaf) {
    if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
        throw "Refusing to overwrite an unknown proxy DLL: $DestinationDll"
    }

    $Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    $ExistingHash = (Get-FileHash -LiteralPath $DestinationDll -Algorithm SHA256).Hash
    $ExistingIsOwned =
        $Manifest.product -eq 'TaskbarLyrics' -and
        $Manifest.installedHash -eq $ExistingHash

    if (-not $ExistingIsOwned) {
        throw "Refusing to overwrite a proxy DLL that does not match the Taskbar Lyrics manifest: $DestinationDll"
    }
}

Write-Host "NetEase Cloud Music: $CloudMusicDirectory"
Write-Host "Client version:      $ClientVersion"
Write-Host "Source DLL:          $SourceDll"
Write-Host "Destination DLL:     $DestinationDll"
Write-Host "SHA-256:             $SourceHash"

if ($DryRun) {
    Write-Host 'Dry run completed; no files were copied.'
    exit 0
}

$TemporaryDll = "$DestinationDll.taskbar-lyrics-new"
Copy-Item -LiteralPath $SourceDll -Destination $TemporaryDll -Force

if (Test-Path -LiteralPath $DestinationDll) {
    Remove-Item -LiteralPath $DestinationDll -Force
}
Move-Item -LiteralPath $TemporaryDll -Destination $DestinationDll

$InstalledHash = (Get-FileHash -LiteralPath $DestinationDll -Algorithm SHA256).Hash
$InstallManifest = [ordered]@{
    product = 'TaskbarLyrics'
    version = '1.0.1'
    clientVersion = $ClientVersion
    installedAtUtc = [DateTime]::UtcNow.ToString('o')
    installedFile = 'msimg32.dll'
    installedHash = $InstalledHash
}
$InstallManifest | ConvertTo-Json | Set-Content -LiteralPath $ManifestPath -Encoding UTF8

Write-Host 'Taskbar Lyrics installed successfully.'

