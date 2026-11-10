param(
    [string]$Url = "https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/14.2.rel1/binrel/arm-gnu-toolchain-14.2.rel1-mingw-w64-x86_64-arm-none-eabi.zip",
    [string]$Out = "D:\Tools\embedded\_downloads\arm-gcc-14.2.rel1.zip",
    [long]$Total = 312299563,
    [int]$Threads = 16
)

$ErrorActionPreference = "Continue"
$ua = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/124.0"
$work = Join-Path (Split-Path $Out) ("parts_" + [IO.Path]::GetFileNameWithoutExtension($Out))
New-Item -ItemType Directory -Force -Path $work | Out-Null

$chunk = [long][math]::Ceiling($Total / $Threads)
"total=$Total chunk=$chunk threads=$Threads work=$work"

$procs = @()
for ($i = 0; $i -lt $Threads; $i++) {
    $s = $i * $chunk
    $e = [math]::Min($s + $chunk - 1, $Total - 1)
    if ($s -ge $Total) { break }
    $part = Join-Path $work ("part{0:d2}" -f $i)
    $want = $e - $s + 1
    $argsStr = "-sS -L --retry 8 --retry-delay 1 --max-time 900 -A `"$ua`" -r $s-$e -o `"$part`" `"$Url`""
    $p = Start-Process -FilePath "curl.exe" -ArgumentList $argsStr -NoNewWindow -PassThru `
        -RedirectStandardError (Join-Path $work ("err{0:d2}.txt" -f $i))
    $procs += [pscustomobject]@{ Proc = $p; Part = $part; Want = $want; Idx = $i }
}

foreach ($it in $procs) { $it.Proc.WaitForExit() }

$bad = @()
foreach ($it in $procs) {
    $has = if (Test-Path $it.Part) { (Get-Item $it.Part).Length } else { 0 }
    if ($has -ne $it.Want) {
        # 重试一次（单块）
        $s = $it.Idx * $chunk
        $e = [math]::Min($s + $chunk - 1, $Total - 1)
        & curl.exe -sS -L --retry 8 --retry-delay 1 --max-time 900 -A $ua -r "$s-$e" -o $it.Part $Url
        $has = if (Test-Path $it.Part) { (Get-Item $it.Part).Length } else { 0 }
    }
    if ($has -ne $it.Want) { $bad += "$($it.Part) got=$has want=$($it.Want)" }
}

if ($bad.Count -gt 0) {
    "CHUNK_FAILED:"
    $bad | ForEach-Object { "  $_" }
    exit 1
}
"all chunks OK"

$parts = $procs | Sort-Object Idx | ForEach-Object { $_.Part }
$list = ($parts -join "+")
& cmd.exe /c "copy /b $list `"$Out`"" | Out-Null
$len = (Get-Item $Out).Length
"assembled $Out size=$len expected=$Total"
if ($len -ne $Total) { exit 2 }
"OK"
