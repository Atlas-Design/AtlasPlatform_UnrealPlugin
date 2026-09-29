param(
    [string]$EditorCmd = 'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$TestFilter = 'Atlas.TempStorage',
    [int]$ExpectedTests = 4,
    [string[]]$ExtraEditorArguments = @()
)
$ErrorActionPreference = 'Stop'
$pluginDirectory = Split-Path $PSScriptRoot -Parent
$hostDirectory = Split-Path (Split-Path $pluginDirectory -Parent) -Parent
$runDirectory = Join-Path $hostDirectory ('Saved\CleanupTests\' + [guid]::NewGuid().ToString('N'))
$cacheDirectory = Join-Path $runDirectory 'DDC'
if ($cacheDirectory.Length -gt 119) { throw 'Unreal requires a DDC path of at most 119 characters; use a shorter host project path.' }
New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $runDirectory 'Content') -Force | Out-Null

# A separate, content-free host prevents editor startup from reconciling the user's real history.
$projectPath = Join-Path $runDirectory 'AtlasCleanupHarness.uproject'
@{
    FileVersion = 3
    EngineAssociation = '5.5'
    AdditionalPluginDirectories = @((Split-Path $pluginDirectory -Parent))
    Plugins = @(@{ Name = 'AtlasWorkflow'; Enabled = $true })
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $projectPath -Encoding utf8

$linkRoot = Join-Path $runDirectory 'JunctionFixture'
$targetDirectory = Join-Path $linkRoot 'Target'
$scratchDirectory = Join-Path $linkRoot 'Scratch'
New-Item -ItemType Directory -Path $targetDirectory, $scratchDirectory -Force | Out-Null
Set-Content -LiteralPath (Join-Path $targetDirectory 'Important.txt') -Value 'Preserve junction target'
Set-Content -LiteralPath (Join-Path $scratchDirectory 'Keep.txt') -Value 'Preserve on failed preflight'
$canonicalScratch = $scratchDirectory.Replace('\', '/')
[IO.File]::WriteAllText((Join-Path $scratchDirectory '.atlas-temp-storage'),
    "AtlasWorkflow temp storage v1`n$canonicalScratch", [Text.UTF8Encoding]::new($false))
New-Item -ItemType Junction -Path (Join-Path $linkRoot 'Link') -Target $targetDirectory | Out-Null
New-Item -ItemType Junction -Path (Join-Path $scratchDirectory 'NestedLink') -Target $targetDirectory | Out-Null

$previousFixture = $env:ATLAS_TEMP_TEST_LINK_ROOT
$previousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath', 'Process')
[Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $cacheDirectory, 'Process')
$env:ATLAS_TEMP_TEST_LINK_ROOT = $linkRoot
try {
    $reportDirectory = Join-Path $runDirectory 'Report'
    $logPath = Join-Path $runDirectory 'Automation.log'
    $arguments = @(
        $projectPath, '-unattended', '-nop4', '-nosplash', '-nullrhi', '-nosound',
        '-ddc=InstalledNoZenLocalFallback', '-DDC-ForceMemoryCache', '-SharedDataCachePath=None',
        "-ExecCmds=Automation RunTests $TestFilter",
        '-TestExit=Automation Test Queue Empty',
        "-ReportExportPath=$reportDirectory",
        "-abslog=$logPath", '-stdout', '-FullStdOutLogOutput',
        '-ini:EditorPerProjectUserSettings:[/Script/AtlasSDK.AtlasSDKSettings]:bCleanTempOnEditorStart=False'
    )
    & $EditorCmd @arguments @ExtraEditorArguments *> (Join-Path $runDirectory 'Console.log')
    $editorExit = $LASTEXITCODE
    Write-Output "Run directory: $runDirectory"
    Write-Output "Editor exit code: $editorExit"
    if (Test-Path -LiteralPath $logPath) {
        Select-String -LiteralPath $logPath -Pattern 'Test Completed|Test Succeeded|Test Failed|Error:|TEST COMPLETE' |
            Select-Object -Last 20 | ForEach-Object { $_.Line }
    }
    if ($editorExit -ne 0) { throw "Unreal automation failed; inspect $runDirectory" }
    $reportPath = Join-Path $reportDirectory 'index.json'
    if (!(Test-Path -LiteralPath $reportPath)) { throw "No automation report produced: $reportPath" }
    $report = Get-Content -LiteralPath $reportPath -Raw | ConvertFrom-Json
    if ($report.failed -ne 0 -or ($report.succeeded + $report.succeededWithWarnings) -ne $ExpectedTests) {
        throw "Expected $ExpectedTests completed tests with no failures; inspect $reportPath"
    }
    Write-Output "All $ExpectedTests $TestFilter tests passed."
} finally {
    $env:ATLAS_TEMP_TEST_LINK_ROOT = $previousFixture
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $previousCache, 'Process')
}
# Retain fixtures and reports for inspection. No recursive deletion occurs in this runner.
