@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /I "Arduino\ServoSerialTest\tests\stubs" "Arduino\ServoSerialTest\tests\serial_command_tests.cpp" /Fo"tmp\servo-serial-test\serial_command_tests.obj" /Fe"tmp\servo-serial-test\serial_command_tests.exe"
if errorlevel 1 exit /b 1
"tmp\servo-serial-test\serial_command_tests.exe"
