$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$BoardHost = ((Get-Content (Join-Path $PrivateDir 'board_endpoint.txt') -Raw) -split '=', 2)[1].Trim()
$ExportDir = Join-Path $PrivateDir 'board-export'
$Archive = Join-Path $ExportDir 'board-export.tar'
$RemoteHashFile = Join-Path $ExportDir 'board-export.tar.sha256'
$ListFile = Join-Path $ExportDir 'tar.list.txt'
$ExtractDir = Join-Path $ExportDir 'extracted'
$RawTranscript = Join-Path $PrivateDir 'fetch_board_export.txt'
$PublicReport = Join-Path $RunDir 'fetch_board_export.redacted.txt'
$Trace = Join-Path $RunDir 'fetch_board_export.redaction-trace.txt'

foreach ($Path in @($ExportDir, $RawTranscript, $PublicReport, $Trace)) {
    if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite existing export evidence: $Path" }
}
New-Item -ItemType Directory -Path $ExportDir | Out-Null
$Started = Get-Date -Format o
$ArchiveRemote = "root@${BoardHost}:/tmp/m8-reactor-gate-20260901T170636/board-export.tar"
$HashRemote = "root@${BoardHost}:/tmp/m8-reactor-gate-20260901T170636/board-export.tar.sha256"
$ArchiveOutput = & scp -o BatchMode=yes -o ConnectTimeout=10 $ArchiveRemote $Archive 2>&1 | Out-String
$ArchiveExit = $LASTEXITCODE
$HashOutput = & scp -o BatchMode=yes -o ConnectTimeout=10 $HashRemote $RemoteHashFile 2>&1 | Out-String
$HashExit = $LASTEXITCODE
@(
    "started_at=$Started"
    "archive_command=scp $ArchiveRemote $Archive"
    "archive_exit=$ArchiveExit"
    $ArchiveOutput.TrimEnd()
    "hash_command=scp $HashRemote $RemoteHashFile"
    "hash_exit=$HashExit"
    $HashOutput.TrimEnd()
    "ended_at=$((Get-Date).ToString('o'))"
) | Out-File -LiteralPath $RawTranscript -Encoding utf8
if ($ArchiveExit -ne 0 -or $HashExit -ne 0) { throw 'SCP board export failed' }

$ExpectedHash = ((Get-Content -LiteralPath $RemoteHashFile -Raw).Trim() -split '\s+')[0].ToLowerInvariant()
$ActualHash = (Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($ExpectedHash -ne $ActualHash) { throw 'downloaded board export hash mismatch' }
$Entries = @(& tar -tf $Archive 2>&1)
$TarListExit = $LASTEXITCODE
$Entries | Out-File -LiteralPath $ListFile -Encoding utf8
if ($TarListExit -ne 0 -or $Entries.Count -eq 0) { throw 'cannot list board export tar' }
foreach ($Entry in $Entries) {
    $Normalized = $Entry.Replace('\', '/')
    if ($Normalized.StartsWith('/') -or $Normalized -match '^[A-Za-z]:' -or
        $Normalized -match '(^|/)\.\.(/|$)') {
        throw "unsafe tar entry: $Entry"
    }
}
New-Item -ItemType Directory -Path $ExtractDir | Out-Null
& tar -xf $Archive -C $ExtractDir
if ($LASTEXITCODE -ne 0) { throw 'board export extraction failed' }
$ExtractedFiles = @(Get-ChildItem -LiteralPath $ExtractDir -Recurse -File)
if ($ExtractedFiles.Count -eq 0) { throw 'board export extraction produced no files' }

@(
    "started_at=$Started"
    'archive_source=root@<REDACTED_BOARD_HOST>:/tmp/m8-reactor-gate-20260901T170636/board-export.tar'
    "scp_archive_exit=$ArchiveExit"
    "scp_hash_exit=$HashExit"
    "remote_sha256=$ExpectedHash"
    "local_sha256=$ActualHash"
    "archive_bytes=$((Get-Item -LiteralPath $Archive).Length)"
    "tar_list_exit=$TarListExit"
    "tar_entry_count=$($Entries.Count)"
    'path_safety=PASS (no absolute, drive-qualified, or parent traversal entries)'
    "extracted_file_count=$($ExtractedFiles.Count)"
    'extraction=PASS (private_raw only)'
    "ended_at=$((Get-Date).ToString('o'))"
    'status=PASS'
) | Out-File -LiteralPath $PublicReport -Encoding utf8
@(
    "raw_transcript_sha256=$((Get-FileHash -LiteralPath $RawTranscript -Algorithm SHA256).Hash.ToLowerInvariant())"
    "redacted_report_sha256=$((Get-FileHash -LiteralPath $PublicReport -Algorithm SHA256).Hash.ToLowerInvariant())"
    'redaction_method=公开报告不复制 SCP 原始输出，并将目标地址替换为 <REDACTED_BOARD_HOST>；原始归档仅保存在 private_raw'
) | Out-File -LiteralPath $Trace -Encoding utf8
Get-Content -LiteralPath $PublicReport
