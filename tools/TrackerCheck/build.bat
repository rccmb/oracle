@echo off
rem Builds TrackerCheck against the rules engine in src\. No OpenCV needed:
rem ChessRules deliberately depends on nothing but the standard library.
setlocal

call "%~dp0find_vcvars.bat"
if errorlevel 1 (
    echo Could not locate vcvars64.bat. Open a Developer Command Prompt and rerun.
    exit /b 1
)

set ORACLE_SRC=%~dp0..\..\src
cd /d "%~dp0"
cl /nologo /std:c++17 /EHsc /O2 /I "%ORACLE_SRC%" TrackerCheck.cpp "%ORACLE_SRC%\chess\ChessRules.cpp" "%ORACLE_SRC%\chess\GameTracker.cpp" /Fe:TrackerCheck.exe
if errorlevel 1 exit /b 1
echo Built TrackerCheck.exe
