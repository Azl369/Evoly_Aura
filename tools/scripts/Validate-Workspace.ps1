[CmdletBinding()]
param(
    [string]$Root
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Join-Path $PSScriptRoot '..\..'
}
$workspaceRoot = (Resolve-Path -LiteralPath $Root).Path
$requiredFiles = @(
    'README.md',
    'AGENTS.md',
    '.gitignore',
    '.gitattributes',
    '.editorconfig',
    'docs/product/PRODUCT_BRIEF.md',
    'docs/development/WORKSPACE.md',
    'docs/development/GIT_WORKFLOW.md',
    'docs/development/DELIVERY_STATUS.md',
    'docs/architecture/ARCHITECTURE.md',
    'docs/architecture/RENDERING.md',
    'docs/architecture/adr/0001-native-resident-core.md',
    'docs/performance/PERFORMANCE_BUDGET.md',
    'docs/plans/ROADMAP.md',
    'docs/plans/P0_NATIVE_PROBE.md',
    'docs/testing/VALIDATION.md',
    'config/examples/layout.example.json',
    'benchmarks/scenarios/b0.json',
    '.github/workflows/docs.yml'
)
$requiredDirectories = @(
    'apps/host', 'apps/editor',
    'src/core', 'src/platform/windows', 'src/render', 'src/widgets', 'src/persistence',
    'include/aura', 'assets/icons', 'assets/themes',
    'tests/unit', 'tests/integration', 'tests/performance',
    'cmake', 'packaging/windows', 'benchmarks/results'
)

foreach ($relativePath in $requiredFiles) {
    if (-not (Test-Path -LiteralPath (Join-Path $workspaceRoot $relativePath) -PathType Leaf)) {
        throw "Missing required file: $relativePath"
    }
}
foreach ($relativePath in $requiredDirectories) {
    if (-not (Test-Path -LiteralPath (Join-Path $workspaceRoot $relativePath) -PathType Container)) {
        throw "Missing required directory: $relativePath"
    }
}

$scanRoots = @('docs', 'apps', 'src', 'assets', 'config', 'benchmarks', 'tools', '.github')
$scannedFiles = @(
    Get-ChildItem -LiteralPath $workspaceRoot -File -Filter '*.md'
    foreach ($relativePath in $scanRoots) {
        Get-ChildItem -LiteralPath (Join-Path $workspaceRoot $relativePath) -File -Recurse -Force |
            Where-Object { $_.Extension -in @('.md', '.json') }
    }
)
$jsonCount = 0
$linkCount = 0
foreach ($file in $scannedFiles) {
    $content = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    if ($file.Extension -eq '.json') {
        $null = ConvertFrom-Json -InputObject $content -ErrorAction Stop
        $jsonCount++
        continue
    }
    foreach ($match in [regex]::Matches($content, '\[[^\]]*\]\((?<target>[^)]+)\)')) {
        $target = $match.Groups['target'].Value.Trim()
        if ($target -match '^(https?://|mailto:|#)') { continue }
        $target = [Uri]::UnescapeDataString(($target -split '#', 2)[0].Trim('<', '>'))
        if ([string]::IsNullOrWhiteSpace($target)) { continue }
        $resolvedTarget = Join-Path $file.DirectoryName $target
        if (-not (Test-Path -LiteralPath $resolvedTarget)) {
            throw "Broken local link in $($file.FullName): $target"
        }
        $linkCount++
    }
}
Write-Output "Workspace validation passed: $($requiredFiles.Count) required files, $($requiredDirectories.Count) directories, $jsonCount JSON files, $linkCount local links."
Write-Output 'Native build and runtime benchmarks are not implemented at this planning stage.'
