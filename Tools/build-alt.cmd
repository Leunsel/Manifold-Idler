@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%i"
if not defined MSBUILD (
    echo MSBuild was not found.
    exit /b 1
)
"%MSBUILD%" "%~dp0..\Manifold-Idler.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:CardFarm=true "/p:OutDir=%~dp0..\x64\Alt\\" "/p:IntDir=%~dp0..\x64\obj\Alt\\" /v:minimal /nologo
