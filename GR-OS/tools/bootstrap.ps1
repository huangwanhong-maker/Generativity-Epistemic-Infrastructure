[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Configuration = 'Release',
    [string]$BuildDirectory = 'build',
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$grWorkspace = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$grBuild = [System.IO.Path]::GetFullPath((Join-Path $grWorkspace $BuildDirectory))
if (-not $grBuild.StartsWith($grWorkspace + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'The build directory must be inside GR-OS.'
}
$grCMakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
if ($grCMakeCommand) {
    $grCMake = $grCMakeCommand.Source
} else {
    $grVsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $grVsWhere)) { throw 'CMake is not on PATH and Visual Studio discovery is unavailable.' }
    $grInstallation = & $grVsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $grInstallation) { throw 'An installed Visual Studio C++ toolchain was not found.' }
    $grCMake = Join-Path $grInstallation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
}
if (-not (Test-Path -LiteralPath $grCMake)) { throw "CMake was not found at $grCMake" }
$grCTest = Join-Path (Split-Path $grCMake) 'ctest.exe'
& $grCMake -S $grWorkspace -B $grBuild -G 'Visual Studio 17 2022' -A x64 -DGR_BUILD_TESTS=ON -DGR_BUILD_BINDINGS=ON -DGR_REQUIRE_PYTHON_TESTS=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& $grCMake --build $grBuild --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Native build failed.' }
if (-not $SkipTests) {
    & $grCTest --test-dir $grBuild -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
}
Write-Output "Native library: $(Join-Path $grBuild "$Configuration/gr_native.dll")"
Write-Output 'Python access: set GR_OS_LIBRARY to that DLL and PYTHONPATH to GR-OS/bindings/python/src, or install the SDK wheel.'
