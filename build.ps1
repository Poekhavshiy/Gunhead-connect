#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Build script for Gunhead-Connect Qt/CMake project

.DESCRIPTION
    Simplified PowerShell script to build, test, and run the Gunhead-Connect project

.PARAMETER BuildRun
    Build and run the executable

.PARAMETER JustBuild
    Only build the project

.PARAMETER Run
    Run the built executable

.PARAMETER Clean
    Clean build directory

.PARAMETER CompileDb
    Generate compile_commands.json

.PARAMETER Help
    Show help

.EXAMPLE
    .\build.ps1 -JustBuild
    .\build.ps1 -BuildRun
#>

[CmdletBinding()]
param(
    [switch]$BuildRun,
    [switch]$JustBuild, 
    [switch]$Run,
    [switch]$Clean,
    [switch]$CompileDb,
    [switch]$Help
)

# Configuration
$BUILD_DIR = "build"
$COMPILE_DB_OUT = "buildfiles"
$QT_PATH = "D:\Qt\6.9.2\mingw_64"
$VULKAN_SDK = "C:\VulkanSDK\1.3.296.0"

# Colors
$GREEN = "Green"
$BLUE = "Blue"
$RED = "Red"
$YELLOW = "Yellow"

# Setup environment
$env:VULKAN_SDK = $VULKAN_SDK
$env:VK_SDK_PATH = $VULKAN_SDK
$env:CMAKE_PREFIX_PATH = $QT_PATH

function Write-ColorText($text, $color = "White") {
    Write-Host $text -ForegroundColor $color
}

function Show-Help {
    Write-ColorText "Gunhead-Connect Build Script" $BLUE
    Write-ColorText "Usage: .\build.ps1 [options]" $GREEN
    Write-ColorText ""
    Write-ColorText "Options:" $YELLOW
    Write-ColorText "  -JustBuild    Build the project"
    Write-ColorText "  -BuildRun     Build and run"
    Write-ColorText "  -Run          Run executable"
    Write-ColorText "  -Clean        Clean build"
    Write-ColorText "  -CompileDb    Generate compile commands"
    Write-ColorText "  -Help         Show this help"
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
            Remove-Item $BUILD_DIR -Recurse -Force
            Write-ColorText "Build directory cleaned!" $GREEN
        }
    }

    if ($CompileDb -or $JustBuild -or $BuildRun) {
        Write-ColorText "Configuring with CMake..." $BLUE
        
        # Ensure directories exist
        if (-not (Test-Path $COMPILE_DB_OUT)) {
            New-Item -ItemType Directory -Path $COMPILE_DB_OUT -Force | Out-Null
        }
        
        # Configure with CMake
        cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="$QT_PATH" -B $BUILD_DIR -G Ninja
        
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
        Write-ColorText "Building project..." $BLUE
        cmake --build $BUILD_DIR
        
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }
        
        Write-ColorText "Build completed successfully!" $GREEN
    }

    if ($Run -or $BuildRun) {
        Write-ColorText "Running executable..." $BLUE
        
        $exePath = "$BUILD_DIR\bin\windows\Gunhead-Connect.exe"
        if (-not (Test-Path $exePath)) {
            throw "Executable not found at: $exePath"
        }
        
        Write-ColorText "Starting: $exePath" $GREEN
        & $exePath
    }

    Write-ColorText "All operations completed successfully!" $GREEN
}
catch {
    Write-ColorText "Error: $($_.Exception.Message)" $RED
    exit 1
}
