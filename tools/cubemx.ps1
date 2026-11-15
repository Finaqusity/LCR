#Requires -Version 5.1
<#
.SYNOPSIS
    STM32CubeMX 辅助脚本：检查安装 / 用 GUI 打开 .ioc / headless 生成参考工程。

.DESCRIPTION
    本仓库不用 CubeMX 托管工程（见 docs/开发环境.md 第 6 节），只用它做两件事：
      1) 图形化校验时钟树与 Pinout，生成参考代码对照手写寄存器配置；
      2) 把已配好的 .ioc 重新生成一遍，作为复现材料。

    重要限制（2026-11-15 实测，CubeMX 6.18.1-RC2）：
      * headless 模式只能 load/save/generate 已存在的 .ioc，**不能**用脚本配置外设，
        外设/时钟树必须在 GUI 里配。
      * 工程输出目录必须是纯 ASCII 路径。中文路径会让 CubeMX 回退目录并可能弹出
        “Save Project As” 对话框。
      * .ioc 里时钟树不自洽时，headless 生成会弹 “Warning: Code Generation ... [Clock]”
        模态框并等待人工点 Yes/No；本脚本在超时后会杀掉自己启动的 javaw 进程，
        所以请先在 GUI 里把 .ioc 配好、确认真实能生成，再来跑 -Gen。

.PARAMETER Check
    默认行为。打印本机 CubeMX / JRE / 固件包 / .svd 的检测结果。

.PARAMETER Open
    用 GUI 打开指定 .ioc（或空白工程）。

.PARAMETER Ioc
    headless 生成用的源 .ioc 路径。

.PARAMETER OutDir
    headless 生成的输出目录，必须纯 ASCII（如 D:\Tools\lcr-cubemx\out）。

.EXAMPLE
    tools\cubemx.ps1
    tools\cubemx.ps1 -Open 'D:\Tools\lcr-cubemx\lcr.ioc'
    tools\cubemx.ps1 -Ioc 'D:\Tools\lcr-cubemx\lcr.ioc' -OutDir 'D:\Tools\lcr-cubemx\out'
#>
param(
    [switch]$Check,
    [string]$Open,
    [string]$Ioc,
    [string]$OutDir,
    [int]$TimeoutSec = 180
)

$ErrorActionPreference = 'Stop'

function Find-CubeMx {
    $candidates = @(
        'D:\stm32cubemx\STM32CubeMX.exe',
        'D:\STM32CubeMX\STM32CubeMX.exe',
        (Join-Path $env:ProgramFiles 'STMicroelectronics\STM32Cube\STM32CubeMX\STM32CubeMX.exe')
    )
    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) { return (Resolve-Path -LiteralPath $c).Path }
    }
    $cmd = Get-Command STM32CubeMX.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    return $null
}

function Find-Repository([string]$installDir) {
    $repo = Join-Path $env:USERPROFILE 'STM32Cube\Repository'
    if (-not (Test-Path -LiteralPath $repo)) { return @() }
    return @(Get-ChildItem -LiteralPath $repo -Directory -Filter 'STM32Cube_FW_*' |
        Sort-Object Name |
        Select-Object -ExpandProperty FullName)
}

function Test-AsciiPath([string]$path) {
    return ($path -match '^[\x20-\x7E]+$')
}

