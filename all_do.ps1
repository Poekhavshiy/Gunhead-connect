#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Enhanced build orchestrator for Qt/CMake projects

.DESCRIPTION
    PowerShell script to build, test, and run the Gunhead-Connect Qt/CMake project

.PARAMETER Release
    Build in release mode

.PARAMETER BuildRun
    Build, Test and then run the executable

.PARAMETER JustBuild
     }
    cat# ====== Test Section ======
if ($DO_TEST) {
    try {
        Write-Section "Running tests"
        & $CTEST_PATH --test-dir $BUILD_DIR --output-on-failure
        
        if ($LASTEXITCODE -ne 0) {
            throw "Tests failed with exit code $LASTEXITCODE"
        }
        
        Write-Log "Tests completed successfully!" $GREEN
    }
    catch {
        Write-Error-And-Exit "Running tests: $_"
    }
}ite-Error-And-Exit "Running tests: $_"
    }nly build the project

.PARAMETER Run
    Run the built executable through gdb

.PARAMETER DebugMode
    Enable debug mode

.PARAMETER Console
    Enable debugging to console mode

.PARAMETER Test
    Run the unit tests

.PARAMETER Clean
    Delete build/ directory only

.PARAMETER Wipe
    Delete build and compile commands, keep CMake-related files

.PARAMETER NukeAll
    Full nuke (build/, .sln, .vcxproj, CMakeLists.txt.user)

.PARAMETER CompileDb
    Only regenerate compile_commands.json into buildfiles/

.PARAMETER Help
    Show this help message

.EXAMPLE
    .\all_do.ps1 -Release
    .\all_do.ps1 -BuildRun
    .\all_do.ps1 -JustBuild
#>

[CmdletBinding()]
param(
    [switch]$Release,
    [switch]$BuildRun,
    [switch]$JustBuild,
    [switch]$Run,
    [switch]$DebugMode,
    [switch]$Console,
    [switch]$Test,
    [switch]$Clean,
    [switch]$Wipe,
    [switch]$NukeAll,
    [switch]$CompileDb,
    [switch]$Help
)

# ====== Configuration ======
$BUILD_DIR = "build"
$BUILD_TYPE = "Debug"
$BUILD_PRESET_WINDOWS = "windows-dynamic"
$BUILD_PRESET_LINUX = "linux-dynamic"
$LOG_DIR = "logs"
$LOG_FILE = "$LOG_DIR\build.log"
$COMPILE_DB_OUT = "buildfiles"

# MSYS2 paths
$MSYS2_ROOT = "D:\msys64"
$MINGW64_BIN = "$MSYS2_ROOT\mingw64\bin"
$CMAKE_PATH = "$MINGW64_BIN\cmake.exe"
$NINJA_PATH = "$MINGW64_BIN\ninja.exe"
$CTEST_PATH = "$MINGW64_BIN\ctest.exe"
$GDB_PATH = "$MINGW64_BIN\gdb.exe"

# Setup environment for MSYS2 tools
$env:PATH = "$MINGW64_BIN;$env:PATH"
$env:CMAKE_MAKE_PROGRAM = $NINJA_PATH
$env:CC = "$MINGW64_BIN\gcc.exe"
$env:CXX = "$MINGW64_BIN\g++.exe"
$env:CMAKE_C_COMPILER = "$MINGW64_BIN\gcc.exe"
$env:CMAKE_CXX_COMPILER = "$MINGW64_BIN\g++.exe"
$env:VULKAN_SDK = "$MSYS2_ROOT\mingw64"
$env:VK_SDK_PATH = "$MSYS2_ROOT\mingw64"
$env:CMAKE_PREFIX_PATH = "D:\Qt\6.9.2\mingw_64"

# ====== Color output ======
$RED = "Red"
$GREEN = "Green"
$BLUE = "Blue"
$YELLOW = "Yellow"

# ====== Initialize log ======
if (-not (Test-Path $LOG_DIR)) {
    New-Item -ItemType Directory -Path $LOG_DIR -Force | Out-Null
}
if (-not (Test-Path $COMPILE_DB_OUT)) {
    New-Item -ItemType Directory -Path $COMPILE_DB_OUT -Force | Out-Null
}
if (-not (Test-Path "data")) {
    New-Item -ItemType Directory -Path "data" -Force | Out-Null
}

# Timestamp function
function Get-Timestamp {
    return "[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')]"
}

# Logging function
function Write-Log {
    param([string]$Message, [string]$Color = "White")
    $timestampedMessage = "$(Get-Timestamp) $Message"
    Write-Host $timestampedMessage -ForegroundColor $Color
    Add-Content -Path $LOG_FILE -Value $timestampedMessage
}

# Error handling function
function Write-Error-And-Exit {
    param([string]$Step)
    Write-Log "[ERROR] Step failed: $Step" $RED
    # Play system beep sounds
    [System.Console]::Beep(800, 200)
    Start-Sleep -Milliseconds 300
    [System.Console]::Beep(800, 200)
    Start-Sleep -Milliseconds 300
    [System.Console]::Beep(800, 200)
    exit 1
}

# Clear terminal
Clear-Host

