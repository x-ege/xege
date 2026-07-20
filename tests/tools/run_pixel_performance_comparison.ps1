[CmdletBinding()]
param(
    [ValidateRange(1, 100)]
    [int]$Runs = 5,

    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",

    [string]$GdiBuildDirectory,
    [string]$OpenGlBuildDirectory,
    [string]$OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path -Path $PSScriptRoot -ChildPath "../.."))
$isWindowsHost = [Environment]::OSVersion.Platform -eq [PlatformID]::Win32NT
if (-not $isWindowsHost) {
    throw "The dual-backend GDI/OpenGL comparison runner is Windows-only."
}

if ([string]::IsNullOrWhiteSpace($GdiBuildDirectory)) {
    $GdiBuildDirectory = Join-Path -Path $repositoryRoot -ChildPath "build/gdi"
}
if ([string]::IsNullOrWhiteSpace($OpenGlBuildDirectory)) {
    $OpenGlBuildDirectory = Join-Path -Path $repositoryRoot -ChildPath "build/opengl"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $OutputDirectory = Join-Path -Path $repositoryRoot -ChildPath (
        "build/pixel-performance-results/{0}" -f $timestamp)
}

$GdiBuildDirectory = [System.IO.Path]::GetFullPath($GdiBuildDirectory)
$OpenGlBuildDirectory = [System.IO.Path]::GetFullPath($OpenGlBuildDirectory)
$OutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)

