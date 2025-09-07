@echo off
REM Wrapper script to run the PowerShell build script
REM Usage: all_do.bat [arguments]
REM
REM Examples:
REM   all_do.bat -Release
REM   all_do.bat -BuildRun
REM   all_do.bat -JustBuild
REM   all_do.bat -DebugMode
REM   all_do.bat -Help

powershell.exe -ExecutionPolicy Bypass -File "%~dp0all_do.ps1" %*
