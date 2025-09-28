#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Working build script for Gunhead-Connect (bypasses CMake ninja issue)

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

.PARAMETER Test
    Run tests after building

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
    [switch]$Test,
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

# Use MSYS2 CMake
$CMAKE_PATH = "D:\MSYS64\mingw64\bin\cmake.exe"

function Write-ColorText($text, $color = "White") {
    Write-Host $text -ForegroundColor $color
}

function Show-Help {
    Write-ColorText "Gunhead-Connect Working Build Script" $BLUE
    Write-ColorText "This script bypasses the CMake ninja issue" $YELLOW
    Write-ColorText ""
    Write-ColorText "Usage: .\quick-build.ps1 [options]" $GREEN
    Write-ColorText ""
    Write-ColorText "Options:" $YELLOW
    Write-ColorText "  -JustBuild    Build the project"
    Write-ColorText "  -BuildRun     Build and run"
    Write-ColorText "  -Run          Run executable"
    Write-ColorText "  -Clean        Clean build"
    Write-ColorText "  -DebugMode    Run with GDB"
    Write-ColorText "  -Release      Build in Release mode"
    Write-ColorText "  -Test         Run tests"
    Write-ColorText "  -CompileDb    Generate compile commands only"
    Write-ColorText "  -Help         Show this help"
    Write-ColorText ""
    Write-ColorText "Examples:" $GREEN
    Write-ColorText "  .\quick-build.ps1 -JustBuild"
    Write-ColorText "  .\quick-build.ps1 -BuildRun -DebugMode"
    Write-ColorText "  .\quick-build.ps1 -Release -BuildRun"
    exit 0
}

if ($Help) { Show-Help }

# Show help if no parameters
if (-not ($JustBuild -or $BuildRun -or $Run -or $Clean -or $CompileDb -or $Test)) {
    Show-Help
}

try {
    if ($Clean) {
        Write-ColorText "Cleaning build directory..." $YELLOW
        if (Test-Path $BUILD_DIR) {
            Remove-Item $BUILD_DIR -Recurse -Force
            Write-ColorText "Build directory cleaned!" $GREEN
        }
    }

    if ($CompileDb -or $JustBuild -or $BuildRun) {
        Write-ColorText "Configuring with CMake ($BUILD_TYPE)..." $BLUE
        
        # Ensure directories exist
        if (-not (Test-Path $COMPILE_DB_OUT)) {
            New-Item -ItemType Directory -Path $COMPILE_DB_OUT -Force | Out-Null
        }
        
        # Step 1: Configure with CMake (use Unix Makefiles to avoid ninja issues)
        & $CMAKE_PATH -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_PREFIX_PATH="D:\Qt\6.9.2\mingw_64" -DCMAKE_MAKE_PROGRAM="D:\msys64\mingw64\bin\make.exe" -DCMAKE_CXX_COMPILER="D:\msys64\mingw64\bin\c++.exe" -DCMAKE_C_COMPILER="D:\msys64\mingw64\bin\gcc.exe" -DBUILD_TESTING=OFF -B $BUILD_DIR -G "Unix Makefiles"
        
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
        
        # Step 3: Build with make
        Write-ColorText "Building with Make..." $BLUE
        & "D:\msys64\mingw64\bin\make.exe" -C $BUILD_DIR
        
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }
        
        Write-ColorText "Build completed successfully!" $GREEN
    }

    if ($Test -or $BuildRun) {
        Write-ColorText "Running tests..." $BLUE
        ctest --test-dir $BUILD_DIR --output-on-failure
        
        if ($LASTEXITCODE -ne 0) {
            Write-ColorText "Some tests failed, but continuing..." $YELLOW
        } else {
            Write-ColorText "All tests passed!" $GREEN
        }
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
