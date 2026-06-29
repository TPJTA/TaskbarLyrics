Set-StrictMode -Version Latest

function ConvertTo-ExecutableDirectory {
    param(
        [AllowEmptyString()]
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $null
    }

    $Candidate = $Value.Trim()
    if ($Candidate.StartsWith('"')) {
        $ClosingQuote = $Candidate.IndexOf('"', 1)
        if ($ClosingQuote -gt 1) {
            $Candidate = $Candidate.Substring(1, $ClosingQuote - 1)
        }
    } else {
        $Comma = $Candidate.IndexOf(',')
        if ($Comma -gt 0) {
            $Candidate = $Candidate.Substring(0, $Comma)
        }
        $SpaceExe = $Candidate.IndexOf('.exe ', [StringComparison]::OrdinalIgnoreCase)
        if ($SpaceExe -ge 0) {
            $Candidate = $Candidate.Substring(0, $SpaceExe + 4)
        }
        $Candidate = $Candidate.Trim('" ')
    }

    if ([IO.Path]::GetExtension($Candidate) -ieq '.exe') {
        return Split-Path -Parent $Candidate
    }
    return $Candidate
}

function Test-CloudMusicDirectory {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Container)) {
        return $false
    }

    foreach ($RelativePath in @(
        'cloudmusic.exe',
        'cloudmusic.dll',
        'package\orpheus.ntpk'
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $Path $RelativePath) -PathType Leaf)) {
            return $false
        }
    }
    return $true
}

function Get-CloudMusicDirectoryFromRegistry {
    $RegistryRoots = @(
        'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall',
        'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall',
        'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall'
    )

    foreach ($Root in $RegistryRoots) {
        if (-not (Test-Path -LiteralPath $Root)) {
            continue
        }

        foreach ($Key in Get-ChildItem -LiteralPath $Root -ErrorAction SilentlyContinue) {
            $Entry = Get-ItemProperty -LiteralPath $Key.PSPath -ErrorAction SilentlyContinue
            if ($null -eq $Entry) {
                continue
            }

            $DisplayNameProperty = $Entry.PSObject.Properties['DisplayName']
            if ($null -eq $DisplayNameProperty -or
                [string]::IsNullOrWhiteSpace([string]$DisplayNameProperty.Value) -or
                [string]$DisplayNameProperty.Value -notmatch '网易云音乐|NetEase Cloud Music') {
                continue
            }

            foreach ($PropertyName in @('InstallLocation', 'DisplayIcon', 'UninstallString')) {
                $Property = $Entry.PSObject.Properties[$PropertyName]
                $Value = if ($null -eq $Property) { '' } else { [string]$Property.Value }
                $Candidate = ConvertTo-ExecutableDirectory -Value $Value
                if ($Candidate -and (Test-CloudMusicDirectory -Path $Candidate)) {
                    return (Resolve-Path -LiteralPath $Candidate).Path
                }
            }
        }
    }
    return $null
}

function Resolve-CloudMusicDirectory {
    param(
        [AllowEmptyString()]
        [string]$ExplicitPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (-not (Test-CloudMusicDirectory -Path $ExplicitPath)) {
            throw "The supplied directory is not a valid NetEase Cloud Music installation: $ExplicitPath"
        }
        return (Resolve-Path -LiteralPath $ExplicitPath).Path
    }

    $RegistryPath = Get-CloudMusicDirectoryFromRegistry
    if ($RegistryPath) {
        return $RegistryPath
    }

    $EnteredPath = Read-Host 'NetEase Cloud Music was not found in the registry. Enter its installation directory'
    if (-not (Test-CloudMusicDirectory -Path $EnteredPath)) {
        throw "The entered directory is not a valid NetEase Cloud Music installation: $EnteredPath"
    }
    return (Resolve-Path -LiteralPath $EnteredPath).Path
}

function Assert-CloudMusicStopped {
    $Running = Get-Process -Name cloudmusic -ErrorAction SilentlyContinue
    if ($Running) {
        throw 'NetEase Cloud Music is running. Exit it completely before installing or uninstalling Taskbar Lyrics.'
    }
}

function Get-PeMachine {
    param(
        [Parameter(Mandatory)]
        [string]$Path
    )

    $Stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try {
        $Reader = [IO.BinaryReader]::new($Stream)
        if ($Reader.ReadUInt16() -ne 0x5A4D) {
            throw "Not a PE file: $Path"
        }
        $Stream.Position = 0x3C
        $PeOffset = $Reader.ReadInt32()
        $Stream.Position = $PeOffset
        if ($Reader.ReadUInt32() -ne 0x00004550) {
            throw "Invalid PE signature: $Path"
        }
        return $Reader.ReadUInt16()
    } finally {
        $Stream.Dispose()
    }
}

function Assert-X64CloudMusic {
    param(
        [Parameter(Mandatory)]
        [string]$CloudMusicDirectory
    )

    $Executable = Join-Path $CloudMusicDirectory 'cloudmusic.exe'
    if ((Get-PeMachine -Path $Executable) -ne 0x8664) {
        throw 'Taskbar Lyrics supports only the x64 NetEase Cloud Music client.'
    }
}
