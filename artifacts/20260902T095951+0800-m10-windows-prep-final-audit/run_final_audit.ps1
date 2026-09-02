$ErrorActionPreference = 'Stop'

$artifactDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $artifactDir '..\..')).Path
$projectRoot = Join-Path $repoRoot 'stm32/firmware/imx6ull-can-mqtt-gateway'
$mdkRoot = Join-Path $projectRoot 'MDK-ARM'
$projectRel = 'stm32/firmware/imx6ull-can-mqtt-gateway/MDK-ARM/imx6ull-can-mqtt-gateway.uvprojx'

function Write-NewUtf8 {
    param([string]$Name, [object[]]$Value)
    $path = Join-Path $artifactDir $Name
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite audit output: $path"
    }
    $Value | Out-File -LiteralPath $path -Encoding utf8
}

function Invoke-Captured {
    param([string]$Name, [scriptblock]$Command)
    $path = Join-Path $artifactDir $Name
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite audit output: $path"
    }
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $captured = & $Command 2>&1 | ForEach-Object { $_.ToString() }
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $oldPreference
    @("exit_code=$exitCode") + $captured | Out-File -LiteralPath $path -Encoding utf8
    return $exitCode
}

function Test-Manifest {
    param([string]$Directory)
    $manifest = Join-Path $Directory 'artifact_manifest.sha256'
    $failures = @()
    $count = 0
    foreach ($line in Get-Content -LiteralPath $manifest) {
        $count++
        if ($line -notmatch '^([0-9a-f]{64})  (.+)$') {
            $failures += "malformed=$line"
            continue
        }
        $actual = (Get-FileHash -LiteralPath (Join-Path $Directory $Matches[2]) -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $Matches[1]) {
            $failures += "mismatch=$($Matches[2])"
        }
    }
    if ($failures.Count -ne 0) {
        throw "Manifest failed in $Directory`: $($failures -join ', ')"
    }
    return "directory=$([IO.Path]::GetFileName($Directory)) files=$count result=PASS"
}

Set-Location -LiteralPath $repoRoot
$start = Get-Date
Write-NewUtf8 -Name 'local_time.txt' -Value @(
    "timezone=$([System.TimeZoneInfo]::Local.Id)",
    "start_local=$($start.ToString('yyyy-MM-ddTHH:mm:ss.fffK'))"
)

$unitExit = Invoke-Captured -Name 'analyzer_unit_tests.txt' -Command {
    python -B tools/protocol/test_analyze_m10_candump.py
}
$generatorExit = Invoke-Captured -Name 'keil_target_generator_check.txt' -Command {
    python -B tools/stm32/generate_m10_keil_targets.py --project $projectRel --check
}

$realSummary = Join-Path $artifactDir 'archived_m4_real_capture_summary.json'
$realExit = Invoke-Captured -Name 'archived_m4_real_capture_replay.txt' -Command {
    python -B tools/protocol/analyze_m10_candump.py `
        --candump artifacts/20260831T111733+0800-m4-stm32-physical-final/candump_60s.log `
        --can-before artifacts/20260831T111733+0800-m4-stm32-physical-final/can_before.txt `
        --can-after artifacts/20260831T111733+0800-m4-stm32-physical-final/can_after.txt `
        --profile 111 --minimum-duration-seconds 59 --summary-json $realSummary
}

$parseExit = Invoke-Captured -Name 'syntax_and_format_checks.txt' -Command {
    python -B -c "import ast,json,pathlib,xml.etree.ElementTree as ET; files=['tools/protocol/analyze_m10_candump.py','tools/protocol/test_analyze_m10_candump.py','tools/stm32/generate_m10_keil_targets.py']; [ast.parse(pathlib.Path(p).read_text(encoding='utf-8')) for p in files]; json.loads(pathlib.Path('protocol/m10_traffic_profiles.json').read_text(encoding='utf-8')); ET.parse('$($projectRel.Replace('\','/'))'); print('python_json_xml=PASS')"
}

