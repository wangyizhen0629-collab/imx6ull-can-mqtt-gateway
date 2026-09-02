[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$artifactDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$expectedHead = '6ee5d475f5451e6cba72f0041613009ed9fc9250'
$requiredAncestor = '691c3bd'
$script:ExitCodes = @{}
$script:Outputs = @{}

$generatedFiles = @(
    '01_git_status_before.txt',
    '02_git_fetch_origin.txt',
    '03_git_pull_ff_only.txt',
    '04_git_head.txt',
    '05_git_origin_master.txt',
    '06_git_required_ancestor.txt',
    '07_analyzer_regression_location.txt',
    '08_analyzer_tests.txt',
    '09_gate_status.txt',
    '10_tool_versions.txt',
    '11_preflight_validation.txt',
    'preflight_exit.txt'
)

foreach ($name in $generatedFiles) {
    $path = Join-Path $artifactDir $name
    if (Test-Path -LiteralPath $path) {
        throw "Refusing to overwrite existing evidence: $path"
    }
}

function Invoke-LoggedNative {
    param(
        [Parameter(Mandatory = $true)][string]$Name,
        [Parameter(Mandatory = $true)][string]$CommandText,
        [Parameter(Mandatory = $true)][scriptblock]$Action
    )

    $started = Get-Date -Format 'o'
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $lines = @(& $Action 2>&1 | ForEach-Object { $_.ToString() })
    $exitCode = $LASTEXITCODE
    $ErrorActionPreference = $savedErrorActionPreference
    if ($null -eq $exitCode) {
        $exitCode = 0
    }
    $finished = Get-Date -Format 'o'

    $content = @(
        "command=$CommandText",
        "started_local=$started",
        "finished_local=$finished",
        "exit_code=$exitCode",
        '--- output ---'
    ) + $lines
    $content | Out-File -LiteralPath (Join-Path $artifactDir ($Name + '.txt')) -Encoding utf8
    $script:ExitCodes[$Name] = [int]$exitCode
    $script:Outputs[$Name] = $lines
}

Invoke-LoggedNative -Name '01_git_status_before' -CommandText 'git status --short --branch' -Action {
    git status --short --branch
}
Invoke-LoggedNative -Name '02_git_fetch_origin' -CommandText 'git fetch origin' -Action {
    git fetch origin
}
Invoke-LoggedNative -Name '03_git_pull_ff_only' -CommandText 'git pull --ff-only origin master' -Action {
    git pull --ff-only origin master
}
Invoke-LoggedNative -Name '04_git_head' -CommandText 'git rev-parse HEAD' -Action {
    git rev-parse HEAD
}
Invoke-LoggedNative -Name '05_git_origin_master' -CommandText 'git rev-parse origin/master' -Action {
    git rev-parse origin/master
}
Invoke-LoggedNative -Name '06_git_required_ancestor' -CommandText "git merge-base --is-ancestor $requiredAncestor HEAD" -Action {
    git merge-base --is-ancestor $requiredAncestor HEAD
}
Invoke-LoggedNative -Name '07_analyzer_regression_location' -CommandText 'rg -n test_extended_target_id_is_rejected tools/protocol/test_analyze_m10_candump.py' -Action {
    rg -n 'test_extended_target_id_is_rejected' 'tools/protocol/test_analyze_m10_candump.py'
}
Invoke-LoggedNative -Name '08_analyzer_tests' -CommandText 'python tools/protocol/test_analyze_m10_candump.py -v' -Action {
    python 'tools/protocol/test_analyze_m10_candump.py' -v
}
Invoke-LoggedNative -Name '09_gate_status' -CommandText 'rg -n gate status docs/PLANS.md docs/milestones/M9.md docs/milestones/M10.md' -Action {
    rg -n 'M9.*MET|M10.*NOT MET|status.*MET|status.*NOT MET|Gate.*MET|Gate.*NOT MET|门禁.*MET|门禁.*NOT MET' 'docs/PLANS.md' 'docs/milestones/M9.md' 'docs/milestones/M10.md'
}

$versionPath = Join-Path $artifactDir '10_tool_versions.txt'
$versionLines = [System.Collections.Generic.List[string]]::new()
$versionLines.Add('command=Windows/PowerShell/Git/Python/Keil/ARMCC/CubeMX/ST-Link/Mosquitto/OpenSSH version inspection')
$versionLines.Add("started_local=$(Get-Date -Format 'o')")
$versionLines.Add('exit_code=0')
$versionLines.Add('--- output ---')
$versionLines.Add("windows_version=$([System.Environment]::OSVersion.VersionString)")
$versionLines.Add("powershell_version=$($PSVersionTable.PSVersion.ToString())")
$versionLines.Add("git_version=$((& git --version 2>&1) -join ' ')")
$versionLines.Add("python_version=$((& python --version 2>&1) -join ' ')")

$versionFiles = @(
    @{ Label = 'keil_uv4'; Path = 'D:\keil5\UV4\UV4.exe' },
    @{ Label = 'cubemx'; Path = 'D:\STM32CubeMX\STM32CubeMX.exe' },
    @{ Label = 'stlink_upgrade'; Path = 'D:\keil5\ARM\STLink\ST-LinkUpgrade.exe' },
    @{ Label = 'stlink_usb_driver'; Path = 'D:\keil5\ARM\STLink\STLinkUSBDriver.dll' },
    @{ Label = 'mosquitto_broker'; Path = 'E:\mosquitto\mosquitto.exe' },
    @{ Label = 'mosquitto_pub'; Path = 'E:\mosquitto\mosquitto_pub.exe' },
    @{ Label = 'mosquitto_sub'; Path = 'E:\mosquitto\mosquitto_sub.exe' },
    @{ Label = 'openssh'; Path = 'C:\Windows\System32\OpenSSH\ssh.exe' }
)
foreach ($entry in $versionFiles) {
    if (Test-Path -LiteralPath $entry.Path) {
        $info = (Get-Item -LiteralPath $entry.Path).VersionInfo
        $versionLines.Add("$($entry.Label)_path=$($entry.Path)")
        $versionLines.Add("$($entry.Label)_file_version=$($info.FileVersion)")
        $versionLines.Add("$($entry.Label)_product_version=$($info.ProductVersion)")
    }
    else {
        $versionLines.Add("$($entry.Label)=NOT FOUND at expected path $($entry.Path)")
    }
}

$armccPath = 'D:\keil5\ARM\ARMCC\bin\armcc.exe'
if (Test-Path -LiteralPath $armccPath) {
    $savedErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    $armccVersion = @(& $armccPath --vsn 2>&1 | ForEach-Object { $_.ToString() })
    $armccExit = $LASTEXITCODE
    $ErrorActionPreference = $savedErrorActionPreference
    $versionLines.Add("armcc_path=$armccPath")
    $versionLines.Add("armcc_version_exit=$armccExit")
    foreach ($line in $armccVersion) { $versionLines.Add("armcc_version=$line") }
}
else {
    $versionLines.Add("armcc=NOT FOUND at expected path $armccPath")
}
$versionLines.Add("finished_local=$(Get-Date -Format 'o')")
$versionLines | Out-File -LiteralPath $versionPath -Encoding utf8

$head = (($script:Outputs['04_git_head'] | Select-Object -Last 1).Trim())
$originHead = (($script:Outputs['05_git_origin_master'] | Select-Object -Last 1).Trim())
$analyzerText = $script:Outputs['08_analyzer_tests'] -join "`n"
$locationText = $script:Outputs['07_analyzer_regression_location'] -join "`n"
$trackedChanges = @(git status --porcelain --untracked-files=no 2>&1 | ForEach-Object { $_.ToString() })
$trackedStatusExit = $LASTEXITCODE

$checks = [ordered]@{
    git_status_exit_zero = ($script:ExitCodes['01_git_status_before'] -eq 0)
    git_fetch_exit_zero = ($script:ExitCodes['02_git_fetch_origin'] -eq 0)
    git_pull_ff_only_exit_zero = ($script:ExitCodes['03_git_pull_ff_only'] -eq 0)
    head_matches_expected = ($script:ExitCodes['04_git_head'] -eq 0 -and $head -eq $expectedHead)
    origin_master_matches_expected = ($script:ExitCodes['05_git_origin_master'] -eq 0 -and $originHead -eq $expectedHead)
    required_commit_is_ancestor = ($script:ExitCodes['06_git_required_ancestor'] -eq 0)
    extended_id_regression_present = ($script:ExitCodes['07_analyzer_regression_location'] -eq 0 -and $locationText -match 'test_extended_target_id_is_rejected')
    analyzer_exit_zero = ($script:ExitCodes['08_analyzer_tests'] -eq 0)
    analyzer_ran_8_tests = ($analyzerText -match 'Ran 8 tests')
    analyzer_result_ok = ($analyzerText -match '(?m)^OK\s*$')
    gate_status_query_exit_zero = ($script:ExitCodes['09_gate_status'] -eq 0)
    tracked_worktree_clean = ($trackedStatusExit -eq 0 -and $trackedChanges.Count -eq 0)
}

$validation = [System.Collections.Generic.List[string]]::new()
$validation.Add('command=preflight validation')
$validation.Add("started_local=$(Get-Date -Format 'o')")
$validation.Add("expected_head=$expectedHead")
$validation.Add("actual_head=$head")
$validation.Add("actual_origin_master=$originHead")
$validation.Add("required_ancestor=$requiredAncestor")
$validation.Add("tracked_status_exit=$trackedStatusExit")
if ($trackedChanges.Count -eq 0) {
    $validation.Add('tracked_status_output=(empty)')
}
else {
    foreach ($line in $trackedChanges) { $validation.Add("tracked_status_output=$line") }
}
$failed = 0
foreach ($item in $checks.GetEnumerator()) {
    $result = if ($item.Value) { 'PASS' } else { 'FAIL' }
    if (-not $item.Value) { $failed++ }
    $validation.Add("check=$($item.Key) result=$result")
}
$overallExit = if ($failed -eq 0) { 0 } else { 1 }
$validation.Add("failed_checks=$failed")
$validation.Add("result=$(if ($overallExit -eq 0) { 'PASS' } else { 'FAIL' })")
$validation.Add("finished_local=$(Get-Date -Format 'o')")
$validation.Add("exit_code=$overallExit")
$validation | Out-File -LiteralPath (Join-Path $artifactDir '11_preflight_validation.txt') -Encoding utf8
@(
    "local_time=$(Get-Date -Format 'o')",
    "exit_code=$overallExit"
) | Out-File -LiteralPath (Join-Path $artifactDir 'preflight_exit.txt') -Encoding utf8

exit $overallExit
