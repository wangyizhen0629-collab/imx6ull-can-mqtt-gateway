$ErrorActionPreference = 'Stop'

$artifactDir = $PSScriptRoot
$repoRoot = (Resolve-Path (Join-Path $artifactDir '..\..')).Path
$projectRel = 'stm32/firmware/imx6ull-can-mqtt-gateway/MDK-ARM/imx6ull-can-mqtt-gateway.uvprojx'
$project = Join-Path $repoRoot $projectRel
$uv4 = 'D:\keil5\UV4\UV4.exe'
$armcc = 'D:\keil5\ARM\ARMCC\bin\armcc.exe'

function Assert-NewFile {
    param([string]$Path)
    if (Test-Path -LiteralPath $Path) {
        throw "Refusing to overwrite evidence file: $Path"
    }
}

function Write-NewUtf8 {
    param([string]$Path, [object[]]$Value)
    Assert-NewFile -Path $Path
    $Value | Out-File -LiteralPath $Path -Encoding utf8
}

function Invoke-Captured {
    param([string]$OutputPath, [scriptblock]$Command)
    Assert-NewFile -Path $OutputPath
    $global:LASTEXITCODE = 0
    & $Command 2>&1 | Out-File -LiteralPath $OutputPath -Encoding utf8
    return $LASTEXITCODE
}

Set-Location -LiteralPath $repoRoot

$start = Get-Date
Write-NewUtf8 -Path (Join-Path $artifactDir 'local_time.txt') -Value @(
    "timezone=$([System.TimeZoneInfo]::Local.Id)",
    "start_local=$($start.ToString('yyyy-MM-ddTHH:mm:ss.fffK'))"
)

$environment = @(
    "powershell=$($PSVersionTable.PSVersion)",
    "python=$(python --version 2>&1)",
    "git=$(git --version)",
    "uv4_path=$uv4",
    "uv4_version=$((Get-Item -LiteralPath $uv4).VersionInfo.FileVersion)",
    "armcc_path=$armcc"
)
$armccVersion = & $armcc --vsn 2>&1
$environment += $armccVersion
Write-NewUtf8 -Path (Join-Path $artifactDir 'environment.txt') -Value $environment

Invoke-Captured -OutputPath (Join-Path $artifactDir 'git_head.txt') -Command {
    git rev-parse HEAD
} | Out-Null
Invoke-Captured -OutputPath (Join-Path $artifactDir 'git_status_before.txt') -Command {
    git status --short --branch
} | Out-Null

$generatorExit = Invoke-Captured -OutputPath (Join-Path $artifactDir 'keil_target_generator_check.txt') -Command {
    python -B tools/stm32/generate_m10_keil_targets.py --project $projectRel --check
}
$analyzerExit = Invoke-Captured -OutputPath (Join-Path $artifactDir 'analyzer_unit_tests.txt') -Command {
    python -B tools/protocol/test_analyze_m10_candump.py
}

$sourceFiles = @(
    'protocol/m10_traffic_profiles.json',
    'stm32/firmware/imx6ull-can-mqtt-gateway/imx6ull-can-mqtt-gateway.ioc',
    'stm32/firmware/imx6ull-can-mqtt-gateway/Core/Inc/ecu_traffic_profile.h',
    'stm32/firmware/imx6ull-can-mqtt-gateway/Core/Src/main.c',
    $projectRel,
    'tools/protocol/analyze_m10_candump.py',
    'tools/protocol/test_analyze_m10_candump.py',
    'tools/stm32/generate_m10_keil_targets.py'
)
$sourceHashes = foreach ($file in $sourceFiles) {
    $item = Get-Item -LiteralPath $file
    $hash = Get-FileHash -LiteralPath $file -Algorithm SHA256
    "{0}  size={1}  {2}" -f $hash.Hash.ToLowerInvariant(), $item.Length, $file
}
Write-NewUtf8 -Path (Join-Path $artifactDir 'source_sha256.txt') -Value $sourceHashes

$targets = @('M10_111', 'M10_500', 'M10_1000')
$buildResults = @()
foreach ($target in $targets) {
    $log = Join-Path $artifactDir ("keil_{0}_rebuild.txt" -f $target.ToLowerInvariant())
    Assert-NewFile -Path $log
    & $uv4 -r $project -t $target -j0 -o $log
    $exitCode = $LASTEXITCODE
    $buildResults += "target=$target exit_code=$exitCode log=$([IO.Path]::GetFileName($log))"
}
Write-NewUtf8 -Path (Join-Path $artifactDir 'keil_exit_codes.txt') -Value $buildResults

$productLines = @()
foreach ($target in $targets) {
    $suffix = $target.Substring(4).ToLowerInvariant()
    $dir = Join-Path (Split-Path -Parent $project) $target
    foreach ($extension in @('axf', 'hex')) {
        $product = Join-Path $dir ("gateway_m10_{0}.{1}" -f $suffix, $extension)
        if (Test-Path -LiteralPath $product) {
            $item = Get-Item -LiteralPath $product
            $hash = Get-FileHash -LiteralPath $product -Algorithm SHA256
            $relative = $product.Substring($repoRoot.Length + 1).Replace('\', '/')
            $productLines += "target=$target type=$extension sha256=$($hash.Hash.ToLowerInvariant()) size=$($item.Length) path=$relative"
        }
        else {
            $productLines += "target=$target type=$extension result=MISSING path=$product"
        }
    }
}
Write-NewUtf8 -Path (Join-Path $artifactDir 'build_product_sha256.txt') -Value $productLines

$end = Get-Date
Add-Content -LiteralPath (Join-Path $artifactDir 'local_time.txt') -Encoding utf8 -Value @(
    "end_local=$($end.ToString('yyyy-MM-ddTHH:mm:ss.fffK'))",
    "elapsed_seconds=$([math]::Round(($end - $start).TotalSeconds, 3))"
)

Write-Output "generator_exit=$generatorExit"
Write-Output "analyzer_exit=$analyzerExit"
$buildResults | Write-Output
$productLines | Write-Output
