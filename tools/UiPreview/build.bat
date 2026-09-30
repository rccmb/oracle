@echo off
rem Builds UiPreview against the interface in src\.
rem
rem Everything in src\ except main.cpp is compiled in, so the menu and the
rem overlay are drawn by exactly the code the overlay runs.
rem
rem Set ORACLE_OPENCV_ROOT to override the OpenCV location, exactly as the main
rem project does. The default is the in-repo OpenCV the README describes.
setlocal

call "%~dp0find_vcvars.bat"
if errorlevel 1 (
    echo Could not locate vcvars64.bat. Open a Developer Command Prompt and rerun.
    exit /b 1
)

set ORACLE_SRC=%~dp0..\..\src
set IMGUI=%~dp0..\..\third_party\imgui
if "%ORACLE_OPENCV_ROOT%"=="" set ORACLE_OPENCV_ROOT=%~dp0..\..\OpenCV\opencv\build

if not exist "%ORACLE_OPENCV_ROOT%\include" (
    echo OpenCV not found at "%ORACLE_OPENCV_ROOT%".
    echo Set ORACLE_OPENCV_ROOT to the OpenCV build directory.
    exit /b 1
)

set SOURCES=
for /r "%ORACLE_SRC%" %%f in (*.cpp) do (
    if /i not "%%~nxf"=="main.cpp" call set SOURCES=%%SOURCES%% "%%f"
)

cd /d "%~dp0"
if not exist obj mkdir obj
if exist UiPreview.exe del UiPreview.exe

cl /nologo /std:c++17 /EHsc /MDd /MP /O2 /DUNICODE /D_UNICODE ^
   /I "%ORACLE_SRC%" /I "%IMGUI%" /I "%ORACLE_OPENCV_ROOT%\include" ^
   /Fo:obj\ ^
   UiPreview.cpp %SOURCES% ^
   "%IMGUI%\imgui.cpp" "%IMGUI%\imgui_draw.cpp" "%IMGUI%\imgui_tables.cpp" ^
   "%IMGUI%\imgui_widgets.cpp" "%IMGUI%\imgui_impl_dx11.cpp" "%IMGUI%\imgui_impl_win32.cpp" ^
   /Fe:UiPreview.exe ^
   /link /LIBPATH:"%ORACLE_OPENCV_ROOT%\x64\vc16\lib" opencv_world4120d.lib ^
   d3d11.lib user32.lib gdi32.lib shell32.lib ole32.lib comdlg32.lib dwmapi.lib
if errorlevel 1 exit /b 1

copy /y "%ORACLE_OPENCV_ROOT%\x64\vc16\bin\opencv_world4120d.dll" . >nul
echo Built UiPreview.exe