$dependencyLines = @()
$dependencyFailures = @()
$newHeader = 'stm32/firmware/imx6ull-can-mqtt-gateway/Core/Inc/ecu_traffic_profile.h'
$dependencies = @()
Get-ChildItem -LiteralPath (Join-Path $mdkRoot 'M10_500') -Filter '*.d' -File | ForEach-Object {
    foreach ($line in Get-Content -LiteralPath $_.FullName) {
        if ($line -match '^[^:]+:\s+(.+)$') {
            $dependency = $Matches[1]
            if ($dependency -notmatch '^[A-Za-z]:\\') {
                $dependencies += $dependency
            }
        }
    }
}
foreach ($dependency in $dependencies | Sort-Object -Unique) {
    $absolute = [IO.Path]::GetFullPath((Join-Path $mdkRoot $dependency))
    $relative = $absolute.Substring($repoRoot.Length + 1).Replace('\', '/')
    git ls-files --error-unmatch -- $relative 2>$null | Out-Null
    $tracked = ($LASTEXITCODE -eq 0)
    $intendedNew = ($relative -eq $newHeader)
    $dependencyLines += "tracked=$tracked intended_new=$intendedNew path=$relative"
    if (-not $tracked -and -not $intendedNew) {
        $dependencyFailures += $relative
    }
}
$dependencyLines += "dependency_count=$($dependencies.Count) unique_count=$(($dependencies | Sort-Object -Unique).Count) failure_count=$($dependencyFailures.Count)"
Write-NewUtf8 -Name 'keil_dependency_tracking_audit.txt' -Value $dependencyLines
if ($dependencyFailures.Count -ne 0) {
    throw "Keil build used unexpected untracked dependencies: $($dependencyFailures -join ', ')"
}

$previousManifest = @(
    Test-Manifest -Directory (Join-Path $repoRoot 'artifacts/20260902T094551+0800-m10-windows-profile-prep'),
    Test-Manifest -Directory (Join-Path $repoRoot 'artifacts/20260902T094824+0800-m10-windows-profile-prep2')
)
Write-NewUtf8 -Name 'previous_manifest_checks.txt' -Value $previousManifest

$ignoreLines = @()
foreach ($target in @('M10_111', 'M10_500', 'M10_1000')) {
    $relative = "stm32/firmware/imx6ull-can-mqtt-gateway/MDK-ARM/$target/"
    git check-ignore -q -- $relative
    $ignored = ($LASTEXITCODE -eq 0)
    $trackedProducts = @(git ls-files -- $relative)
    $ignoreLines += "target=$target ignored=$ignored tracked_product_count=$($trackedProducts.Count)"
    if (-not $ignored -or $trackedProducts.Count -ne 0) {
        throw "Build product scope check failed for $target"
    }
}
Write-NewUtf8 -Name 'build_product_git_audit.txt' -Value $ignoreLines

$diffOutput = git diff --check 2>&1
$diffExit = $LASTEXITCODE
Write-NewUtf8 -Name 'git_diff_check.txt' -Value @("exit_code=$diffExit") + $diffOutput
if ($diffExit -ne 0) { throw 'git diff --check failed.' }

$intended = @(
    '.gitattributes', '.gitignore', 'README.md',
    'docs/DECISION_LOG.md', 'docs/OPEN_QUESTIONS.md', 'docs/PLANS.md',
    'docs/PROJECT_SPEC.md', 'docs/RESUME_TRACEABILITY.md', 'docs/TEST_PLAN.md',
    'docs/milestones/M10.md', 'docs/milestones/M10_STM32_PROFILE_DESIGN.md',
    'docs/milestones/M10_WINDOWS_HARDWARE_CHECKLIST.md',
    'gateway/CMakeLists.txt', 'protocol/README.md', 'protocol/m10_traffic_profiles.json',
    'stm32/firmware/imx6ull-can-mqtt-gateway/Core/Inc/ecu_traffic_profile.h',
    'stm32/firmware/imx6ull-can-mqtt-gateway/Core/Src/main.c',
    'stm32/firmware/imx6ull-can-mqtt-gateway/MDK-ARM/imx6ull-can-mqtt-gateway.uvprojx',
    'tools/README.md', 'tools/protocol/analyze_m10_candump.py',
    'tools/protocol/test_analyze_m10_candump.py', 'tools/stm32/generate_m10_keil_targets.py',
    'artifacts/20260902T094551+0800-m10-windows-profile-prep/',
    'artifacts/20260902T094824+0800-m10-windows-profile-prep2/',
    'artifacts/20260902T095951+0800-m10-windows-prep-final-audit/'
)
Write-NewUtf8 -Name 'intended_commit_scope.txt' -Value $intended

$scanFiles = @()
foreach ($entry in $intended) {
    if (Test-Path -LiteralPath $entry -PathType Leaf) {
        $scanFiles += Get-Item -LiteralPath $entry
    }
    elseif (Test-Path -LiteralPath $entry -PathType Container) {
        $scanFiles += Get-ChildItem -LiteralPath $entry -File -Recurse
    }
}
$scanFiles = $scanFiles | Sort-Object FullName -Unique | Where-Object {
    $_.Name -notin @('sensitive_scan.txt', 'artifact_manifest.sha256', 'artifact_manifest_check.txt')
}
$patterns = @(
    '-----BEGIN (RSA |OPENSSH |EC )?PRIVATE KEY-----',
    '(?i)(password|passwd|secret|token|api[_-]?key)\s*[:=]\s*\S+',
    '(?<![0-9])(?:10|127|169\.254|172\.(?:1[6-9]|2[0-9]|3[01])|192\.168)(?:\.[0-9]{1,3}){2}(?![0-9])'
)
$findings = @()
foreach ($file in $scanFiles) {
    foreach ($pattern in $patterns) {
        if (Select-String -LiteralPath $file.FullName -Pattern $pattern -AllMatches -ErrorAction SilentlyContinue) {
            $findings += "path=$($file.FullName.Substring($repoRoot.Length + 1)) pattern=$pattern"
        }
    }
}
if ($findings.Count -ne 0) {
    Write-NewUtf8 -Name 'sensitive_scan.txt' -Value @('result=FAIL') + $findings
    throw 'Intended commit sensitive-information scan failed.'
}
Write-NewUtf8 -Name 'sensitive_scan.txt' -Value @(
    'result=PASS', "files_scanned=$($scanFiles.Count)",
    'scope=private-key markers, credential assignments, private/loopback/link-local IPv4 literals'
)

$keyFiles = @(
    'protocol/m10_traffic_profiles.json',
    'stm32/firmware/imx6ull-can-mqtt-gateway/Core/Inc/ecu_traffic_profile.h',
    'stm32/firmware/imx6ull-can-mqtt-gateway/Core/Src/main.c',
    $projectRel,
    'tools/protocol/analyze_m10_candump.py',
    'tools/protocol/test_analyze_m10_candump.py',
    'tools/stm32/generate_m10_keil_targets.py'
)
$hashLines = foreach ($file in $keyFiles) {
    $item = Get-Item -LiteralPath $file
    $hash = Get-FileHash -LiteralPath $file -Algorithm SHA256
    "$($hash.Hash.ToLowerInvariant())  size=$($item.Length)  $file"
}
Write-NewUtf8 -Name 'key_source_sha256.txt' -Value $hashLines

$end = Get-Date
Add-Content -LiteralPath (Join-Path $artifactDir 'local_time.txt') -Encoding utf8 -Value @(
    "end_local=$($end.ToString('yyyy-MM-ddTHH:mm:ss.fffK'))",
    "elapsed_seconds=$([math]::Round(($end - $start).TotalSeconds, 3))"
)

Write-Output "unit_tests_exit=$unitExit"
Write-Output "generator_exit=$generatorExit"
Write-Output "real_capture_exit=$realExit"
Write-Output "parse_exit=$parseExit"
Write-Output 'dependency_tracking=PASS'
$previousManifest | Write-Output
$ignoreLines | Write-Output
Write-Output "diff_check_exit=$diffExit"
Write-Output 'sensitive_scan=PASS'
