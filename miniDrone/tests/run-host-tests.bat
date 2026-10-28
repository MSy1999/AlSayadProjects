@echo off
if not exist build-temp mkdir build-temp
where cl >nul 2>nul
if errorlevel 1 call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cl /nologo /W4 /Icomponents\as_flight tests\flight_test.c components\as_flight\as_flight.c components\as_flight\as_protocol.c /Fetests\flight_test.exe /Fobuild-temp\
if errorlevel 1 exit /b 1
tests\flight_test.exe
