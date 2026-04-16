param(
    [int]$MaxIterations = 50,
    [string]$Configuration = "Debug",
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Continue"
$LogFile = "codex_engine_loop.log"

function Log {
    param([string]$msg)
    $time = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $line = "[$time] $msg"
    Write-Host $line
    Add-Content $LogFile $line
}

function Find-Solution {
    $sln = Get-ChildItem -Recurse -Filter *.sln | Select-Object -First 1
    if (!$sln) { throw "No solution file found." }
    return $sln.FullName
}

function Find-MSBuild {

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"

    if (Test-Path $vswhere) {

        $path = & $vswhere -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe

        if ($path) { return $path }
    }

    return "MSBuild.exe"
}

function Run-CodexTask {

    param(
        [string]$Prompt,
        [string]$Title
    )

    Log "Running Codex task: $Title"

    try {

        $result = & codex exec --skip-git-repo-check $Prompt 2>&1
        $result | Tee-Object -FilePath $LogFile -Append

    }
    catch {
        Log "Codex execution error: $_"
    }
}

function Build-Solution {

    param(
        [string]$Solution,
        [string]$MSBuild
    )

    Log "Building solution..."

    $buildLog = "build_iteration.log"

    & $MSBuild $Solution `
        /m `
        /p:Configuration=$Configuration `
        /p:Platform=$Platform `
        /verbosity:minimal `
        > $buildLog 2>&1

    if ($LASTEXITCODE -ne 0) {

        Log "Build FAILED"

        return @{
            Failed = $true
            Log = Get-Content $buildLog -Tail 200
        }
    }

    Log "Build succeeded"

    return @{
        Failed = $false
        Log = ""
    }
}

Clear-Content $LogFile -ErrorAction Ignore

Log "Starting codex_engine_loop"
Log "MaxIterations=$MaxIterations"

$Solution = Find-Solution
$MSBuild = Find-MSBuild

Log "Using solution: $Solution"
Log "Using MSBuild: $MSBuild"

for ($i = 1; $i -le $MaxIterations; $i++) {

    Log "================================="
    Log "Iteration ${i} / ${MaxIterations}"

    $analyzePrompt = @"
Analyze the gameplay movement pipeline in RebelEngine.

Focus on:

Pawn
PlayerController
MovementComponent
movement input flow

Goal:

Identify what is missing to implement a basic CharacterMovementComponent-style locomotion system.

Suggest ONE small improvement or step toward that goal.

Ignore renderer, editor, reflection, asset systems, and physics core.
"@

    Run-CodexTask $analyzePrompt "Iteration ${i}: Analyze movement"

    $implementPrompt = @"
Implement a basic Character Movement Component system for RebelEngine.

The goal is to build the foundation of a CharacterMovementComponent-like system that will later support advanced 3C gameplay.

Focus only on gameplay framework code.

Architecture context:

Current input pipeline:

World
→ PlayerController
→ Pawn (movement input buffer)
→ MovementComponent

Pawn already stores movement intent via ConsumeMovementInput().

Your task is to implement a basic locomotion system that consumes this input and applies movement.

Requirements:

1. Implement velocity-based movement.
2. Add acceleration from movement input.
3. Add friction or damping.
4. Add simple gravity.
5. Apply translation to the actor transform.

Movement model:

velocity += acceleration * deltaTime  
velocity -= friction * deltaTime  
position += velocity * deltaTime

Keep it simple and readable.

Constraints:

- Only modify gameplay movement related code.
- Prefer MovementComponent / TestMovementComponent / Pawn.
- Do NOT modify renderer, editor, asset systems, reflection system, or physics core.
- Avoid large refactors.

Preferred features to introduce:

- Velocity variable
- Acceleration calculation from input
- Max speed clamp
- Basic friction
- Gravity when not grounded
- Simple ground detection placeholder

Important:

Implement the system incrementally.
Do NOT attempt to build a full Unreal-style CMC.
Start with a minimal working locomotion base.

After implementing:
Briefly explain what files were changed and why.
"@

    Run-CodexTask $implementPrompt "Iteration ${i}: Implement improvement"

    $buildResult = Build-Solution $Solution $MSBuild

    if ($buildResult.Failed) {

        $fixPrompt = @"
The engine build failed.

Fix the compile errors.

Errors:
$($buildResult.Log)

Rules:
- only modify relevant code
- avoid touching unrelated engine systems
"@

        Run-CodexTask $fixPrompt "Iteration ${i}: Fix build"

        Build-Solution $Solution $MSBuild | Out-Null
    }

    Start-Sleep -Seconds 10
}

Log "Overnight loop finished"