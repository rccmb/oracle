@echo off
rem Builds FenCheck against the FEN builder in Oracle\.
rem
rem Set ORACLE_OPENCV_ROOT to override the OpenCV location, exactly as the main
rem project does. The default is the in-repo OpenCV the README describes.
setlocal

call "%~dp0find_vcvars.bat"
if errorlevel 1 (
    echo Could not locate vcvars64.bat. Open a Developer Command Prompt and rerun.
    exit /b 1
)

set ORACLE_SRC=%~dp0..\..\Oracle
if "%ORACLE_OPENCV_ROOT%"=="" set ORACLE_OPENCV_ROOT=%~dp0..\..\OpenCV\opencv\build

if not exist "%ORACLE_OPENCV_ROOT%\include" (
    echo OpenCV not found at "%ORACLE_OPENCV_ROOT%".
    echo Set ORACLE_OPENCV_ROOT to the OpenCV build directory.
    exit /b 1
)

cd /d "%~dp0"
cl /nologo /std:c++17 /EHsc /MDd ^
   /I "%ORACLE_SRC%" /I "%ORACLE_SRC%\ImGui" /I "%ORACLE_OPENCV_ROOT%\include" ^
   FenCheck.cpp ^
   "%ORACLE_SRC%\ChessboardDetection.cpp" "%ORACLE_SRC%\BoardDetection.cpp" ^
   "%ORACLE_SRC%\Globals.cpp" "%ORACLE_SRC%\Utils.cpp" ^
   "%ORACLE_SRC%\FileHandler.cpp" "%ORACLE_SRC%\StockfishHandler.cpp" ^
   "%ORACLE_SRC%\InitialConfiguration.cpp" ^
   /Fe:FenCheck.exe ^
   /link /LIBPATH:"%ORACLE_OPENCV_ROOT%\x64\vc16\lib" opencv_world4120d.lib user32.lib gdi32.lib
if errorlevel 1 exit /b 1

copy /y "%ORACLE_OPENCV_ROOT%\x64\vc16\bin\opencv_world4120d.dll" . >nul
echo Built FenCheck.exe
