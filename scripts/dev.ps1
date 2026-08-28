[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('configure', 'build', 'test', 'check', 'format', 'format-check', 'tidy', 'release')]
    [string] $Task = 'check',

    [ValidateRange(0, 256)]
    [int] $Jobs = 0,

    [string] $CTestRegex = '',

    [switch] $Fresh
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$windowsConfigurePreset = 'windows-msvc'
$debugBuildPreset = 'windows-debug'
$releaseBuildPreset = 'windows-release'
$tidyConfigurePreset = 'windows-tidy'
$tidyBuildPreset = 'windows-tidy'
$visualStudioInstallation = $null

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
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        if ($env:VSINSTALLDIR) {
            $script:visualStudioInstallation = $env:VSINSTALLDIR.TrimEnd('\')
        }
    }
    else {
        $script:visualStudioInstallation = Find-VisualStudioInstallation
        $devShellModule = Join-Path $script:visualStudioInstallation `
            'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
        if (-not (Test-Path -LiteralPath $devShellModule -PathType Leaf)) {
            throw "Visual Studio developer-shell module not found: $devShellModule"
        }

        Import-Module $devShellModule
        Enter-VsDevShell -VsInstallPath $script:visualStudioInstallation `
            -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
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
        $llvmBin = Join-Path $script:visualStudioInstallation 'VC\Tools\Llvm\x64\bin'
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

    $arguments = @('--test-dir', 'build/windows-msvc', '-C', $Configuration, '--output-on-failure')
    if ($Jobs -gt 0) {
        $arguments += @('--parallel', $Jobs.ToString())
    }
    if ($CTestRegex) {
        $arguments += @('--tests-regex', $CTestRegex)
    }
    Invoke-NativeCommand ctest $arguments
}

Push-Location $repositoryRoot
try {
    Initialize-WindowsToolchain

    switch ($Task) {
        'configure' {
            Invoke-Configure $windowsConfigurePreset
        }
        'build' {
            Invoke-Configure $windowsConfigurePreset
            Invoke-BuildPreset $debugBuildPreset
        }
        'test' {
            Invoke-Configure $windowsConfigurePreset
            Invoke-BuildPreset $debugBuildPreset
            Invoke-Tests 'Debug'
        }
        'check' {
            Invoke-Configure $windowsConfigurePreset
            Invoke-BuildPreset $debugBuildPreset
            Invoke-BuildTarget 'build/windows-msvc' 'format-check' 'Debug'
            Invoke-Tests 'Debug'
        }
        'format' {
            Invoke-Configure $windowsConfigurePreset
            Invoke-BuildTarget 'build/windows-msvc' 'format' 'Debug'
        }
        'format-check' {
            Invoke-Configure $windowsConfigurePreset
            Invoke-BuildTarget 'build/windows-msvc' 'format-check' 'Debug'
        }
        'tidy' {
            Invoke-Configure $tidyConfigurePreset
            Invoke-BuildPreset $tidyBuildPreset
            Invoke-BuildTarget 'build/windows-tidy' 'tidy'
        }
        'release' {
            Invoke-Configure $windowsConfigurePreset
            Invoke-BuildPreset $releaseBuildPreset
            Invoke-Tests 'Release'
        }
    }
}
finally {
    Pop-Location
}
