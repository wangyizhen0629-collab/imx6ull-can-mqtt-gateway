$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Source = Join-Path $RunDir 'git_diff_cached_check.v3.txt'
$BackupDir = Join-Path $RunDir 'private_raw/pre_whitespace_normalization_v3'
$Backup = Join-Path $BackupDir 'git_diff_cached_check.v3.txt'
$MapPath = Join-Path $RunDir 'whitespace_normalization_map_v2.txt'
if (Test-Path -LiteralPath $BackupDir) { throw "refusing to overwrite preserved preimage: $BackupDir" }
if (Test-Path -LiteralPath $MapPath) { throw "refusing to overwrite evidence: $MapPath" }
[void](New-Item -ItemType Directory -Path $BackupDir)
$Original = [IO.File]::ReadAllBytes($Source)
[IO.File]::WriteAllBytes($Backup, $Original)
$Text = [Text.UTF8Encoding]::new($false, $true).GetString($Original)
$Newline = if ($Text.Contains("`r`n")) { "`r`n" } else { "`n" }
$Lines = @($Text -split '\r?\n')
while ($Lines.Count -gt 0 -and $Lines[-1] -eq '') {
    if ($Lines.Count -eq 1) { $Lines = @() } else { $Lines = $Lines[0..($Lines.Count - 2)] }
}
$Normalized = (($Lines | ForEach-Object { $_ -replace '[ \t]+$', '' }) -join $Newline) + $Newline
[IO.File]::WriteAllText($Source, $Normalized, [Text.UTF8Encoding]::new($false))
$OldSha = (Get-FileHash -LiteralPath $Backup -Algorithm SHA256).Hash.ToLowerInvariant()
$NewSha = (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()
$Map = @(
    'normalized_file_count=1'
    "file=git_diff_cached_check.v3.txt old_sha256=$OldSha new_sha256=$NewSha preimage=private_raw/pre_whitespace_normalization_v3/git_diff_cached_check.v3.txt"
    'operation=collapse trailing blank lines; preserve content and one final newline'
    'preimage=Git-ignored private_raw; no preimage deleted or overwritten'
)
[IO.File]::WriteAllLines($MapPath, $Map, [Text.UTF8Encoding]::new($false))
'NORMALIZED_FILE_COUNT=1'
