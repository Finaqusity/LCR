# flash.ps1 —— 用 DAPmini（CMSIS-DAP）经 pyOCD 烧录
# 用法：
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\flash.ps1
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\flash.ps1 -Target stm32f407vgtx
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools\flash.ps1 -Freq 1000 -File firmware\build\lcr-fw.hex
param(
    [string]$Target = "stm32f407vetx",
    [string]$File = "",
    [int]$Freq = 0,          # SWD 时钟 kHz；0 = 用 pyOCD 默认值
    [switch]$NoReset,
    [switch]$List
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

function Find-Exe($name) {
    $hit = Get-Command $name -ErrorAction SilentlyContinue
    if ($null -ne $hit) { return $hit.Source }
    $known = "C:\Users\Finaqusity\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\Scripts"
    $p = Join-Path $known $name
    if (Test-Path $p) { return $p }
    throw "找不到 $name，请先按 docs\环境搭建.md 安装"
}

$pyocd = Find-Exe "pyocd.exe"

if ($List) {
    & $pyocd list
    exit $LASTEXITCODE
}

if (-not $File) {
    $file = Join-Path $root "firmware\build\lcr-fw.hex"
} else {
    $file = (Resolve-Path $File).Path
}
if (-not (Test-Path $file)) {
    throw "找不到固件文件：$file（先跑 tools\build.ps1）"
}

Write-Host ("[flash] pyocd  = " + $pyocd)
Write-Host ("[flash] target = " + $Target)
Write-Host ("[flash] file   = " + $file)

$args = @("flash", "-t", $Target, $file, "--erase", "sector")
if ($Freq -gt 0) { $args += @("-f", "$Freq") }
& $pyocd @args
if ($LASTEXITCODE -ne 0) { throw ("pyocd 烧录失败，退出码 " + $LASTEXITCODE) }

if (-not $NoReset) {
    & $pyocd reset -t $Target
    if ($LASTEXITCODE -ne 0) { Write-Host "[flash] 复位命令失败，可手动按一下复位键" -ForegroundColor Yellow }
}
Write-Host "[flash] 完成：已烧录并复位运行" -ForegroundColor Green
