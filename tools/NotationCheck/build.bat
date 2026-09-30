@echo off
rem Builds NotationCheck against the rules in src\chess. Standard library only.
setlocal

call "%~dp0find_vcvars.bat"
if errorlevel 1 (
    echo Could not locate vcvars64.bat. Open a Developer Command Prompt and rerun.
    exit /b 1
)

set ORACLE_SRC=%~dp0..\..\src
cd /d "%~dp0"
if exist NotationCheck.exe del NotationCheck.exe
cl /nologo /std:c++17 /EHsc /O2 /I "%ORACLE_SRC%" NotationCheck.cpp "%ORACLE_SRC%\chess\ChessRules.cpp" /Fe:NotationCheck.exe
if errorlevel 1 exit /b 1
echo Built NotationCheck.exe
