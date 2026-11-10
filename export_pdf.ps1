param(
    [Parameter(Mandatory = $true)][string]$Docx,
    [Parameter(Mandatory = $true)][string]$Pdf
)
$ErrorActionPreference = 'Stop'
$logFile = Join-Path (Split-Path -Parent $Pdf) 'export_pdf.log'
function Log([string]$m) { "$(Get-Date -Format HH:mm:ss) $m" | Add-Content -Encoding UTF8 $logFile }

"START ap=$([System.Threading.Thread]::CurrentThread.GetApartmentState())" | Set-Content -Encoding UTF8 $logFile
$word = $null
$doc = $null
try {
    $word = New-Object -ComObject Word.Application
    Log "WORD_OK"
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $word.Options.Pagination = $true
    Log "SETTINGS_OK"
    $doc = $word.Documents.Open($Docx, $false, $true)
    Log "OPEN_OK pages=$($doc.ComputeStatistics(2))"
    $doc.ExportAsFixedFormat($Pdf, 17)
    Log "EXPORT_OK"
}
catch {
    Log ("ERR " + $_.Exception.Message)
}
finally {
    if ($doc) { try { $doc.Close(0); Log "CLOSED" } catch { Log "CLOSE_ERR" } }
    if ($word) { try { $word.Quit(); Log "QUIT" } catch { Log "QUIT_ERR" } }
    [System.Runtime.InteropServices.Marshal]::ReleaseComObject($word) | Out-Null
}