# ====== Verify MSYS2 tools ======
$missingTools = @()
if (-not (Test-Path $CMAKE_PATH)) { $missingTools += "CMake" }
if (-not (Test-Path $NINJA_PATH)) { $missingTools += "Ninja" }
if (-not (Test-Path $CTEST_PATH)) { $missingTools += "CTest" }
if (-not (Test-Path $GDB_PATH)) { $missingTools += "GDB" }

if ($missingTools.Count -gt 0) {
    Write-Host "ERROR: Missing required tools:" -ForegroundColor Red
    foreach ($tool in $missingTools) {
        Write-Host "  - $tool" -ForegroundColor Red
    }
    Write-Host "`nPlease install missing tools using:" -ForegroundColor Yellow
    Write-Host "  pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-gdb" -ForegroundColor Yellow
    exit 1
}

# ====== Default flags ======
$DO_NUKE_ALL = $false
$DO_BUILD = $false
$DO_RUN = $false
$DO_CLEAN = $false
$DO_WIPE = $false
$DO_COMPILE_DB = $false
$DO_TEST = $false
$EXTRA_RUN_ARGS = @()

# ====== Parse parameters ======
if ($Release) {
    $BUILD_TYPE = "Release"
    $DO_NUKE_ALL = $true
    $DO_BUILD = $true
    $DO_RUN = $true
}

if ($NukeAll) { $DO_NUKE_ALL = $true }
if ($Wipe) { $DO_WIPE = $true }
if ($Clean) { $DO_CLEAN = $true }
if ($BuildRun) { $DO_BUILD = $true; $DO_TEST = $true; $DO_RUN = $true }
if ($JustBuild) { $DO_BUILD = $true }
if ($Run) { $DO_RUN = $true }
if ($Test) { $DO_TEST = $true }
if ($CompileDb) { $DO_COMPILE_DB = $true }

if ($DebugMode) { $EXTRA_RUN_ARGS += "--debug" }
if ($Console) { $EXTRA_RUN_ARGS += "--console" }

# ====== Documentation ======
function Show-Help {
    Get-Help $PSCommandPath -Full
    exit 0
}

if ($Help -or ($PSBoundParameters.Count -eq 0 -and $args.Count -eq 0)) {
    Show-Help
}

# ====== Utility Functions ======
function Write-Section {
    param([string]$Title)
    Write-Log "`n==== $Title ====" $BLUE
}

function Remove-BuildArtifacts {
    try {
        if ($DO_NUKE_ALL) {
            Write-Section "Nuking ALL build artifacts"
            Write-Log "WARNING: Deleting all build artifacts!" $RED
            
            $itemsToRemove = @(
                $BUILD_DIR,
                "*.sln",
                "*.vcxproj*",
                ".gitlab-ci.yml",
                "CMakeLists.txt.user"
            )
            
            foreach ($item in $itemsToRemove) {
                if (Test-Path $item) {
                    Remove-Item $item -Recurse -Force
                    Write-Log "Removed: $item" $YELLOW
                }
            }
        }
        elseif ($DO_WIPE) {
            Write-Section "Wiping build directory"
            Write-Log "WARNING: Deleting build directory!" $RED
            if (Test-Path $BUILD_DIR) {
                Remove-Item $BUILD_DIR -Recurse -Force
                Write-Log "Removed: $BUILD_DIR" $YELLOW
            }
        }
        elseif ($DO_CLEAN) {
            Write-Section "Cleaning cache and CMake temp files"
            Write-Log "WARNING: Deleting CMake cache and temp files!" $RED
            
            $cacheFiles = @(
                "$BUILD_DIR\CMakeCache.txt",
                "$BUILD_DIR\cmake_install.cmake"
            )
            
            foreach ($file in $cacheFiles) {
                if (Test-Path $file) {
                    Remove-Item $file -Force
                    Write-Log "Removed: $file" $YELLOW
                }
            }
        }
    }
    catch {
        Write-Error-And-Exit "Cleaning build artifacts: $_"
    }
}

# ====== First handle cleaning operations ======
if ($DO_NUKE_ALL -or $DO_WIPE -or $DO_CLEAN) {
    Remove-BuildArtifacts
    $DO_COMPILE_DB = $true
}

function Export-CompileDb {
    try {
        Write-Section "Exporting compile_commands.json"
        
        & $CMAKE_PATH -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_PREFIX_PATH="D:\Qt\6.9.2\mingw_64" -B $BUILD_DIR -G Ninja
        
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed with exit code $LASTEXITCODE"
        }
        
        $sourceFile = "$BUILD_DIR\compile_commands.json"
        $destFile = "$COMPILE_DB_OUT\compile_commands.json"
        
        if (Test-Path $sourceFile) {
            Copy-Item $sourceFile $destFile -Force
            Write-Log "Compile commands exported to: $destFile" $GREEN
        } else {
            throw "compile_commands.json not found at $sourceFile"
        }
    }
    catch {
        Write-Error-And-Exit "Exporting compile commands: $_"
    }
}

# ====== Compile DB step only ======
if ($DO_COMPILE_DB -and -not $DO_BUILD) {
    Export-CompileDb
    exit 0
}

