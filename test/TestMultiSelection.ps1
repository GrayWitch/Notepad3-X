param(
    [string]$EditSource,
    # Use an existing Release x64 library, or build Scintilla in the temporary directory.
    [string]$ScintillaLibrary
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (!$EditSource) { $EditSource = Join-Path $root 'src\Edit.c' }
$temp = Join-Path ([IO.Path]::GetTempPath()) ('np3-multiselection-' + [guid]::NewGuid())
[void](New-Item -ItemType Directory $temp)
try {
    $source = Get-Content $EditSource -Raw
    $names = @('_EditSelectionExpandCarets', '_EditSelectionMatchText', '_EditSelectionFindNext',
               'EditSelectWordAtPos', 'EditSelectionMultiSelectAll', 'EditSelectionMultiSelectNext', 'EditSelectionMultiSelectSkip',
               'EditSelectionMultiSelectAllEx', '_EditSelectAllFromFind')
    $functions = foreach ($name in $names) {
        $match = [regex]::Match($source, "(?m)^(?:static )?(?:void|char\*|bool) $name\(")
        if (!$match.Success) {
            if ($name.StartsWith('_Edit')) { continue } # Allow pre-fix sources for regression checks.
            throw "Missing $name"
        }
        $body = $source.IndexOf('{', $match.Index)
        $end = [regex]::Match($source.Substring($body), '(?m)^}')
        if (!$end.Success) { throw "Missing end of $name" }
        $source.Substring($match.Index, $body + $end.Index + 1 - $match.Index)
    }
    $code = $functions -join "`n"
    $findClose = if ($source.Contains('static void _EditSelectAllFromFind(')) { 1 } else { 0 }
    Set-Content (Join-Path $temp 'MultiSelection.inc') ("#define TEST_FIND_CLOSE $findClose`n" + $code) -Encoding utf8

    # Reuse the exact production wrapper definitions for the calls under test.
    $wrappers = Get-Content (Join-Path $root 'src\SciCall.h') -Raw
    $macroStart = $wrappers.IndexOf('#define DeclareSciCallR0(')
    $macroEnd = $wrappers.IndexOf('//=============================================================================', $macroStart)
    $testCode = Get-Content (Join-Path $PSScriptRoot 'TestMultiSelection.cpp') -Raw
    $used = [regex]::Matches(($code + $testCode), 'SciCall_(\w+)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    $declarations = foreach ($name in $used) {
        $match = [regex]::Match($wrappers, "(?m)^DeclareSciCall\w+\($name,.*$")
        if (!$match.Success) { throw "Missing SciCall_$name wrapper" }
        $match.Value
    }
    Set-Content (Join-Path $temp 'MultiSelectionWrappers.inc') ($wrappers.Substring($macroStart, $macroEnd - $macroStart) + ($declarations -join "`n")) -Encoding utf8

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'Visual C++ build tools not found' }
    if (!$ScintillaLibrary) {
        $msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
        & $msbuild (Join-Path $root 'scintilla\Scintilla.vcxproj') /p:Configuration=Release /p:Platform=x64 "/p:OutDir=$temp\lib\" "/p:IntDir=$temp\obj\" /nologo /v:minimal
        if ($LASTEXITCODE -ne 0) { throw 'Scintilla test build failed' }
        $ScintillaLibrary = Join-Path $temp 'lib\Scintilla.lib'
    }
    $ScintillaLibrary = (Resolve-Path $ScintillaLibrary).Path
    $vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvars64.bat'
    $test = Join-Path $PSScriptRoot 'TestMultiSelection.cpp'
    $include = Join-Path $root 'scintilla\include'
    & $env:ComSpec /d /c "call `"$vcvars`" >nul && cd /d `"$temp`" && cl /nologo /EHsc /std:c++20 /MT /W4 /WX /utf-8 /I`"$temp`" /I`"$include`" `"$test`" `"$ScintillaLibrary`" user32.lib gdi32.lib imm32.lib ole32.lib oleaut32.lib advapi32.lib shlwapi.lib comctl32.lib uuid.lib /Fe:TestMultiSelection.exe"
    if ($LASTEXITCODE -ne 0) { throw 'Multi-selection test build failed' }
    & (Join-Path $temp 'TestMultiSelection.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Multi-selection tests failed' }
} finally {
    Remove-Item $temp -Recurse -Force
}
