param(
    [ValidateSet(0,100,514,1000)][int]$Jobs = 100,
    [int]$Repeats = 3,
    [switch]$NullRHI,
    [string]$EditorCmd = 'C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
)
$ErrorActionPreference = 'Stop'
$pluginDirectory = Split-Path $PSScriptRoot -Parent
$hostDirectory = Split-Path (Split-Path $pluginDirectory -Parent) -Parent
for ($sample = 0; $sample -lt $Repeats; $sample++) {
    $runDirectory = Join-Path $hostDirectory ('Saved\LibraryBench\' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $runDirectory -Force | Out-Null
    $projectPath = Join-Path $runDirectory 'AtlasLibraryHarness.uproject'
    @{
        FileVersion = 3; EngineAssociation = '5.5'
        AdditionalPluginDirectories = @((Split-Path $pluginDirectory -Parent))
        Plugins = @(@{ Name = 'AtlasWorkflow'; Enabled = $true })
    } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $projectPath -Encoding utf8
    Set-Content -LiteralPath (Join-Path $runDirectory 'AtlasLibraryBenchmark.fixture') -Value 'Disposable synthetic history; never load production archives.'
    if ($Jobs -eq 514) {
        New-Item -ItemType Directory -Path (Join-Path $runDirectory 'Content') -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $hostDirectory 'Content\AtlasWorkflows') -Destination (Join-Path $runDirectory 'Content\AtlasWorkflows') -Recurse
    }
    # Generate before editor startup. History/config are synthetic; the 514-job case uses copied workflow assets.
    & python (Join-Path $PSScriptRoot 'CreateLibraryFixture.py') $runDirectory $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Fixture generation failed.' }
    # Confine startup reconciliation as well as the test itself, before any UObject initializes.
    $configDirectory = Join-Path $runDirectory 'Config'
    New-Item -ItemType Directory -Path $configDirectory -Force | Out-Null
    $fixtureOutput = (Join-Path $runDirectory 'Saved\Atlas\Output').Replace('\', '/')
    @"
[/Script/AtlasSDK.AtlasSDKSettings]
OutputFolder=(Path="$fixtureOutput")
bCleanTempOnEditorStart=False
bReadApiKeyFromEnvironment=False
WorkspaceApiKey=
"@ | Set-Content -LiteralPath (Join-Path $configDirectory 'DefaultEditorPerProjectUserSettings.ini') -Encoding utf8
    $cacheDirectory = Join-Path $hostDirectory 'Saved\LibraryBench\DDC'
    if ($cacheDirectory.Length -gt 119) { throw 'DDC path exceeds Unreal limit.' }
    $previousCache = [Environment]::GetEnvironmentVariable('UE-LocalDataCachePath', 'Process')
    [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $cacheDirectory, 'Process')
    try {
        $arguments = @(
            $projectPath, '/Engine/Maps/Entry', '-unattended', '-nop4', '-nosplash', '-nosound', '-RenderOffscreen',
            '-ddc=InstalledNoZenLocalFallback', '-DDC-ForceMemoryCache', '-SharedDataCachePath=None',
            '-AtlasLibraryProfile', '-AtlasLibraryBenchmark', "-AtlasLibraryJobs=$Jobs", "-AtlasLibraryWorkflows=$(if ($Jobs -eq 514) { 85 } elseif ($Jobs -eq 0) { 0 } else { 5 })",
            '-ExecCmds=Automation RunTests Atlas.Performance.LibraryOpening', '-TestExit=Automation Test Queue Empty',
            "-ReportExportPath=$runDirectory\Report", "-abslog=$runDirectory\Automation.log", '-stdout', '-FullStdOutLogOutput',
            '-ini:EditorPerProjectUserSettings:[/Script/AtlasSDK.AtlasSDKSettings]:bCleanTempOnEditorStart=False',
            '-ini:EditorPerProjectUserSettings:[/Script/AtlasSDK.AtlasSDKSettings]:bReadApiKeyFromEnvironment=False',
            '-ini:EditorPerProjectUserSettings:[/Script/AtlasSDK.AtlasSDKSettings]:WorkspaceApiKey='
        )
        if ($NullRHI) { $arguments += '-nullrhi' }
        Write-Output "Starting $Jobs jobs, repetition $($sample + 1): $runDirectory"
        & $EditorCmd @arguments *> (Join-Path $runDirectory 'Console.log')
        if ($LASTEXITCODE -ne 0) { throw "Editor failed; inspect $runDirectory" }
        $report = Get-Content -LiteralPath (Join-Path $runDirectory 'Report\index.json') -Raw | ConvertFrom-Json
        if ($report.failed -ne 0 -or ($report.succeeded + $report.succeededWithWarnings) -ne 1) { throw "Benchmark failed; inspect $runDirectory" }
        Write-Output "Passed: $runDirectory\Saved\AtlasProfiling"
    } finally {
        [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath', $previousCache, 'Process')
    }
}
