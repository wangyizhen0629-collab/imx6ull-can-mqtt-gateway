$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$BackupDir = Join-Path $PrivateDir 'pre_whitespace_normalization'
$MapPath = Join-Path $RunDir 'whitespace_normalization_map.txt'
if (Test-Path -LiteralPath $BackupDir) { throw "refusing to overwrite preserved preimages: $BackupDir" }
if (Test-Path -LiteralPath $MapPath) { throw "refusing to overwrite evidence: $MapPath" }
[void](New-Item -ItemType Directory -Path $BackupDir)

$Map = New-Object Collections.Generic.List[string]
foreach ($File in Get-ChildItem -LiteralPath $RunDir -File | Sort-Object Name) {
    $OriginalBytes = [IO.File]::ReadAllBytes($File.FullName)
    $Text = [Text.UTF8Encoding]::new($false, $true).GetString($OriginalBytes)
    $Newline = if ($Text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $Lines = @($Text -split '\r?\n')
    while ($Lines.Count -gt 0 -and $Lines[-1] -eq '') {
        if ($Lines.Count -eq 1) { $Lines = @() } else { $Lines = $Lines[0..($Lines.Count - 2)] }
    }
    $CleanLines = @($Lines | ForEach-Object { $_ -replace '[ \t]+$', '' })
    $Normalized = ($CleanLines -join $Newline) + $Newline
    $NormalizedBytes = [Text.UTF8Encoding]::new($false).GetBytes($Normalized)
    if (-not [Linq.Enumerable]::SequenceEqual([byte[]]$OriginalBytes, [byte[]]$NormalizedBytes)) {
        $BackupPath = Join-Path $BackupDir $File.Name
        [IO.File]::WriteAllBytes($BackupPath, $OriginalBytes)
        $OldSha = (Get-FileHash -LiteralPath $BackupPath -Algorithm SHA256).Hash.ToLowerInvariant()
        [IO.File]::WriteAllBytes($File.FullName, $NormalizedBytes)
        $NewSha = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        $Map.Add("file=$($File.Name) old_sha256=$OldSha new_sha256=$NewSha preimage=private_raw/pre_whitespace_normalization/$($File.Name)")
    }
}
$Map.Insert(0, "normalized_file_count=$($Map.Count)")
$Map.Add('operation=strip trailing spaces/tabs; collapse trailing blank lines; preserve original newline style and one final newline')
$Map.Add('preimages=Git-ignored private_raw; no preimage deleted or overwritten')
[IO.File]::WriteAllLines($MapPath, $Map, [Text.UTF8Encoding]::new($false))
"NORMALIZED_FILE_COUNT=$($Map.Count - 3)"
