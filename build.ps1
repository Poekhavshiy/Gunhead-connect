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
    [switch]$DebugMode,
    [switch]$Release,
    [switch]$Test,
    [switch]$Help
)

# Configuration
$BUILD_DIR = "build"
$COMPILE_DB_OUT = "buildfiles"
$QT_PATH = "D:\Qt\6.9.2\mingw_64"
$VULKAN_SDK = "C:\VulkanSDK\1.3.296.0"

# Tool paths
$CMAKE_PATH = "$PWD\cmake_extract\cmake-3.30.3-windows-x86_64\bin\cmake.exe"
$NINJA_PATH = "D:\msys64\mingw64\bin\ninja.exe"
$CTEST_PATH = "D:\msys64\mingw64\bin\ctest.exe"
$GDB_PATH = "D:\msys64\mingw64\bin\gdb.exe"

# Set build type based on parameters
$BUILD_TYPE = if ($Release) { "Release" } else { "Debug" }

# Extra run arguments
$EXTRA_RUN_ARGS = @()
if ($DebugMode) { $EXTRA_RUN_ARGS += "--debug" }

# Setup environment for MSYS2 tools
$env:VULKAN_SDK = $VULKAN_SDK
$env:VK_SDK_PATH = $VULKAN_SDK
$env:CMAKE_PREFIX_PATH = $QT_PATH
$env:PATH = "D:\msys64\mingw64\bin;$env:PATH"

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
    Write-ColorText "  -Release      Build in Release mode (default: Debug)"
    Write-ColorText "  -DebugMode    Run with debug arguments"
    Write-ColorText "  -Test         Run tests after building"
    Write-ColorText "  -Help         Show this help"
    Write-ColorText ""
    Write-ColorText "Examples:" $BLUE
    Write-ColorText "  .\build.ps1 -JustBuild"
    Write-ColorText "  .\build.ps1 -BuildRun -DebugMode"
    Write-ColorText "  .\build.ps1 -Release -BuildRun"
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
        if (Test-Path ".fc_cache") {
            Remove-Item ".fc_cache" -Recurse -Force
            Write-ColorText "FetchContent cache cleaned!" $GREEN
        }
    }

    if ($CompileDb -or $JustBuild -or $BuildRun) {
        Write-ColorText "Configuring with CMake ($BUILD_TYPE)..." $BLUE
        
        # Ensure directories exist
        if (-not (Test-Path $COMPILE_DB_OUT)) {
            New-Item -ItemType Directory -Path $COMPILE_DB_OUT -Force | Out-Null
        }
        
        # Configure with CMake
        & $CMAKE_PATH -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_PREFIX_PATH="$QT_PATH" -B $BUILD_DIR -G "MinGW Makefiles"
        
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
        
        # Fix the ninja files if they have the BUILD_TYPE issue
        $ninjaFiles = Get-ChildItem "$BUILD_DIR" -Recurse -Name "*.ninja"
        foreach ($file in $ninjaFiles) {
            $fullPath = "$BUILD_DIR\$file"
            if (Test-Path $fullPath) {
                $content = Get-Content $fullPath -Raw -ErrorAction SilentlyContinue
                if ($content -and $content -match '\$BUILD_TYPE') {
                    Write-ColorText "Fixing ninja file: $file" $YELLOW
                    $content -replace '\$BUILD_TYPE', $BUILD_TYPE | Set-Content $fullPath
                }
            }
        }
        
        # Build just the main target to avoid CMake compatibility issues
        & $NINJA_PATH -C $BUILD_DIR Gunhead-Connect
        
        if ($LASTEXITCODE -ne 0) {
            Write-ColorText "Main target failed, trying cmake build..." $YELLOW
            & $CMAKE_PATH --build $BUILD_DIR --config $BUILD_TYPE
            
            if ($LASTEXITCODE -ne 0) {
                throw "Build failed"
            }
        }
        
        Write-ColorText "Build completed successfully!" $GREEN
    }

    if ($Test -or $BuildRun) {
        Write-ColorText "Running tests..." $BLUE
        & $CTEST_PATH --test-dir $BUILD_DIR --output-on-failure
        
        if ($LASTEXITCODE -ne 0) {
            Write-ColorText "Some tests failed, but continuing..." $YELLOW
        } else {
            Write-ColorText "All tests passed!" $GREEN
        }
    }

    if ($Run -or $BuildRun) {
        Write-ColorText "Running executable..." $BLUE
        
        $exePath = "$BUILD_DIR\bin\windows\Gunhead-Connect.exe"
        if (-not (Test-Path $exePath)) {
            throw "Executable not found at: $exePath"
        }
        
        if ($DebugMode) {
            Write-ColorText "Starting with GDB debugger: $exePath" $GREEN
            if (Test-Path $GDB_PATH) {
                $gdbArgs = @("--args", $exePath) + $EXTRA_RUN_ARGS
                & $GDB_PATH $gdbArgs
            } else {
                Write-ColorText "GDB not found, running without debugger" $YELLOW
                & $exePath $EXTRA_RUN_ARGS
            }
        } else {
            Write-ColorText "Starting: $exePath" $GREEN
            & $exePath $EXTRA_RUN_ARGS
        }
    }

    Write-ColorText "All operations completed successfully!" $GREEN
}
catch {
    Write-ColorText "Error: $($_.Exception.Message)" $RED
    exit 1
}
