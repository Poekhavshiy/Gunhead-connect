#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Simple build script for Gunhead-Connect

.PARAMETER BuildRun
    Build and run
.PARAMETER JustBuild
    Just build
.PARAMETER Run
    Just run
.PARAMETER Clean
    Clean build
.PARAMETER DebugMode
    Run with debugger
.PARAMETER Help
    Show help
#>

param(
    [switch]$BuildRun,
    [switch]$JustBuild,
    [switch]$Run,
    [switch]$Clean,
    [switch]$DebugMode,
    [switch]$Help
)

# Paths
$CMAKE = "D:\msys64\mingw64\bin\cmake.exe"
$NINJA = "D:\msys64\mingw64\bin\ninja.exe"
$GDB = "D:\msys64\mingw64\bin\gdb.exe"

function Write-Status($msg, $color = "White") {
    Write-Host $msg -ForegroundColor $color
}

function Show-Help {
    Write-Status "Gunhead-Connect Build Script" "Blue"
    Write-Status "Usage: .\build.ps1 [options]" "Green"
    Write-Status ""
    Write-Status "Options:" "Yellow"
    Write-Status "  -JustBuild    Build the project"
    Write-Status "  -BuildRun     Build and run"
    Write-Status "  -Run          Run executable"
    Write-Status "  -Clean        Clean build"
    Write-Status "  -DebugMode    Run with debugger"
    Write-Status "  -Help         Show this help"
    exit 0
}

if ($Help) { Show-Help }
if (-not ($JustBuild -or $BuildRun -or $Run -or $Clean)) { Show-Help }

# Setup environment
$env:VULKAN_SDK = "C:\VulkanSDK\1.3.296.0"
$env:VK_SDK_PATH = "C:\VulkanSDK\1.3.296.0"
$env:CMAKE_PREFIX_PATH = "D:\Qt\6.9.2\mingw_64"
$env:PATH = "D:\msys64\mingw64\bin;$env:PATH"

try {
    if ($Clean) {
        Write-Status "Cleaning..." "Yellow"
        if (Test-Path "build") {
            Remove-Item "build" -Recurse -Force
        }
        Write-Status "Clean complete!" "Green"
        return
    }

    if ($JustBuild -or $BuildRun) {
        Write-Status "Configuring..." "Blue"
        
        # Use a more compatible CMake configuration
        & $CMAKE -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="D:\Qt\6.9.2\mingw_64"
        
        if ($LASTEXITCODE -ne 0) {
            throw "Configuration failed"
        }

        Write-Status "Building..." "Blue"
        
        # Build just the main target, skip tests for now
        & $NINJA -C build Gunhead-Connect
        
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed"
        }
        
        Write-Status "Build successful!" "Green"
    }

    if ($Run -or $BuildRun) {
        $exe = "build\bin\windows\Gunhead-Connect.exe"
        
        if (-not (Test-Path $exe)) {
            throw "Executable not found: $exe"
        }
        
        Write-Status "Running..." "Blue"
        
        if ($DebugMode -and (Test-Path $GDB)) {
            Write-Status "Starting with debugger..." "Cyan"
            & $GDB --args $exe
        } else {
            Write-Status "Starting application..." "Green"
            & $exe
        }
    }
    
    Write-Status "Done!" "Green"
}
catch {
    Write-Status "Error: $($_.Exception.Message)" "Red"
    exit 1
}
