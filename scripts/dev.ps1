[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('configure', 'build', 'test', 'check', 'format', 'format-check', 'tidy', 'release', 'package')]
    [string] $Task = 'check',

    [ValidateRange(0, 256)]
    [int] $Jobs = 0,

    [ValidateSet('native', 'x64', 'arm64')]
    [string] $Architecture = 'native',

    [string] $CTestRegex = '',

    [string] $OutputDirectory = 'out/packages',

    [switch] $Fresh
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$processArchitecture = [System.Runtime.InteropServices.RuntimeInformation]::ProcessArchitecture.ToString().ToLowerInvariant()
if ($Architecture -eq 'native') {
    $Architecture = if ($processArchitecture -eq 'arm64') { 'arm64' } else { 'x64' }
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$windowsConfigurePreset = if ($Architecture -eq 'arm64') { 'windows-arm64' } else { 'windows-msvc' }
$debugBuildPreset = if ($Architecture -eq 'arm64') { 'windows-arm64-debug' } else { 'windows-debug' }
$windowsBuildDirectory = if ($Architecture -eq 'arm64') { 'build/windows-arm64' } else { 'build/windows-msvc' }
$releaseBuildPreset = if ($Architecture -eq 'arm64') { 'windows-arm64-release' } else { 'windows-release' }
$tidyConfigurePreset = if ($Architecture -eq 'arm64') { 'windows-arm64-tidy' } else { 'windows-tidy' }
$tidyBuildPreset = if ($Architecture -eq 'arm64') { 'windows-arm64-tidy' } else { 'windows-tidy' }
$tidyBuildDirectory = if ($Architecture -eq 'arm64') { 'build/windows-arm64-tidy' } else { 'build/windows-tidy' }
$visualStudioInstallation = $null
$windowsHostArchitecture = if ($Architecture -eq 'arm64' -and $processArchitecture -eq 'arm64') { 'arm64' } else { 'x64' }

function Invoke-NativeCommand {
    param(
        [Parameter(Mandatory)]
        [string] $Executable,

        [Parameter(Mandatory)]
        [string[]] $Arguments
    )

    Write-Host "> $Executable $($Arguments -join ' ')" -ForegroundColor Cyan
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Executable $($Arguments -join ' ')"
    }
}

