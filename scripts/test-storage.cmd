@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
if not exist .pio\host mkdir .pio\host
cl /nologo /EHsc /std:c++17 /utf-8 /Itest\host /Isrc test\storage_test.cpp /Fo.pio\host\storage_test.obj /Fe.pio\host\storage_test.exe
if errorlevel 1 exit /b 1
.pio\host\storage_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /Itest\host /Isrc test\capture_ble_test.cpp /Fo.pio\host\capture_ble_test.obj /Fe.pio\host\capture_ble_test.exe
if errorlevel 1 exit /b 1
.pio\host\capture_ble_test.exe
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /Itest\host /Isrc test\nvs_manager_test.cpp /Fo.pio\host\nvs_manager_test.obj /Fe.pio\host\nvs_manager_test.exe
if errorlevel 1 exit /b 1
.pio\host\nvs_manager_test.exe
