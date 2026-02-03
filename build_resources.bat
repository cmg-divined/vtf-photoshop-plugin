@echo off
call "c:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

set "CNVT_PATH=c:\Users\adobe_photoshop_sdk_2024_win_v1\pluginsdk\samplecode\resources\Cnvtpipl.exe"
set "SRC_DIR=c:\Users\photoshop_vtf_plugin\src"
set "SDK_ROOT=c:\Users\adobe_photoshop_sdk_2024_win_v1"

echo Preprocessing...
cl /EP /DWIN32=1 /D_WINDOWS /DMSWindows=1 /I "%SRC_DIR%" /I "%SDK_ROOT%\pluginsdk\samplecode\common\includes" /I "%SDK_ROOT%\pluginsdk\samplecode\common\resources" /I "%SDK_ROOT%\pluginsdk\photoshopapi\photoshop" /I "%SDK_ROOT%\pluginsdk\photoshopapi\pica_sp" /Tc "%SRC_DIR%\VTFFormat.r" > "win\VTFFormat.rr"

if %ERRORLEVEL% NEQ 0 (
    echo Preprocessing failed with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Converting...
"%CNVT_PATH%" "win\VTFFormat.rr" "win\VTFPiPL.rc"

if %ERRORLEVEL% NEQ 0 (
    echo Conversion failed with error %ERRORLEVEL%
    exit /b %ERRORLEVEL%
)

echo Done.
