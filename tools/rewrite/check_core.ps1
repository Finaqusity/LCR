param(
    [string]$ArmClang = 'D:\Keil_v5\ARM\ARMCLANG\bin\armclang.exe',
    [string]$CmsisCore = 'D:\Keil_v5\ARM\PACK\ARM\CMSIS\5.0.1\CMSIS\Include'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$outputDirectory = Join-Path $projectRoot 'firmware_next\build-check'
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
$compilerFlags = @('--target=arm-arm-none-eabi', '-mcpu=cortex-m4',
    '-mfpu=fpv4-sp-d16', '-mfloat-abi=hard', '-std=c11', '-O2',
    '-Wall', '-Wextra', '-Werror', "-I$(Join-Path $projectRoot 'firmware_next\include')")
$compilerFlags += @('-DSTM32F407xx',
    "-I$CmsisCore",
    "-I$(Join-Path $projectRoot 'firmware\Drivers\CMSIS\Device\ST\STM32F4xx\Include')")
$sources = @()
foreach ($sourceDirectory in @('src', 'tests')) {
    $sources += @(Get-ChildItem (Join-Path $projectRoot "firmware_next\$sourceDirectory") -Filter '*.c' -File |
        ForEach-Object { "$sourceDirectory\$($_.Name)" })
}
$platformDirectory = Join-Path $projectRoot 'firmware_next\platform'
if (Test-Path $platformDirectory) {
    $sources += @(Get-ChildItem $platformDirectory -Filter '*.c' -File |
        ForEach-Object { "platform\$($_.Name)" })
}
foreach ($relativeSource in $sources) {
    $sourcePath = Join-Path $projectRoot "firmware_next\$relativeSource"
    $objectPath = Join-Path $outputDirectory (([IO.Path]::GetFileNameWithoutExtension($relativeSource)) + '.o')
    & $ArmClang @compilerFlags -c $sourcePath -o $objectPath
    if ($LASTEXITCODE -ne 0) { throw "Target compilation failed: $relativeSource" }
}
Write-Host "Cortex-M4 compilation passed for $($sources.Count) sources. No target image linked or executed."
