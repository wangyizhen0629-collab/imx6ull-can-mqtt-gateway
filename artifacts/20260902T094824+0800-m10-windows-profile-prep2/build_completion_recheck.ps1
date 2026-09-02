$ErrorActionPreference = 'Stop'

$artifactDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $artifactDir '..\..')).Path
$projectRel = 'stm32/firmware/imx6ull-can-mqtt-gateway/MDK-ARM/imx6ull-can-mqtt-gateway.uvprojx'
$project = Join-Path $repoRoot $projectRel
$targets = @('M10_111', 'M10_500', 'M10_1000')

function Write-NewUtf8 {
    param([string]$Path, [object[]]$Value)
    if (Test-Path -LiteralPath $Path) {
        throw "Refusing to overwrite evidence file: $Path"
    }
    $Value | Out-File -LiteralPath $Path -Encoding utf8
}

Set-Location -LiteralPath $repoRoot

$assessment = @()
foreach ($target in $targets) {
    $log = Join-Path $artifactDir ("keil_{0}_rebuild.txt" -f $target.ToLowerInvariant())
    $content = Get-Content -LiteralPath $log -Raw
    $pass = ($content -match "Rebuild target '$target'") -and
            ($content -match "0 Error\(s\), 0 Warning\(s\)\.") -and
            ($content -match "Using Compiler 'V5\.06 update 6 \(build 750\)'")
    $assessment += "target=$target log_pass=$($pass.ToString().ToUpperInvariant()) sha256=$((Get-FileHash -LiteralPath $log -Algorithm SHA256).Hash.ToLowerInvariant())"
}
Write-NewUtf8 -Path (Join-Path $artifactDir 'keil_log_assessment.txt') -Value $assessment

$products = @()
foreach ($target in $targets) {
    $suffix = $target.Substring(4).ToLowerInvariant()
    $dir = Join-Path (Split-Path -Parent $project) $target
    foreach ($extension in @('axf', 'hex')) {
        $product = Join-Path $dir ("gateway_m10_{0}.{1}" -f $suffix, $extension)
        if (-not (Test-Path -LiteralPath $product -PathType Leaf)) {
            throw "Missing build product after uVision completion: $product"
        }
        $item = Get-Item -LiteralPath $product
        $hash = Get-FileHash -LiteralPath $product -Algorithm SHA256
        $relative = $product.Substring($repoRoot.Length + 1).Replace('\', '/')
        $products += "target=$target type=$extension sha256=$($hash.Hash.ToLowerInvariant()) size=$($item.Length) mtime_local=$($item.LastWriteTime.ToString('yyyy-MM-ddTHH:mm:ss.fffK')) path=$relative"
    }
}
Write-NewUtf8 -Path (Join-Path $artifactDir 'build_product_sha256_after_completion.txt') -Value $products

[xml]$xml = Get-Content -LiteralPath $project -Raw
$settings = foreach ($target in $xml.Project.Targets.Target) {
    "target=$($target.TargetName) compiler=$($target.pCCUsed) uAC6=$($target.uAC6) debug_information=$($target.TargetOption.TargetCommonOption.DebugInformation) optim=$($target.TargetOption.TargetArmAds.Cads.Optim) create_hex=$($target.TargetOption.TargetCommonOption.CreateHexFile) define=$($target.TargetOption.TargetArmAds.Cads.VariousControls.Define)"
}
Write-NewUtf8 -Path (Join-Path $artifactDir 'keil_target_settings.txt') -Value $settings

$uv4Processes = @(Get-Process -Name UV4 -ErrorAction SilentlyContinue)
$processLines = @("uv4_process_count=$($uv4Processes.Count)")
foreach ($process in $uv4Processes) {
    $processLines += "pid=$($process.Id) path=$($process.Path) responding=$($process.Responding)"
}
Write-NewUtf8 -Path (Join-Path $artifactDir 'uv4_process_after.txt') -Value $processLines

$scanTargets = Get-ChildItem -LiteralPath $artifactDir -File | Where-Object {
    $_.Name -notin @('sensitive_scan.txt', 'artifact_manifest.sha256', 'artifact_manifest_check.txt')
}
$patterns = @(
    '-----BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY-----',
    '(?i)(password|passwd|secret|token|api[_-]?key)\s*[:=]\s*\S+',
    '(?<![0-9])(?:10|127|169\.254|172\.(?:1[6-9]|2[0-9]|3[01])|192\.168)(?:\.[0-9]{1,3}){2}(?![0-9])'
)
$findings = @()
foreach ($file in $scanTargets) {
    foreach ($pattern in $patterns) {
        $match = Select-String -LiteralPath $file.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue
        if ($match) {
            $findings += "file=$($file.Name) pattern=$pattern"
        }
    }
}
if ($findings.Count -eq 0) {
    Write-NewUtf8 -Path (Join-Path $artifactDir 'sensitive_scan.txt') -Value @('result=PASS', "files_scanned=$($scanTargets.Count)", 'scope=private-key markers, credential assignments, private/loopback/link-local IPv4 literals')
}
else {
    Write-NewUtf8 -Path (Join-Path $artifactDir 'sensitive_scan.txt') -Value @('result=FAIL') + $findings
    throw 'Sensitive-information scan failed.'
}

$manifestPath = Join-Path $artifactDir 'artifact_manifest.sha256'
$checkPath = Join-Path $artifactDir 'artifact_manifest_check.txt'
if ((Test-Path -LiteralPath $manifestPath) -or (Test-Path -LiteralPath $checkPath)) {
    throw 'Refusing to overwrite manifest evidence.'
}
$manifestFiles = Get-ChildItem -LiteralPath $artifactDir -File | Where-Object {
    $_.Name -notin @('artifact_manifest.sha256', 'artifact_manifest_check.txt')
} | Sort-Object Name
$manifest = foreach ($file in $manifestFiles) {
    $hash = Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant())  $($file.Name)"
}
$manifest | Out-File -LiteralPath $manifestPath -Encoding ascii

$checkFailures = @()
foreach ($line in Get-Content -LiteralPath $manifestPath) {
    if ($line -notmatch '^([0-9a-f]{64})  (.+)$') {
        $checkFailures += "malformed=$line"
        continue
    }
    $actual = (Get-FileHash -LiteralPath (Join-Path $artifactDir $Matches[2]) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Matches[1]) {
        $checkFailures += "mismatch=$($Matches[2])"
    }
}
if ($checkFailures.Count -eq 0) {
    'result=PASS' | Out-File -LiteralPath $checkPath -Encoding ascii
}
else {
    @('result=FAIL') + $checkFailures | Out-File -LiteralPath $checkPath -Encoding ascii
    throw 'Artifact manifest verification failed.'
}

$assessment | Write-Output
$products | Write-Output
$processLines | Write-Output
Write-Output 'sensitive_scan=PASS'
Write-Output 'artifact_manifest_check=PASS'
