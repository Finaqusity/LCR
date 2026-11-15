# monitor.ps1 —— 串口监视包装器：自动找可用 python（避开 Windows Store 别名）并转发参数给 tools\monitor.py
# 用法：
#   powershell -ExecutionPolicy Bypass -File tools\monitor.ps1
#   powershell -ExecutionPolicy Bypass -File tools\monitor.ps1 --list
#   powershell -ExecutionPolicy Bypass -File tools\monitor.ps1 --port COM5 --baud 115200 --log uart.log
param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$PyArgs
)

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

function Find-Python {
    $hit = Get-Command python -ErrorAction SilentlyContinue
    if ($hit -and $hit.Source -notmatch "WindowsApps" -and (Test-Path $hit.Source)) { return $hit.Source }
    $known = @(
        "C:\Users\Finaqusity\.dsh\dsh-runtimes\dsh-primary-runtime\dependencies\python\python.exe",
        "D:\Tools\embedded\python\python.exe",
        "$env:LOCALAPPDATA\Programs\Python\Python312\python.exe"
    )
    foreach ($k in $known) { if (Test-Path $k) { return $k } }
    throw "找不到可用的 python。安装后把路径加到 tools\monitor.ps1 的 `$known，或设 PATH（不要用 Windows Store 的 python 别名）。"
}

$py = Find-Python
$script = Join-Path $root "tools\monitor.py"
& $py $script @PyArgs
exit $LASTEXITCODE