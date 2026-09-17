param(
    [string]$QtRoot = '',
    [string]$Configuration = 'Release'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $QtRoot) { $QtRoot = Join-Path $projectRoot '.tools\Qt\6.8.3\msvc2022_64' }
if (-not (Test-Path -LiteralPath (Join-Path $QtRoot 'lib\cmake\Qt6'))) { throw 'Qt6 SDK not found. Supply -QtRoot.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'Visual Studio C++ x64 tools not found.' }
$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$build = Join-Path $projectRoot 'build'
& $cmake -S $projectRoot -B $build -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$QtRoot"
if ($LASTEXITCODE) { throw 'CMake configure failed.' }
& $cmake --build $build --config $Configuration --parallel 8
if ($LASTEXITCODE) { throw 'Build failed.' }
$oldPath = $env:PATH
try {
    $env:PATH = (Join-Path $QtRoot 'bin') + ';' + $env:PATH
    & $ctest --test-dir $build -C $Configuration --output-on-failure
    $testExit = $LASTEXITCODE
    Get-Content -LiteralPath (Join-Path $build 'core-tests.txt'),(Join-Path $build 'ui-tests.txt')
    if ($testExit) { throw 'Tests failed.' }
} finally { $env:PATH = $oldPath }
