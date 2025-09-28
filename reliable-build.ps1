#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Reliable build script for Gunhead-Connect (bypasses CMake issues)

.PARAMETER BuildRun
    Build and run the executable

.PARAMETER JustBuild
    Only build the project

.PARAMETER Run
    Run the built executable

.PARAMETER Clean
    Clean build directory

.PARAMETER DebugMode
    Run with GDB debugger

.PARAMETER Release
    Build in Release mode (default is Debug)

.PARAMETER CompileDb
    Generate compile_commands.json only

.PARAMETER Help
    Show help
#>

[CmdletBinding()]
param(
    [switch]$BuildRun,
    [switch]$JustBuild, 
    [switch]$Run,
    [switch]$Clean,
    [switch]$DebugMode,
    [switch]$Release,
    [switch]$CompileDb,
    [switch]$Help
)

# Colors
$GREEN = "Green"
$BLUE = "Blue" 
$RED = "Red"
$YELLOW = "Yellow"

# Build configuration
$BUILD_TYPE = if ($Release) { "Release" } else { "Debug" }
$BUILD_DIR = "build"
$COMPILE_DB_OUT = "buildfiles"

function Write-ColorText($text, $color = "White") {
    Write-Host $text -ForegroundColor $color
}

function Show-Help {
    Write-ColorText "Gunhead-Connect Reliable Build Script" $BLUE
    Write-ColorText "This script builds only the main application (bypasses test issues)" $YELLOW
    Write-ColorText ""
    Write-ColorText "Usage: .\reliable-build.ps1 [options]" $GREEN
    Write-ColorText ""
    Write-ColorText "Options:" $YELLOW
    Write-ColorText "  -JustBuild    Build the project"
    Write-ColorText "  -BuildRun     Build and run"
    Write-ColorText "  -Run          Run executable"
    Write-ColorText "  -Clean        Clean build"
    Write-ColorText "  -DebugMode    Run with GDB"
    Write-ColorText "  -Release      Build in Release mode"
    Write-ColorText "  -CompileDb    Generate compile commands only"
    Write-ColorText "  -Help         Show this help"
    Write-ColorText ""
    Write-ColorText "Examples:" $GREEN
    Write-ColorText "  .\reliable-build.ps1 -JustBuild"
    Write-ColorText "  .\reliable-build.ps1 -BuildRun -DebugMode"
    Write-ColorText "  .\reliable-build.ps1 -Release -BuildRun"
    exit 0
}

if ($Help) { Show-Help }

# Show help if no parameters
if (-not ($JustBuild -or $BuildRun -or $Run -or $Clean -or $CompileDb)) {
    Show-Help
}

try {
    if ($Clean) {
        Write-ColorText "Cleaning build directory..." $YELLOW
        if (Test-Path $BUILD_DIR) {
            Remove-Item $BUILD_DIR -Recurse -Force -ErrorAction SilentlyContinue
            Write-ColorText "Build directory cleaned!" $GREEN
        }
    }

    if ($CompileDb -or $JustBuild -or $BuildRun) {
        Write-ColorText "Configuring with CMake ($BUILD_TYPE)..." $BLUE
        
        # Ensure directories exist
        if (-not (Test-Path $COMPILE_DB_OUT)) {
            New-Item -ItemType Directory -Path $COMPILE_DB_OUT -Force | Out-Null
        }
        
        # Configure with CMake (disable tests to avoid cmake_transform_depfile issue)
        cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_PREFIX_PATH="D:\Qt\6.9.2\mingw_64" -DBUILD_TESTING=OFF -B $BUILD_DIR -G Ninja
        
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed"
        }
        
        # Copy compile commands
        if (Test-Path "$BUILD_DIR\compile_commands.json") {
            Copy-Item "$BUILD_DIR\compile_commands.json" "$COMPILE_DB_OUT\compile_commands.json" -Force
            Write-ColorText "Compile commands exported to: $COMPILE_DB_OUT\compile_commands.json" $GREEN
        }
    }

    if ($JustBuild -or $BuildRun) {
        Write-ColorText "Building project ($BUILD_TYPE)..." $BLUE
        
        # Fix the problematic ninja rule (CMake ninja generation bug workaround)
        Write-ColorText "Fixing ninja rules..." $YELLOW
        $rulesPath = "$BUILD_DIR\CMakeFiles\rules.ninja"
        $buildPath = "$BUILD_DIR\build.ninja"
        
        if (Test-Path $rulesPath) {
            (Get-Content $rulesPath) -replace '\$BUILD_TYPE', $BUILD_TYPE | Set-Content $rulesPath
        }
        
        if (Test-Path $buildPath) {
            (Get-Content $buildPath) -replace '\$BUILD_TYPE', $BUILD_TYPE | Set-Content $buildPath
        }
        
        # Build only the main target to avoid test issues
        Write-ColorText "Building main application with Ninja..." $BLUE
        ninja -C $BUILD_DIR Gunhead-Connect
        
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }
        
        Write-ColorText "Build completed successfully!" $GREEN
    }

    if ($Run -or $BuildRun) {
        Write-ColorText "Running executable..." $BLUE
        
        # Find the executable
        $possiblePaths = @(
            "$BUILD_DIR\bin\windows\Gunhead-Connect.exe",
            "$BUILD_DIR\$BUILD_TYPE\Gunhead-Connect.exe", 
            "$BUILD_DIR\Debug\Gunhead-Connect.exe",
            "$BUILD_DIR\Release\Gunhead-Connect.exe",
            "$BUILD_DIR\Gunhead-Connect.exe"
        )
        
        $exePath = $null
        foreach ($path in $possiblePaths) {
            if (Test-Path $path) {
                $exePath = $path
                break
            }
        }
        
        if (-not $exePath) {
            Write-ColorText "Searching for executable in build directory..." $YELLOW
            $foundExes = Get-ChildItem -Path $BUILD_DIR -Recurse -Name "Gunhead-Connect.exe" -ErrorAction SilentlyContinue
            if ($foundExes) {
                $exePath = Join-Path $BUILD_DIR $foundExes[0]
                Write-ColorText "Found executable at: $exePath" $GREEN
            } else {
                throw "Executable not found in build directory"
            }
        }
        
        if ($DebugMode) {
            Write-ColorText "Starting with GDB debugger: $exePath" $GREEN
            $gdbPath = "D:\msys64\mingw64\bin\gdb.exe"
            if (Test-Path $gdbPath) {
                & $gdbPath "--args" $exePath
            } else {
                Write-ColorText "GDB not found, running normally" $YELLOW
                & $exePath
            }
        } else {
            Write-ColorText "Starting: $exePath" $GREEN
            & $exePath
        }
    }

    Write-ColorText "All operations completed successfully!" $GREEN
}
catch {
    Write-ColorText "Error: $($_.Exception.Message)" $RED
    exit 1
}
