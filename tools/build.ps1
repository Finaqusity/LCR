# build.ps1 —— 一条命令编译固件（CMake + Ninja + arm-none-eabi-gcc）
# 用法：
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -Release
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\build.ps1 -Clean
param(
    [switch]$Release,
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$fw   = Join-Path $root "firmware"
$build = Join-Path $fw "build"

function Find-Exe($name, $extraRoots) {
    $hit = Get-Command $name -ErrorAction SilentlyContinue
    if ($null -ne $hit) { return $hit.Source }
    # 已知的便携工具链位置（避免递归搜索大目录）
    $known = @(
        "D:\Tools\embedded\arm-gcc-14.2\bin",
        "D:\Tools\embedded\arm-none-eabi-gcc\bin",
        "D:\Tools\embedded\bin",
        "C:\Users\Finaqusity\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\Scripts"
    )
    foreach ($k in $known) {
        $p = Join-Path $k $name
        if (Test-Path $p) { return $p }
    }
    foreach ($r in $extraRoots) {
        if (-not (Test-Path $r)) { continue }
        $f = Get-ChildItem -Path $r -Recurse -Filter $name -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($null -ne $f) { return $f.FullName }
    }
    return $null
}

$searchRoots = @(
    "D:\Tools\embedded",
    "D:\Tools\platformio\packages",
    "C:\Users\Finaqusity\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\Scripts"
)

$cmake = Find-Exe "cmake.exe" $searchRoots
$ninja = Find-Exe "ninja.exe" $searchRoots
$gcc   = Find-Exe "arm-none-eabi-gcc.exe" $searchRoots

if ($null -eq $cmake) { throw "找不到 cmake.exe，请先运行 tools\setup_toolchain.ps1" }
if ($null -eq $ninja) { throw "找不到 ninja.exe，请先运行 tools\setup_toolchain.ps1" }
if ($null -eq $gcc)   { throw "找不到 arm-none-eabi-gcc.exe，请先运行 tools\setup_toolchain.ps1" }

$gccBin = Split-Path $gcc -Parent
$cmakeBin = Split-Path $cmake -Parent
$ninjaBin = Split-Path $ninja -Parent
$env:Path = $gccBin + ";" + $cmakeBin + ";" + $ninjaBin + ";" + $env:Path

$gen = "Debug"
if ($Release) { $gen = "Release" }

Write-Host ("[build] gcc   = " + $gcc)
Write-Host ("[build] cmake = " + $cmake)
Write-Host ("[build] mode  = " + $gen)

if ($Clean -and (Test-Path $build)) {
    Remove-Item -Recurse -Force $build
    Write-Host "[build] 已清理 build/"
}

& $cmake -S $fw -B $build -G Ninja "-DCMAKE_BUILD_TYPE=$gen" "-DCMAKE_TOOLCHAIN_FILE=$fw\cmake\arm-none-eabi.cmake"
if ($LASTEXITCODE -ne 0) { throw "cmake configure 失败" }

& $cmake --build $build
if ($LASTEXITCODE -ne 0) { throw "编译失败" }

Get-ChildItem $build -Filter "lcr-fw.*" | Where-Object { $_.Extension -in ".elf", ".hex", ".bin" } |
    Select-Object Name, @{ n = "KB"; e = { [math]::Round($_.Length / 1KB, 1) } } | Format-Table -AutoSize
Write-Host ("[build] 产物目录: " + $build) -ForegroundColor Green
