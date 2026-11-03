# Tests only temporary meshes. The application's --ui-test mode isolates settings.
[CmdletBinding()]
param([switch]$SkipUI)
$ErrorActionPreference = 'Stop'
$installRoot = Split-Path -Parent $PSScriptRoot
$app = Join-Path $installRoot 'bin\sculptforge.exe'
$cli = Join-Path $installRoot 'bin\sculptforge-cli.exe'
$testRoot = Join-Path $env:LOCALAPPDATA ('SculptForge\InstallationTests\' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$results = New-Object 'System.Collections.Generic.List[object]'

function Run-Check([string]$Name, [string]$Executable, [string[]]$Arguments, [int]$Timeout = 300) {
    $stdout = Join-Path $testRoot ($Name + '.stdout.txt')
    $stderr = Join-Path $testRoot ($Name + '.stderr.txt')
    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -WorkingDirectory $testRoot -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    if (-not $process.WaitForExit($Timeout * 1000)) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        throw "$Name timed out. See $testRoot"
    }
    $process.WaitForExit()
    $process.Refresh()
    $code = $process.ExitCode
    if ($null -eq $code) { throw "$Name did not return an exit code. See $testRoot" }
    $passed = $code -eq 0
    $results.Add([pscustomobject]@{test=$Name; passed=$passed; exitCode=$code})
    Write-Host (('{0}: {1}' -f $Name, $(if ($passed) {'PASS'} else {'FAIL'})))
    if (-not $passed) { throw "$Name failed with exit code $code. See $testRoot" }
}

try {
    if (-not (Test-Path -LiteralPath $app) -or -not (Test-Path -LiteralPath $cli)) { throw 'Application files are missing. Reinstall SculptForge.' }
    $manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'installed-files.json') -Raw | ConvertFrom-Json
    foreach ($entry in $manifest) {
        $path = Join-Path $installRoot $entry.path
        if (-not (Test-Path -LiteralPath $path)) { throw "Missing installed file: $($entry.path)" }
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $entry.sha256) { throw "File checksum mismatch: $($entry.path)" }
    }
    $results.Add([pscustomobject]@{test='installed-file-checksums';passed=$true;exitCode=0})
    Write-Host 'Installed file checksums: PASS'
    Run-Check 'cli-help' $cli @('--help')
    @'
new_sphere subdivisions=3
save_project path=baseline.sfproj
save_obj path=baseline.obj
export_glb path=baseline.glb
open path=baseline.sfproj
save_obj path=reopened.obj
'@ | Set-Content -LiteralPath (Join-Path $testRoot 'roundtrip.sfscript') -Encoding ASCII
    Run-Check 'cli-roundtrip' $cli @('-f', 'roundtrip.sfscript')
    foreach ($file in @('baseline.sfproj','baseline.obj','baseline.glb','reopened.obj')) {
        $saved = Get-Item -LiteralPath (Join-Path $testRoot $file)
        if ($saved.Length -eq 0) { throw "Empty output: $file" }
    }
    if ((Get-FileHash (Join-Path $testRoot 'baseline.obj')).Hash -ne (Get-FileHash (Join-Path $testRoot 'reopened.obj')).Hash) { throw 'The project round trip changed the exported mesh.' }
    if (-not $SkipUI) {
        foreach ($name in @('sculpt_basics','navigation','subtools','viewport_reference','sculpt_refinement','smooth_behavior','stroke_history','cut_topology_mask','surface_cursor','extract_mirror','timeline','studio_tools','studio_pose','replay_modifiers','profile_brushes','gizmo_deformers','gizmo','radial_symmetry','looks')) {
            $script = Join-Path $PSScriptRoot "tests\ui\$name.sfui"
            Run-Check ('ui-' + $name) $app @('--ui-test', ('"' + $script + '"'), '--ui-out', ('"' + (Join-Path $testRoot $name) + '"'))
        }
    }
    $results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $testRoot 'results.json') -Encoding UTF8
    Write-Host "All requested checks passed. Results: $testRoot"
    exit 0
} catch {
    $results.Add([pscustomobject]@{test='failure';passed=$false;message=$_.Exception.Message})
    $results | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $testRoot 'results.json') -Encoding UTF8
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host "Logs: $testRoot"
    exit 1
}
