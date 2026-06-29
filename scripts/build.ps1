[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$Clean
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildRoot = Join-Path $ProjectRoot 'build'
$VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'

if (-not (Test-Path -LiteralPath $VsWhere)) {
    throw 'Visual Studio Installer (vswhere.exe) was not found.'
}

$VsInstall = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath | Select-Object -First 1

if (-not $VsInstall) {
    throw 'A Visual Studio installation with the x64 C++ toolchain was not found.'
}

$CMake = Join-Path $VsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$CTest = Join-Path $VsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$Ninja = Join-Path $VsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$DevShell = Join-Path $VsInstall 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'

foreach ($RequiredTool in @($CMake, $CTest, $Ninja, $DevShell)) {
    if (-not (Test-Path -LiteralPath $RequiredTool)) {
        throw "Required Visual Studio build tool was not found: $RequiredTool"
    }
}

function Remove-BuildRoot {
    if (-not (Test-Path -LiteralPath $BuildRoot)) {
        return
    }
    $ResolvedProject = (Resolve-Path -LiteralPath $ProjectRoot).Path
    $ResolvedBuild = (Resolve-Path -LiteralPath $BuildRoot).Path
    if (-not $ResolvedBuild.StartsWith($ResolvedProject + [IO.Path]::DirectorySeparatorChar)) {
        throw "Refusing to clean build directory outside project root: $ResolvedBuild"
    }
    Remove-Item -LiteralPath $ResolvedBuild -Recurse -Force
}

if ($Clean) {
    Remove-BuildRoot
} else {
    $Cache = Join-Path $BuildRoot 'CMakeCache.txt'
    if ((Test-Path -LiteralPath $Cache) -and
        -not (Select-String -LiteralPath $Cache -Pattern '^CMAKE_GENERATOR:INTERNAL=Ninja Multi-Config$' -Quiet)) {
        Remove-BuildRoot
    }
}

# Some hosts expose both `Path` and `PATH`. Visual Studio's development-shell
# module treats those as duplicate dictionary keys even though Windows does not.
$ProcessEnvironment = [Environment]::GetEnvironmentVariables(
    [EnvironmentVariableTarget]::Process)
if ($ProcessEnvironment.ContainsKey('Path') -and $ProcessEnvironment.ContainsKey('PATH')) {
    $MergedPath = (
        ($ProcessEnvironment['Path'] -split ';') +
        ($ProcessEnvironment['PATH'] -split ';') |
        Where-Object { $_ } |
        Select-Object -Unique
    ) -join ';'
    [Environment]::SetEnvironmentVariable(
        'PATH', $null, [EnvironmentVariableTarget]::Process)
    [Environment]::SetEnvironmentVariable(
        'Path', $MergedPath, [EnvironmentVariableTarget]::Process)
}

Import-Module $DevShell
Enter-VsDevShell -VsInstallPath $VsInstall -SkipAutomaticLocation `
    -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null

& $CMake -S $ProjectRoot -B $BuildRoot -G 'Ninja Multi-Config' "-DCMAKE_MAKE_PROGRAM=$Ninja"
if ($LASTEXITCODE -ne 0) {
    throw "CMake configure failed with exit code $LASTEXITCODE."
}

& $CMake --build $BuildRoot --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

& $CTest --test-dir $BuildRoot -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "Tests failed with exit code $LASTEXITCODE."
}

$Output = Join-Path $BuildRoot "$Configuration\msimg32.dll"
if (-not (Test-Path -LiteralPath $Output)) {
    throw "Expected build output was not found: $Output"
}

Write-Host "Built and tested: $Output"
