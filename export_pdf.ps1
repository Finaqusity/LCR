param(
    [Parameter(Mandatory = $true)][string]$Docx,
    [Parameter(Mandatory = $true)][string]$Pdf
)
$ErrorActionPreference = "Stop"
$log = Join-Path (Split-Path -Parent $Pdf) "export_pdf.log"
"BEGIN $(Get-Date -Format o)" | Out-File -FilePath $log -Encoding utf8
$word = New-Object -ComObject Word.Application
$word.Visible = $false
$word.DisplayAlerts = 0
$doc = $word.Documents.Open($Docx, $false, $true)
"PAGES $($doc.ComputeStatistics(2))" | Out-File -FilePath $log -Append -Encoding utf8
$doc.ExportAsFixedFormat($Pdf, 17)
$doc.Close($false)
$word.Quit()
[System.Runtime.InteropServices.Marshal]::ReleaseComObject($doc) | Out-Null
[System.Runtime.InteropServices.Marshal]::ReleaseComObject($word) | Out-Null
"OK $(Get-Date -Format o)" | Out-File -FilePath $log -Append -Encoding utf8
