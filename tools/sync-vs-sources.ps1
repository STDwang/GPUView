$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$ideRoot = Join-Path $projectRoot 'ide\vs2022'
New-Item -ItemType Directory -Path $ideRoot -Force | Out-Null

# VS C++工程文件中写明确的相对路径；目录枚举由脚本承担，不要求逐项维护。
# 不直接修改CMake生成的vcxproj，它们会在重新配置时被覆盖。
$entries = @()
foreach ($folder in @('src', 'tests', 'tools', 'cmake')) {
    $entries += Get-ChildItem -LiteralPath (Join-Path $projectRoot $folder) -Recurse -File |
        Where-Object { $_.Extension -in @('.cpp', '.cxx', '.h', '.hpp', '.ui', '.qrc', '.ps1', '.cmake') }
}
$entries += Get-Item -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt')
$entries = $entries | Sort-Object FullName
$items = @()
$filterItems = @()
$folders = [Collections.Generic.SortedSet[string]]::new()
foreach ($entry in $entries) {
    $relative = $entry.FullName.Substring($projectRoot.Length + 1).Replace('/', '\')
    $path = [Security.SecurityElement]::Escape('..\..\' + $relative)
    $kind = if ($entry.Extension -in @('.cpp','.cxx')) { 'ClCompile' } elseif ($entry.Extension -in @('.h','.hpp')) { 'ClInclude' } else { 'None' }
    $items += "    <$kind Include=`"$path`" />"
    $folder = Split-Path $relative -Parent
    if ($folder) {
        $filter = [Security.SecurityElement]::Escape($folder)
        $filterItems += "    <$kind Include=`"$path`"><Filter>$filter</Filter></$kind>"
        while ($folder) { [void]$folders.Add($folder); $folder = Split-Path $folder -Parent }
    } else { $filterItems += "    <$kind Include=`"$path`" />" }
}
$header = '<?xml version="1.0" encoding="utf-8"?>' + "`n" + '<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">'
$sources = $header + "`n  <ItemGroup>`n" + ($items -join "`n") + "`n  </ItemGroup>`n</Project>`n"
$filterDefinitions = @($folders | ForEach-Object { '    <Filter Include="' + [Security.SecurityElement]::Escape($_) + '" />' })
$filters = $header + "`n  <ItemGroup>`n" + ($filterDefinitions -join "`n") + "`n  </ItemGroup>`n  <ItemGroup>`n" + ($filterItems -join "`n") + "`n  </ItemGroup>`n</Project>`n"
foreach ($pair in @(@('GPUView.sources.props', $sources), @('GPUView.vcxproj.filters', $filters))) {
    $target = Join-Path $ideRoot $pair[0]
    if (-not (Test-Path -LiteralPath $target) -or [IO.File]::ReadAllText($target).Replace("`r`n", "`n") -ne $pair[1]) {
        [IO.File]::WriteAllText($target, $pair[1], [Text.UTF8Encoding]::new($false))
    }
}
