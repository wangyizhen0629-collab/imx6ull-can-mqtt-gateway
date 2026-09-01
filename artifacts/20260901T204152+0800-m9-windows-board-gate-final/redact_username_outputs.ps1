$ErrorActionPreference = 'Stop'
$RunDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$PrivateDir = Join-Path $RunDir 'private_raw'
$Quarantine = Join-Path $PrivateDir 'quarantine_username'
$EndpointFile = Join-Path $PrivateDir 'board_endpoint.txt'
$UserFile = Join-Path $PrivateDir 'board_user.txt'
$MapPath = Join-Path $RunDir 'username_redaction_map.txt'
if (Test-Path -LiteralPath $Quarantine) { throw 'username quarantine already exists' }
if (Test-Path -LiteralPath $MapPath) { throw 'username redaction map already exists' }
New-Item -ItemType Directory -Path $Quarantine | Out-Null
$BoardHost = ((Get-Content -LiteralPath $EndpointFile -Raw) -split '=', 2)[1].Trim()
$BoardUser = ((Get-Content -LiteralPath $UserFile -Raw) -split '=', 2)[1].Trim()
$Needle = "$BoardUser@"
$Candidates = @(Get-ChildItem -LiteralPath $RunDir -File -Filter '*.redacted.txt' | Where-Object {
    (Get-Content -LiteralPath $_.FullName -Raw).Contains($Needle)
} | Sort-Object Name)
$Map = New-Object Collections.Generic.List[string]
$Map.Add("created_at=$((Get-Date).ToString('o'))")
$Map.Add("candidate_count=$($Candidates.Count)")
foreach ($File in $Candidates) {
    $Suffix = '.redacted.txt'
    $Prefix = $File.Name.Substring(0, $File.Name.Length - $Suffix.Length)
    $PublicName = "$Prefix.public.txt"
    $PublicPath = Join-Path $RunDir $PublicName
    $PublicTrace = Join-Path $RunDir "$Prefix.public-redaction-trace.txt"
    $OldTrace = Join-Path $RunDir "$Prefix.redaction-trace.txt"
    $PrivateDestination = Join-Path $Quarantine $File.Name
    foreach ($Path in @($PublicPath, $PublicTrace, $PrivateDestination)) {
        if (Test-Path -LiteralPath $Path) { throw "refusing to overwrite: $Path" }
    }
    $Original = Get-Content -LiteralPath $File.FullName -Raw
    $OriginalHash = (Get-FileHash -LiteralPath $File.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    $Redacted = $Original.Replace($Needle, '<REDACTED_USER>@').Replace($BoardHost, '<REDACTED_BOARD_HOST>')
    $Redacted = [regex]::Replace($Redacted, '(?<![0-9])(?:[0-9]{1,3}\.){1,3}[0-9]{1,3}(?![0-9])', '<REDACTED_DOTTED_NUMERIC>')
    [IO.File]::WriteAllText($PublicPath, $Redacted, [Text.UTF8Encoding]::new($false))
    $PublicHash = (Get-FileHash -LiteralPath $PublicPath -Algorithm SHA256).Hash.ToLowerInvariant()
    $UserLeak = ([regex]::Matches($Redacted, [regex]::Escape($Needle))).Count
    $EndpointLeak = ([regex]::Matches($Redacted, [regex]::Escape($BoardHost))).Count
    $DottedLeak = ([regex]::Matches($Redacted, '(?<![0-9])(?:[0-9]{1,3}\.){1,3}[0-9]{1,3}(?![0-9])')).Count
    $Trace = @(
        "quarantined_original_sha256=$OriginalHash"
        "public_sha256=$PublicHash"
        'redaction_method=private login username;exact endpoint;all 2-to-4-component dotted numeric sequences'
        "login_username_leak_count=$UserLeak"
        "exact_endpoint_leak_count=$EndpointLeak"
        "dotted_numeric_leak_count=$DottedLeak"
        "status=$(if ($UserLeak + $EndpointLeak + $DottedLeak -eq 0) { 'PASS' } else { 'FAIL' })"
    ) -join "`r`n"
    [IO.File]::WriteAllText($PublicTrace, $Trace + "`r`n", [Text.UTF8Encoding]::new($false))
    if ($UserLeak + $EndpointLeak + $DottedLeak -ne 0) { throw "redaction failed: $($File.Name)" }
    Move-Item -LiteralPath $File.FullName -Destination $PrivateDestination
    $MovedHash = (Get-FileHash -LiteralPath $PrivateDestination -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($MovedHash -ne $OriginalHash) { throw "quarantine hash mismatch: $($File.Name)" }
    if (Test-Path -LiteralPath $OldTrace -PathType Leaf) {
        Move-Item -LiteralPath $OldTrace -Destination (Join-Path $Quarantine (Split-Path -Leaf $OldTrace))
    }
    $Map.Add("original=$($File.Name) quarantined_sha256=$OriginalHash public=$PublicName public_sha256=$PublicHash")
}
[IO.File]::WriteAllLines($MapPath, $Map, [Text.UTF8Encoding]::new($false))
Get-Content -LiteralPath $MapPath
