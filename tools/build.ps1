<#
.SYNOPSIS
定位独立Qt SDK与VS2022工具链，同步根目录工程浏览列表，构建、部署并验证GPUView。
.PARAMETER QtRoot
Qt安装根目录；为空使用仓库本地.tools路径，相对路径按仓库根解析，不写入公开工程。
.PARAMETER Configuration
Debug用于IDE调试，Release用于性能测量；默认Release。
.PARAMETER Action
Build增量构建，Rebuild先清理，Clean仅清理构建目标。
.PARAMETER SkipTests
仅跳过本轮测试，不代表已有测试结果仍有效；正常开发默认执行测试。
.NOTES
失败立即停止；只部署到本机构建目录，不上传SDK和运行时DLL。
#>
param(
    [string]$QtRoot = '',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('Build', 'Rebuild', 'Clean')]
    [string]$Action = 'Build',
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (-not $QtRoot) { $QtRoot = Join-Path $projectRoot '.tools\Qt\6.8.3\msvc2022_64' }
if (-not [IO.Path]::IsPathRooted($QtRoot)) { $QtRoot = Join-Path $projectRoot $QtRoot }
if (-not (Test-Path -LiteralPath (Join-Path $QtRoot 'lib\cmake\Qt6'))) { throw 'Qt6 SDK not found. Supply -QtRoot.' }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'Visual Studio C++ x64 tools not found.' }
$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ctest = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'
$build = Join-Path $projectRoot 'build'
& (Join-Path $PSScriptRoot 'sync-vs-sources.ps1')
& $cmake -S $projectRoot -B $build -G 'Visual Studio 17 2022' -A x64 "-DCMAKE_PREFIX_PATH=$QtRoot"
if ($LASTEXITCODE) { throw 'CMake configure failed.' }
if ($Action -eq 'Clean') {
    & $cmake --build $build --config $Configuration --target clean
    if ($LASTEXITCODE) { throw 'Clean failed.' }
    return
}
$buildOptions = @('--build', $build, '--config', $Configuration, '--parallel', '8')
if ($Action -eq 'Rebuild') { $buildOptions += '--clean-first' }
& $cmake @buildOptions
if ($LASTEXITCODE) { throw 'Build failed.' }
# 部署到本地构建输出，使两个IDE都无需修改系统PATH即可调试；不上传这些DLL。
$deploy = Join-Path $QtRoot 'bin\windeployqt.exe'
& $deploy "--$($Configuration.ToLowerInvariant())" --no-translations --no-compiler-runtime (Join-Path $build "$Configuration\GPUView.exe")
if ($LASTEXITCODE) { throw 'Qt runtime deployment failed.' }
if ($SkipTests) { return }
$oldPath = $env:PATH
try {
    $env:PATH = (Join-Path $QtRoot 'bin') + ';' + $env:PATH
    $testLogs = @((Join-Path $build "core-tests-$Configuration.txt"), (Join-Path $build "ui-tests-$Configuration.txt"))
    foreach ($testLog in $testLogs) { [IO.File]::WriteAllText($testLog, '') }
    & $ctest --test-dir $build -C $Configuration --output-on-failure
    $testExit = $LASTEXITCODE
    Get-Content -LiteralPath $testLogs
    if ($testExit) { throw 'Tests failed.' }
} finally { $env:PATH = $oldPath }