function Find-VisualStudioInstallation {
    $vswhereCandidates = @(@(
            (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'),
            (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\Installer\vswhere.exe')
        ) | Where-Object { $_ -and (Test-Path -LiteralPath $_ -PathType Leaf) })

    if ($vswhereCandidates.Count -eq 0) {
        throw 'Visual Studio was not initialized and vswhere.exe could not be found.'
    }

    $installation = & $vswhereCandidates[0] -latest -products '*' `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installation)) {
        throw 'A Visual Studio installation with the MSVC x64 tools could not be found.'
    }

    return $installation.Trim()
}

function Initialize-WindowsToolchain {
    $hasRequestedToolchain =
        (Get-Command cl.exe -ErrorAction SilentlyContinue) -and
        $env:VSCMD_ARG_TGT_ARCH -eq $Architecture -and
        $env:VSCMD_ARG_HOST_ARCH -eq $windowsHostArchitecture

    if ($env:VSINSTALLDIR) {
        $script:visualStudioInstallation = $env:VSINSTALLDIR.TrimEnd('\')
    }

    if (-not $hasRequestedToolchain) {
        if (-not $script:visualStudioInstallation) {
            $script:visualStudioInstallation = Find-VisualStudioInstallation
        }
        $devShellModule = Join-Path $script:visualStudioInstallation `
            'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
        if (-not (Test-Path -LiteralPath $devShellModule -PathType Leaf)) {
            throw "Visual Studio developer-shell module not found: $devShellModule"
        }

        Import-Module $devShellModule
        Enter-VsDevShell -VsInstallPath $script:visualStudioInstallation `
            -SkipAutomaticLocation -DevCmdArguments "-arch=$Architecture -host_arch=$windowsHostArchitecture"
    }

    if (-not $script:visualStudioInstallation -and $env:VSINSTALLDIR) {
        $script:visualStudioInstallation = $env:VSINSTALLDIR.TrimEnd('\')
    }

    if (-not $env:VCPKG_ROOT) {
        if (-not $script:visualStudioInstallation) {
            $script:visualStudioInstallation = Find-VisualStudioInstallation
        }
        $bundledVcpkg = Join-Path $script:visualStudioInstallation 'VC\vcpkg'
        if (-not (Test-Path -LiteralPath $bundledVcpkg -PathType Container)) {
            throw 'VCPKG_ROOT is not set and Visual Studio bundled vcpkg was not found.'
        }
        $env:VCPKG_ROOT = $bundledVcpkg
    }

    if ($script:visualStudioInstallation) {
        $llvmBin = Join-Path $script:visualStudioInstallation "VC\Tools\Llvm\$windowsHostArchitecture\bin"
        if ((Test-Path -LiteralPath $llvmBin -PathType Container) -and
            -not (($env:Path -split ';') -contains $llvmBin)) {
            $env:Path = "$llvmBin;$env:Path"
        }
    }
}

function Invoke-Configure {
    param([string] $Preset)

    $arguments = @('--preset', $Preset)
    if ($Fresh) {
        $arguments += '--fresh'
    }
    Invoke-NativeCommand cmake $arguments
}

function Ensure-Configure {
    param(
        [string] $Preset,
        [string] $CachePath
    )

    if ($Fresh -or -not (Test-Path -LiteralPath $CachePath -PathType Leaf)) {
        Invoke-Configure $Preset
    }
}

function Invoke-BuildPreset {
    param([string] $Preset)

    $arguments = @('--build', '--preset', $Preset)
    if ($Jobs -gt 0) {
        $arguments += @('--parallel', $Jobs.ToString())
    }
    Invoke-NativeCommand cmake $arguments
}

function Invoke-BuildTarget {
    param(
        [string] $BuildDirectory,
        [string] $Target,
        [string] $Configuration = ''
    )

    $arguments = @('--build', $BuildDirectory, '--target', $Target)
    if ($Configuration) {
        $arguments += @('--config', $Configuration)
    }
    if ($Jobs -gt 0) {
        $arguments += @('--parallel', $Jobs.ToString())
    }
    Invoke-NativeCommand cmake $arguments
}

function Invoke-Tests {
    param([string] $Configuration)

    $arguments = @('--test-dir', $windowsBuildDirectory, '-C', $Configuration, '--output-on-failure')
    if ($Jobs -gt 0) {
        $arguments += @('--parallel', $Jobs.ToString())
    }
    if ($CTestRegex) {
        $arguments += @('--tests-regex', $CTestRegex)
    }
    Invoke-NativeCommand ctest $arguments
}

function Assert-CleanReleaseTag {
    $status = & git status --porcelain
    if ($LASTEXITCODE -ne 0) {
        throw 'Could not inspect Git status for release packaging.'
    }
    if (-not [string]::IsNullOrWhiteSpace($status)) {
        throw 'Release packaging requires a clean Git worktree.'
    }

    $tag = & git describe --tags --exact-match --match 'v[0-9]*.[0-9]*.[0-9]*'
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($tag)) {
        throw 'Release packaging requires an exact vMAJOR.MINOR.PATCH Git tag.'
    }
}

function Invoke-Package {
    Assert-CleanReleaseTag
    Invoke-Configure $windowsConfigurePreset
    Invoke-BuildPreset $releaseBuildPreset
    Invoke-Tests 'Release'

    $packageDirectory = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot $OutputDirectory))
    New-Item -ItemType Directory -Force -Path $packageDirectory | Out-Null
    Invoke-NativeCommand cpack @(
        '--config', (Join-Path $repositoryRoot "$windowsBuildDirectory/CPackConfig.cmake"),
        '-C', 'Release',
        '-G', 'ZIP',
        '-B', $packageDirectory
    )
    Get-ChildItem -LiteralPath $packageDirectory -File |
        Where-Object { $_.Name -like 'vulpes-*.zip' -or $_.Name -like 'vulpes-*.zip.sha256' } |
        Sort-Object Name |
        ForEach-Object { Write-Host "Created $($_.FullName)" -ForegroundColor Green }
}

Push-Location $repositoryRoot
try {
    Initialize-WindowsToolchain

    switch ($Task) {
        'configure' {
            Invoke-Configure $windowsConfigurePreset
        }
        'build' {
            Ensure-Configure $windowsConfigurePreset "$windowsBuildDirectory/CMakeCache.txt"
            Invoke-BuildPreset $debugBuildPreset
        }
        'test' {
            Ensure-Configure $windowsConfigurePreset "$windowsBuildDirectory/CMakeCache.txt"
            Invoke-BuildPreset $debugBuildPreset
            Invoke-Tests 'Debug'
        }
        'check' {
            Ensure-Configure $windowsConfigurePreset "$windowsBuildDirectory/CMakeCache.txt"
            Invoke-BuildPreset $debugBuildPreset
            Invoke-BuildTarget $windowsBuildDirectory 'format-check' 'Debug'
            Invoke-Tests 'Debug'
        }
        'format' {
            Ensure-Configure $windowsConfigurePreset "$windowsBuildDirectory/CMakeCache.txt"
            Invoke-BuildTarget $windowsBuildDirectory 'format' 'Debug'
        }
        'format-check' {
            Ensure-Configure $windowsConfigurePreset "$windowsBuildDirectory/CMakeCache.txt"
            Invoke-BuildTarget $windowsBuildDirectory 'format-check' 'Debug'
        }
        'tidy' {
            Ensure-Configure $tidyConfigurePreset "$tidyBuildDirectory/CMakeCache.txt"
            Invoke-BuildPreset $tidyBuildPreset
            Invoke-BuildTarget $tidyBuildDirectory 'tidy'
        }
        'release' {
            Ensure-Configure $windowsConfigurePreset "$windowsBuildDirectory/CMakeCache.txt"
            Invoke-BuildPreset $releaseBuildPreset
            Invoke-Tests 'Release'
        }
        'package' {
            Invoke-Package
        }
    }
}
finally {
    Pop-Location
}