# Platform preset detection
$OS_NAME = $env:OS
if ($OS_NAME -eq "Windows_NT") {
    $PRESET = $BUILD_PRESET_WINDOWS
} else {
    $PRESET = $BUILD_PRESET_LINUX
}

# Release build Suffix
if ($BUILD_TYPE -eq "Release") {
    $PRESET = "$PRESET-release"
}

# ====== Build Section ======
if ($DO_BUILD) {
    try {
        # Generate compile_commands.json as part of build process if requested
        if ($DO_COMPILE_DB) {
            Write-Section "Generating compile_commands.json"
            & $CMAKE_PATH -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_PREFIX_PATH="D:\Qt\6.9.2\mingw_64" -B $BUILD_DIR -G Ninja
            
            if ($LASTEXITCODE -ne 0) {
                throw "CMake configuration failed with exit code $LASTEXITCODE"
            }
            
            $sourceFile = "$BUILD_DIR\compile_commands.json"
            $destFile = "$COMPILE_DB_OUT\compile_commands.json"
            
            if (Test-Path $sourceFile) {
                Copy-Item $sourceFile $destFile -Force
                Write-Log "Compile commands exported to: $destFile" $GREEN
            }
        }

        Write-Section "Configuring with CMake (Direct Configuration)"
        & $CMAKE_PATH -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCMAKE_PREFIX_PATH="D:\Qt\6.9.2\mingw_64" -B $BUILD_DIR -G Ninja
        
        if ($LASTEXITCODE -ne 0) {
            throw "CMake configuration failed with exit code $LASTEXITCODE"
        }

        Write-Section "Building project"
        & $CMAKE_PATH --build $BUILD_DIR
        
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed with exit code $LASTEXITCODE"
        }
        
        Write-Log "Build completed successfully!" $GREEN
    }
    catch {
        Write-Error-And-Exit "Building project: $_"
    }
}

# ====== Test Section ======
if ($DO_TEST) {
    try {
        Write-Section "Running tests"
        & $CTEST_PATH --preset "test-$PRESET" --output-on-failure
        
        if ($LASTEXITCODE -ne 0) {
            throw "Tests failed with exit code $LASTEXITCODE"
        }
        
        Write-Log "Tests completed successfully!" $GREEN
    }
    catch {
        Handle-Error "Running tests: $_"
    }
}

# ====== Run Section ======
if ($DO_RUN) {
    try {
        Write-Section "Running executable"
        
        # Look for the executable in common locations
        $possiblePaths = @(
            "$BUILD_DIR\KillApiConnect.exe",
            "$BUILD_DIR\bin\KillApiConnect.exe", 
            "$BUILD_DIR\bin\windows\KillApiConnect.exe",
            "$BUILD_DIR\Gunhead-Connect.exe",
            "$BUILD_DIR\bin\Gunhead-Connect.exe",
            "$BUILD_DIR\bin\windows\Gunhead-Connect.exe"
        )
        
        $EXECUTABLE_PATH = $null
        foreach ($path in $possiblePaths) {
            if (Test-Path $path) {
                $EXECUTABLE_PATH = $path
                break
            }
        }
        
        if (-not $EXECUTABLE_PATH) {
            throw "Executable not found in any of the expected locations: $($possiblePaths -join ', ')"
        }
        
        $gdbInitFile = "gdbinit"
        
        if (Test-Path $gdbInitFile) {
            $gdbArgs = @("-x", $gdbInitFile, "--args", $EXECUTABLE_PATH) + $EXTRA_RUN_ARGS
        } else {
            $gdbArgs = @("--args", $EXECUTABLE_PATH) + $EXTRA_RUN_ARGS
        }
        
        Write-Log "Starting GDB with: $EXECUTABLE_PATH" $GREEN
        & $GDB_PATH $gdbArgs
    }
    catch {
        Write-Error-And-Exit "Running executable: $_"
    }
}

# ====== Finalize ======
Write-Section "Finalizing"
try {
    [System.Console]::Beep(1000, 200)
} catch {
    # Ignore beep errors
}

Write-Log "All steps completed successfully!" $GREEN
Write-Log "Log file: $LOG_FILE" $BLUE

# Display summary
Write-Host "`n" -NoNewline
Write-Host "========================================" -ForegroundColor $BLUE
Write-Host "           BUILD SUMMARY" -ForegroundColor $BLUE
Write-Host "========================================" -ForegroundColor $BLUE
Write-Host "Build Type: " -NoNewline; Write-Host $BUILD_TYPE -ForegroundColor $GREEN
Write-Host "Preset: " -NoNewline; Write-Host $PRESET -ForegroundColor $GREEN
if ($DO_BUILD) { Write-Host "[OK] Build completed" -ForegroundColor $GREEN }
if ($DO_TEST) { Write-Host "[OK] Tests executed" -ForegroundColor $GREEN }
if ($DO_RUN) { Write-Host "[OK] Executable launched" -ForegroundColor $GREEN }
Write-Host "Log: " -NoNewline; Write-Host $LOG_FILE -ForegroundColor $YELLOW
Write-Host "========================================" -ForegroundColor $BLUE