function Invoke-CubeMxScript {
    param([string]$Exe, [string]$ScriptPath, [int]$Timeout)

    $log = [System.IO.Path]::ChangeExtension($ScriptPath, '.log')
    $err = [System.IO.Path]::ChangeExtension($ScriptPath, '.err.log')
    $started = Get-Date
    # 直接启动 exe（它自己再拉 javaw）；-q = headless，无 -s 交互提示
    $p = Start-Process -FilePath $Exe -ArgumentList @('-q', ('"' + $ScriptPath + '"')) -PassThru -WindowStyle Hidden -RedirectStandardOutput $log -RedirectStandardError $err

    # 先等 javaw 起来，再等它退出（exe 本身可能立刻返回）
    Start-Sleep -Seconds 4
    $deadline = (Get-Date).AddSeconds($Timeout)
    while ((Get-Date) -lt $deadline) {
        $running = @(Get-Process -Name javaw -ErrorAction SilentlyContinue |
            Where-Object { $_.StartTime -ge $started.AddSeconds(-5) })
        if ($running.Count -eq 0) { break }
        # 若日志里已经出现“等待人工确认”的警告，说明要弹模态框，直接杀掉
        if (Test-Path -LiteralPath $log) {
            $tail = Get-Content -LiteralPath $log -Tail 200 -ErrorAction SilentlyContinue
            if ($tail -match 'IP not ready for code generation') {
                foreach ($r in $running) { Stop-Process -Id $r.Id -Force -ErrorAction SilentlyContinue }
                Write-Host '[!] .ioc 的时钟/外设配置不自洽（日志出现 "IP not ready for code generation"）。' -ForegroundColor Yellow
                Write-Host '    CubeMX 会弹模态框等待点 Yes/No；已中止本次生成。' -ForegroundColor Yellow
                Write-Host '    处理：用 GUI 打开该 .ioc → 顶部 PCC 面板会标红 → 点 "Solve" 或手工修正时钟树后再试。' -ForegroundColor Yellow
                return 3
            }
        }
        Start-Sleep -Milliseconds 500
    }

    $remaining = @(Get-Process -Name javaw -ErrorAction SilentlyContinue |
        Where-Object { $_.StartTime -ge $started.AddSeconds(-5) })
    if ($remaining.Count -gt 0) {
        foreach ($r in $remaining) { Stop-Process -Id $r.Id -Force -ErrorAction SilentlyContinue }
        Write-Host ("[!] 超时 {0}s，已杀掉本次启动的 javaw。" -f $Timeout) -ForegroundColor Yellow
        return 4
    }

    $text = if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' }
    if ($text -match '(?m)^KO\s*$') {
        Write-Host '[!] CubeMX 脚本返回 KO（某条命令失败）。完整日志：' -ForegroundColor Red
        Write-Host "    $log"
        return 2
    }
    if ($text -notmatch 'Bye bye') {
        Write-Host '[!] 日志里没有 "Bye bye"，生成可能未完成。日志：' -ForegroundColor Yellow
        Write-Host "    $log"
        return 5
    }
    Write-Host "[ok] headless 生成完成，日志：$log" -ForegroundColor Green
    return 0
}

# ---------------------------------------------------------------- -Check
if (-not $Check -and -not $Open -and -not ($Ioc -and $OutDir)) { $Check = $true }
if (-not ($Open -or $Ioc -or $OutDir)) { $Check = $true }

if ($Check) {
    Write-Host '=== STM32CubeMX 环境检测 ==='
    $exe = Find-CubeMx
    if (-not $exe) {
        Write-Host '[x] 未找到 STM32CubeMX.exe' -ForegroundColor Red
        Write-Host '    安装：https://www.st.com/en/development-tools/stm32cubemx.html（约 1 GB，自带 JRE）'
        exit 1
    }
    Write-Host ("[ok] exe       : {0}" -f $exe)
    $vi = (Get-Item -LiteralPath $exe).VersionInfo
    Write-Host ("     版本      : {0} / {1}  ({2:N0} B)" -f $vi.FileVersion, $vi.ProductVersion, (Get-Item -LiteralPath $exe).Length)
    $installDir = Split-Path -Parent $exe
    $java = Join-Path $installDir 'jre\bin\java.exe'
    if (Test-Path -LiteralPath $java) {
        Write-Host ("[ok] 自带 JRE  : {0}" -f $java)
    }
    else {
        Write-Host '[!] 安装目录下未找到 jre\bin\java.exe（可能需要系统 Java）' -ForegroundColor Yellow
    }
    $um = Join-Path $installDir 'help\UM1718.pdf'
    Write-Host ("     命令手册  : {0}" -f $(if (Test-Path -LiteralPath $um) { $um } else { '(未找到 UM1718.pdf)' }))

    $repos = Find-Repository $installDir
    if ($repos.Count -eq 0) {
        Write-Host '[!] 未找到固件包仓库（%USERPROFILE%\STM32Cube\Repository\STM32Cube_FW_*）' -ForegroundColor Yellow
    }
    else {
        Write-Host '[ok] 固件包仓库：'
        foreach ($r in $repos) { Write-Host ("     {0}" -f $r) }
    }

    $svd = @()
    foreach ($r in $repos) {
        $svd += @(Get-ChildItem -LiteralPath $r -Recurse -Filter '*.svd' -ErrorAction SilentlyContinue)
    }
    if ($svd.Count -gt 0) { Write-Host ("[ok] 找到 SVD: {0}" -f $svd[0].FullName) }
    else { Write-Host '[!] 本机固件包内未找到 .svd（外设寄存器视图需另找，见 docs/开发环境.md §5）' -ForegroundColor Yellow }

    Write-Host ''
    Write-Host '用法：tools\cubemx.ps1 -Open <ioc>                                    # GUI 配置'
    Write-Host '      tools\cubemx.ps1 -Ioc <ioc> -OutDir D:\Tools\lcr-cubemx\out    # headless 生成（ASCII 路径）'
    exit 0
}