function Get-BenchmarkExecutable {
    param([Parameter(Mandatory = $true)][string]$BuildDirectory)

    $executableName = "pixel_access_performance_test"
    if ($isWindowsHost) {
        $executableName += ".exe"
    }

    $candidates = @(
        (Join-Path -Path $BuildDirectory -ChildPath (
            "bin/{0}/{1}" -f $Configuration, $executableName)),
        (Join-Path -Path $BuildDirectory -ChildPath (
            "bin/{0}" -f $executableName))
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    throw "Benchmark executable not found under '$BuildDirectory'. Build the '$Configuration' pixel_access_performance_test target first."
}

function ConvertTo-InvariantDouble {
    param([Parameter(Mandatory = $true)][string]$Value)

    return [double]::Parse(
        $Value,
        [System.Globalization.NumberStyles]::Float,
        [System.Globalization.CultureInfo]::InvariantCulture)
}

function Get-Median {
    param([Parameter(Mandatory = $true)][double[]]$Values)

    $sorted = @($Values | Sort-Object)
    $middle = [math]::Floor($sorted.Count / 2)
    if (($sorted.Count % 2) -eq 1) {
        return [double]$sorted[$middle]
    }
    return ([double]$sorted[$middle - 1] + [double]$sorted[$middle]) / 2.0
}

function Get-NearestRankPercentile {
    param(
        [Parameter(Mandatory = $true)][double[]]$Values,
        [Parameter(Mandatory = $true)][double]$Fraction
    )

    $sorted = @($Values | Sort-Object)
    $index = [math]::Ceiling($Fraction * $sorted.Count) - 1
    $index = [math]::Max(0, [math]::Min($index, $sorted.Count - 1))
    return [double]$sorted[$index]
}

function Invoke-BenchmarkRun {
    param(
        [Parameter(Mandatory = $true)][string]$Backend,
        [Parameter(Mandatory = $true)][string]$Executable,
        [Parameter(Mandatory = $true)][int]$Run
    )

    $previousBackend = [Environment]::GetEnvironmentVariable(
        "EGE_TEST_OPENGL", "Process")
    try {
        if ($Backend -eq "opengl") {
            [Environment]::SetEnvironmentVariable(
                "EGE_TEST_OPENGL", "1", "Process")
        } else {
            [Environment]::SetEnvironmentVariable(
                "EGE_TEST_OPENGL", $null, "Process")
        }

        Write-Host ("Run {0}/{1}: {2}" -f $Run, $Runs, $Backend)
        $outputLines = @(& $Executable 2>&1 | ForEach-Object { $_.ToString() })
        $exitCode = $LASTEXITCODE
    } finally {
        [Environment]::SetEnvironmentVariable(
            "EGE_TEST_OPENGL", $previousBackend, "Process")
    }

    $logPath = Join-Path -Path $OutputDirectory -ChildPath (
        "{0}-run-{1:D2}.log" -f $Backend, $Run)
    $outputLines | Set-Content -LiteralPath $logPath -Encoding UTF8

    if ($exitCode -ne 0) {
        throw "The $Backend benchmark failed with exit code $exitCode. See '$logPath'."
    }

    $rows = @()
    foreach ($line in $outputLines) {
        if (-not $line.StartsWith("PIXEL_PERF ")) {
            continue
        }

        $fields = @{}
        foreach ($token in ($line -split " ") | Select-Object -Skip 1) {
            $pair = $token -split "=", 2
            if ($pair.Count -eq 2) {
                $fields[$pair[0]] = $pair[1]
            }
        }

        $requiredFields = @(
            "backend", "metric", "samples", "operations_per_sample",
            "median_ms", "mean_ms", "stddev_ms", "p95_ms", "min_ms",
            "max_ms", "ns_per_op")
        foreach ($field in $requiredFields) {
            if (-not $fields.ContainsKey($field)) {
                throw "Missing '$field' in benchmark output: $line"
            }
        }
        if ($fields["backend"] -ne $Backend) {
            throw "Expected backend '$Backend', but the executable reported '$($fields["backend"])'."
        }

        $rows += [pscustomobject][ordered]@{
            run                   = $Run
            backend               = $fields["backend"]
            metric                = $fields["metric"]
            samples               = [int]$fields["samples"]
            operations_per_sample = [int]$fields["operations_per_sample"]
            median_ms             = ConvertTo-InvariantDouble $fields["median_ms"]
            mean_ms               = ConvertTo-InvariantDouble $fields["mean_ms"]
            stddev_ms             = ConvertTo-InvariantDouble $fields["stddev_ms"]
            p95_ms                = ConvertTo-InvariantDouble $fields["p95_ms"]
            min_ms                = ConvertTo-InvariantDouble $fields["min_ms"]
            max_ms                = ConvertTo-InvariantDouble $fields["max_ms"]
            ns_per_op             = ConvertTo-InvariantDouble $fields["ns_per_op"]
        }
    }

    if ($rows.Count -eq 0) {
        throw "No PIXEL_PERF rows were found in '$logPath'."
    }
    return $rows
}

$gdiExecutable = Get-BenchmarkExecutable -BuildDirectory $GdiBuildDirectory
$openGlExecutable = Get-BenchmarkExecutable -BuildDirectory $OpenGlBuildDirectory
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null

$rawRows = @()
for ($run = 1; $run -le $Runs; ++$run) {
    # Alternate the order so startup, temperature, and background-load effects do
    # not consistently favor one backend.
    $order = if (($run % 2) -eq 1) {
        @("gdi", "opengl")
    } else {
        @("opengl", "gdi")
    }

    foreach ($backend in $order) {
        $executable = if ($backend -eq "gdi") {
            $gdiExecutable
        } else {
            $openGlExecutable
        }
        $rawRows += Invoke-BenchmarkRun -Backend $backend `
            -Executable $executable -Run $run
    }
}

$rawCsvPath = Join-Path -Path $OutputDirectory -ChildPath "raw.csv"
$rawRows | Sort-Object run, backend, metric |
    Export-Csv -LiteralPath $rawCsvPath -NoTypeInformation -Encoding UTF8

$backendSummaryRows = @()
foreach ($group in $rawRows | Group-Object backend, metric) {
    $first = $group.Group[0]
    [double[]]$medians = @($group.Group | ForEach-Object { $_.median_ms })
    $mean = ($medians | Measure-Object -Average).Average
    $sumOfSquares = 0.0
    foreach ($value in $medians) {
        $difference = $value - $mean
        $sumOfSquares += $difference * $difference
    }
    $standardDeviation = if ($medians.Count -gt 1) {
        [math]::Sqrt($sumOfSquares / ($medians.Count - 1))
    } else {
        0.0
    }

    $backendSummaryRows += [pscustomobject][ordered]@{
        backend                    = $first.backend
        metric                     = $first.metric
        process_runs               = $medians.Count
        samples_per_process        = $first.samples
        operations_per_sample      = $first.operations_per_sample
        median_of_process_medians_ms = Get-Median $medians
        mean_of_process_medians_ms = $mean
        stddev_of_process_medians_ms = $standardDeviation
        p95_of_process_medians_ms  = Get-NearestRankPercentile $medians 0.95
        min_process_median_ms      = ($medians | Measure-Object -Minimum).Minimum
        max_process_median_ms      = ($medians | Measure-Object -Maximum).Maximum
    }
}

$summaryCsvPath = Join-Path -Path $OutputDirectory -ChildPath "backend-summary.csv"
$backendSummaryRows | Sort-Object metric, backend |
    Export-Csv -LiteralPath $summaryCsvPath -NoTypeInformation -Encoding UTF8

$comparisonRows = @()
foreach ($metricGroup in $backendSummaryRows | Group-Object metric) {
    $gdi = $metricGroup.Group | Where-Object backend -eq "gdi"
    $openGl = $metricGroup.Group | Where-Object backend -eq "opengl"
    if ($null -eq $gdi -or $null -eq $openGl) {
        throw "Metric '$($metricGroup.Name)' is missing a backend result."
    }

    $gdiMedian = [double]$gdi.median_of_process_medians_ms
    $openGlMedian = [double]$openGl.median_of_process_medians_ms
    $ratio = if ($gdiMedian -gt 0.0) { $openGlMedian / $gdiMedian } else { 0.0 }
    $fasterBackend = if ($openGlMedian -lt $gdiMedian) { "opengl" } else { "gdi" }
    $speedFactor = if ([math]::Min($gdiMedian, $openGlMedian) -gt 0.0) {
        [math]::Max($gdiMedian, $openGlMedian) /
            [math]::Min($gdiMedian, $openGlMedian)
    } else {
        0.0
    }

    $comparisonRows += [pscustomobject][ordered]@{
        metric                = $metricGroup.Name
        operations_per_sample = $gdi.operations_per_sample
        process_runs          = $Runs
        samples_per_process   = $gdi.samples_per_process
        gdi_median_ms         = $gdiMedian
        opengl_median_ms      = $openGlMedian
        opengl_over_gdi       = $ratio
        faster_backend        = $fasterBackend
        speed_factor          = $speedFactor
    }
}

$comparisonCsvPath = Join-Path -Path $OutputDirectory -ChildPath "comparison.csv"
$comparisonRows | Sort-Object metric |
    Export-Csv -LiteralPath $comparisonCsvPath -NoTypeInformation -Encoding UTF8

$gitRevision = (& git -C $repositoryRoot rev-parse HEAD).Trim()
$gitBranch = (& git -C $repositoryRoot branch --show-current).Trim()
& git -C $repositoryRoot diff --quiet --ignore-submodules --
$trackedDirty = $LASTEXITCODE -ne 0
& git -C $repositoryRoot diff --cached --quiet --ignore-submodules --
$indexDirty = $LASTEXITCODE -ne 0
$untracked = @(& git -C $repositoryRoot ls-files --others --exclude-standard)

$processor = Get-CimInstance -ClassName Win32_Processor |
    Select-Object -First 1 Name, NumberOfCores, NumberOfLogicalProcessors
$videoControllers = @(Get-CimInstance -ClassName Win32_VideoController |
    Select-Object Name, DriverVersion)
$operatingSystem = Get-CimInstance -ClassName Win32_OperatingSystem |
    Select-Object Caption, Version, BuildNumber

$environment = [pscustomobject][ordered]@{
    recorded_at_utc    = (Get-Date).ToUniversalTime().ToString("o")
    repository_root    = $repositoryRoot
    git_revision       = $gitRevision
    git_branch         = $gitBranch
    worktree_dirty     = ($trackedDirty -or $indexDirty -or $untracked.Count -gt 0)
    configuration      = $Configuration
    process_runs       = $Runs
    samples_per_metric = $rawRows[0].samples
    gdi_executable     = $gdiExecutable
    opengl_executable  = $openGlExecutable
    operating_system   = $operatingSystem
    processor          = $processor
    video_controllers  = $videoControllers
}
$environmentPath = Join-Path -Path $OutputDirectory -ChildPath "environment.json"
$environment | ConvertTo-Json -Depth 5 |
    Set-Content -LiteralPath $environmentPath -Encoding UTF8

Write-Host ""
Write-Host "Pixel performance comparison complete:"
Write-Host ("  Raw measurements: {0}" -f $rawCsvPath)
Write-Host ("  Backend summary: {0}" -f $summaryCsvPath)
Write-Host ("  Comparison:      {0}" -f $comparisonCsvPath)
Write-Host ("  Environment:     {0}" -f $environmentPath)
Write-Host ""
foreach ($row in $comparisonRows | Sort-Object metric) {
    Write-Host ("PIXEL_PERF_COMPARISON metric={0} gdi_ms={1:F6} opengl_ms={2:F6} opengl_over_gdi={3:F3} faster={4} factor={5:F2}" -f `
        $row.metric, $row.gdi_median_ms, $row.opengl_median_ms,
        $row.opengl_over_gdi, $row.faster_backend, $row.speed_factor)
}
