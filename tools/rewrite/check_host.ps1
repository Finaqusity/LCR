param(
    [string]$VsRoot = 'D:\vs studio',
    [string]$WindowsSdkRoot = 'D:\Windows Kits\10'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$sourceRoot = Join-Path $projectRoot 'firmware_next'
$msvc = Get-ChildItem (Join-Path $VsRoot 'VC\Tools\MSVC') -Directory |
    Sort-Object Name -Descending | Select-Object -First 1
$sdk = Get-ChildItem (Join-Path $WindowsSdkRoot 'Include') -Directory |
    Where-Object { Test-Path (Join-Path $_.FullName 'ucrt\stdio.h') } |
    Sort-Object Name -Descending | Select-Object -First 1
if (!$msvc -or !$sdk) { throw 'MSVC/Windows SDK not found. Pass -VsRoot and -WindowsSdkRoot, or use CMake with a native compiler.' }
$compilerDirectory = Join-Path $msvc.FullName 'bin\Hostx64\x64'
$compiler = Join-Path $compilerDirectory 'cl.exe'
$outputDirectory = Join-Path $sourceRoot 'build-host'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$savedPath, $savedInclude, $savedLib = $env:PATH, $env:INCLUDE, $env:LIB
try {
    $savedPreview = $env:LCR_PREVIEW_DIR
    $env:LCR_PREVIEW_DIR = $outputDirectory
    $env:PATH = "$compilerDirectory;$savedPath"
    $env:INCLUDE = "$(Join-Path $msvc.FullName 'include');$($sdk.FullName)\ucrt;$($sdk.FullName)\shared;$($sdk.FullName)\um"
    $sdkLib = Join-Path $WindowsSdkRoot "Lib\$($sdk.Name)"
    $env:LIB = "$(Join-Path $msvc.FullName 'lib\x64');$sdkLib\ucrt\x64;$sdkLib\um\x64"
    $librarySources = @(Get-ChildItem (Join-Path $sourceRoot 'src') -Filter '*.c' -File | ForEach-Object FullName)
    $testSources = @(Get-ChildItem (Join-Path $sourceRoot 'tests') -Filter 'test_*.c' -File)
    foreach ($testSource in $testSources) {
        $suite = $testSource.BaseName
        $executable = Join-Path $outputDirectory "$suite.exe"
        $sourcePaths = $librarySources + @($testSource.FullName)
        & $compiler /nologo /std:c11 /W4 /WX /O2 /UNDEBUG "/I$(Join-Path $sourceRoot 'include')" @sourcePaths "/Fe$executable" "/Fo$outputDirectory\"
        if ($LASTEXITCODE -ne 0) { throw "Host compilation failed: $suite" }
        & $executable
        if ($LASTEXITCODE -ne 0) { throw "Host tests failed: $suite" }
    }
} finally {
    $env:LCR_PREVIEW_DIR = $savedPreview
    $env:PATH, $env:INCLUDE, $env:LIB = $savedPath, $savedInclude, $savedLib
}
Write-Host 'All native C tests executed successfully.'