# ---------------------------------------------------------------- -Open
if ($Open) {
    $exe = Find-CubeMx
    if (-not $exe) { Write-Host '[x] 未找到 STM32CubeMX.exe' -ForegroundColor Red; exit 1 }
    if (-not (Test-Path -LiteralPath $Open)) {
        Write-Host ("[x] 文件不存在：{0}" -f $Open) -ForegroundColor Red
        Write-Host '    想从空白工程开始：先打开 GUI，File → New Project 选 STM32F407VETx，另存 .ioc。'
        exit 1
    }
    $Open = (Resolve-Path -LiteralPath $Open).Path
    Write-Host ("[..] 用 CubeMX 打开 {0}" -f $Open)
    Start-Process -FilePath $exe -ArgumentList ('"' + $Open + '"') | Out-Null
    exit 0
}

# ---------------------------------------------------------------- -Gen
if (-not $Ioc -or -not $OutDir) {
    Write-Host '[x] -Ioc 与 -OutDir 必须同时提供。' -ForegroundColor Red
    exit 2
}
$exe = Find-CubeMx
if (-not $exe) { Write-Host '[x] 未找到 STM32CubeMX.exe' -ForegroundColor Red; exit 1 }
if (-not (Test-Path -LiteralPath $Ioc)) { Write-Host ("[x] .ioc 不存在：{0}" -f $Ioc) -ForegroundColor Red; exit 2 }
$Ioc = (Resolve-Path -LiteralPath $Ioc).Path
if (-not (Test-AsciiPath $Ioc)) {
    Write-Host '[x] .ioc 路径含非 ASCII 字符（中文等）。headless 模式不支持，请先复制到纯 ASCII 路径。' -ForegroundColor Red
    exit 2
}
if (-not (Test-AsciiPath $OutDir)) {
    Write-Host '[x] -OutDir 含非 ASCII 字符。headless 生成的输出目录必须是纯 ASCII 路径。' -ForegroundColor Red
    Write-Host '    例：D:\Tools\lcr-cubemx\out'
    exit 2
}
if (-not (Test-Path -LiteralPath $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$name = [System.IO.Path]::GetFileNameWithoutExtension($Ioc)
$scriptPath = Join-Path $env:TEMP ('lcr-cubemx-' + [guid]::NewGuid().ToString('N') + '.tcl')
$lines = @(
    ('config load "' + $Ioc + '"'),
    ('project name ' + $name),
    ('project path "' + $OutDir + '"'),
    'project toolchain "CMake"',
    'project generate',
    'exit'
)
$lines | Out-File -FilePath $scriptPath -Encoding ascii

Write-Host ("[..] headless 生成：{0} → {1}" -f $Ioc, $OutDir)
$code = Invoke-CubeMxScript -Exe $exe -ScriptPath $scriptPath -Timeout $TimeoutSec
if ($code -ne 0) { exit $code }

$made = Get-ChildItem -LiteralPath $OutDir -ErrorAction SilentlyContinue
if ($made.Count -eq 0) {
    Write-Host '[!] 输出目录为空，生成可能静默失败，请检查日志。' -ForegroundColor Yellow
    exit 5
}
Write-Host ('[ok] 输出：')
$made | ForEach-Object { Write-Host ("     {0}" -f $_.Name) }
Write-Host ''
Write-Host '提醒：CubeMX 生成的代码只作“参考答案”对照（时钟树 / Pinout / 寄存器值）。'
Write-Host '     产品固件仍用手写寄存器版本，定稿数值以 docs/软件架构.md §14.1 为准。'
exit 0
