[CmdletBinding()]
param(
    [string]$CloudMusicPath = '',

    [switch]$DryRun
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'common.ps1')

$CloudMusicDirectory = Resolve-CloudMusicDirectory -ExplicitPath $CloudMusicPath
$DestinationDll = Join-Path $CloudMusicDirectory 'msimg32.dll'
$ManifestPath = Join-Path $CloudMusicDirectory '.taskbar-lyrics.install.json'

if (-not (Test-Path -LiteralPath $ManifestPath -PathType Leaf)) {
    throw "Taskbar Lyrics installation manifest was not found: $ManifestPath"
}
if (-not (Test-Path -LiteralPath $DestinationDll -PathType Leaf)) {
    throw "The installed proxy DLL was not found: $DestinationDll"
}

$Manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
$InstalledHash = (Get-FileHash -LiteralPath $DestinationDll -Algorithm SHA256).Hash
if ($Manifest.product -ne 'TaskbarLyrics' -or $Manifest.installedHash -ne $InstalledHash) {
    throw 'The installed proxy DLL does not match the Taskbar Lyrics manifest; refusing to remove it.'
}

if (-not $DryRun) {
    Assert-CloudMusicStopped
}

Write-Host "NetEase Cloud Music: $CloudMusicDirectory"
Write-Host "Remove DLL:          $DestinationDll"
Write-Host "Remove manifest:     $ManifestPath"

if ($DryRun) {
    Write-Host 'Dry run completed; no files were removed.'
    exit 0
}

Remove-Item -LiteralPath $DestinationDll -Force
Remove-Item -LiteralPath $ManifestPath -Force

Write-Host 'Taskbar Lyrics uninstalled successfully. User settings and logs were preserved.'

