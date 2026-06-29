[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Version,

    [switch]$Publish,

    [switch]$SkipBuild,

    [string]$Remote = 'origin'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$CMakePath = Join-Path $ProjectRoot 'CMakeLists.txt'
$InstallerPath = Join-Path $ProjectRoot 'scripts\install.ps1'
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

function Invoke-Git {
    param(
        [Parameter(Mandatory)]
        [string[]]$GitArguments
    )

    & git @GitArguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($GitArguments -join ' ') failed with exit code $LASTEXITCODE."
    }
}

function Get-GitOutput {
    param(
        [Parameter(Mandatory)]
        [string[]]$GitArguments
    )

    $Output = & git @GitArguments
    if ($LASTEXITCODE -ne 0) {
        throw "git $($GitArguments -join ' ') failed with exit code $LASTEXITCODE."
    }
    return ($Output | Out-String).Trim()
}

function Set-ProjectVersion {
    param(
        [Parameter(Mandatory)]
        [string]$BaseVersion
    )

    $CMakeContent = [IO.File]::ReadAllText($CMakePath)
    $CMakePattern = '(project\(TaskbarLyrics VERSION )\d+\.\d+\.\d+( LANGUAGES CXX\))'
    if ([regex]::Matches($CMakeContent, $CMakePattern).Count -ne 1) {
        throw 'Expected exactly one TaskbarLyrics project version in CMakeLists.txt.'
    }
    $UpdatedCMake = [regex]::Replace(
        $CMakeContent,
        $CMakePattern,
        {
            param($Match)
            return $Match.Groups[1].Value + $BaseVersion + $Match.Groups[2].Value
        }
    )

    $InstallerContent = [IO.File]::ReadAllText($InstallerPath)
    $InstallerPattern = "(?m)(^\s*version\s*=\s*')\d+\.\d+\.\d+(')"
    if ([regex]::Matches($InstallerContent, $InstallerPattern).Count -ne 1) {
        throw 'Expected exactly one install manifest version in scripts/install.ps1.'
    }
    $UpdatedInstaller = [regex]::Replace(
        $InstallerContent,
        $InstallerPattern,
        {
            param($Match)
            return $Match.Groups[1].Value + $BaseVersion + $Match.Groups[2].Value
        }
    )

    [IO.File]::WriteAllText($CMakePath, $UpdatedCMake, $Utf8NoBom)
    [IO.File]::WriteAllText($InstallerPath, $UpdatedInstaller, $Utf8NoBom)
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git was not found in PATH.'
}

$Version = $Version.Trim()
if ($Version.StartsWith('v', [StringComparison]::OrdinalIgnoreCase)) {
    $Version = $Version.Substring(1)
}
if ($Version -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?$') {
    throw "Version must use the format 1.2.3 or 1.2.3-preview.1: $Version"
}

$BaseVersion = $Version.Split('-')[0]
$Tag = "v$Version"

Push-Location $ProjectRoot
try {
    [void](Get-GitOutput -GitArguments @('rev-parse', '--is-inside-work-tree'))

    $WorkingTreeStatus = Get-GitOutput -GitArguments @(
        'status',
        '--porcelain',
        '--untracked-files=normal'
    )
    if ($WorkingTreeStatus) {
        throw @"
The working tree must be clean before preparing a release.
Commit or stash the following changes first:
$WorkingTreeStatus
"@
    }

    $Branch = Get-GitOutput -GitArguments @('branch', '--show-current')
    if (-not $Branch) {
        throw 'Release preparation is not allowed from a detached HEAD.'
    }

    if ($Publish) {
        if (Get-GitOutput -GitArguments @('tag', '--list', $Tag)) {
            throw "Local tag already exists: $Tag"
        }

        & git ls-remote --exit-code --tags $Remote "refs/tags/$Tag" | Out-Null
        $RemoteTagExitCode = $LASTEXITCODE
        if ($RemoteTagExitCode -eq 0) {
            throw "Remote tag already exists: $Tag"
        }
        if ($RemoteTagExitCode -ne 2) {
            throw "Could not check tag $Tag on remote $Remote."
        }
    }

    Set-ProjectVersion -BaseVersion $BaseVersion
    Write-Host "Project version updated to $BaseVersion."

    if (-not $SkipBuild) {
        & (Join-Path $PSScriptRoot 'build.ps1') -Configuration Release -Clean
        if ($LASTEXITCODE -ne 0) {
            throw "Release build failed with exit code $LASTEXITCODE."
        }
    } else {
        Write-Warning 'Build and tests were skipped.'
    }

    if (-not $Publish) {
        Write-Host ''
        Write-Host 'Release preparation completed locally; nothing was committed or pushed.'
        Write-Host 'Review the changes, commit them, then create and push the tag:'
        Write-Host "  git add CMakeLists.txt scripts/install.ps1"
        Write-Host "  git commit -m `"chore: release $Tag`""
        Write-Host "  git tag -a $Tag -m `"Release $Tag`""
        Write-Host "  git push $Remote $Branch"
        Write-Host "  git push $Remote $Tag"
        exit 0
    }

    Invoke-Git -GitArguments @('add', '--', 'CMakeLists.txt', 'scripts/install.ps1')

    & git diff --cached --quiet
    if ($LASTEXITCODE -eq 1) {
        Invoke-Git -GitArguments @('commit', '-m', "chore: release $Tag")
    } elseif ($LASTEXITCODE -ne 0) {
        throw "Could not inspect staged release changes (exit code $LASTEXITCODE)."
    } else {
        Write-Host 'Version files already match; no version commit was needed.'
    }

    Invoke-Git -GitArguments @('push', $Remote, $Branch)
    Invoke-Git -GitArguments @('tag', '-a', $Tag, '-m', "Release $Tag")
    Invoke-Git -GitArguments @('push', $Remote, $Tag)

    Write-Host ''
    Write-Host "Published $Tag. GitHub Actions will build and create the release."
} finally {
    Pop-Location
}
