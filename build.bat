@echo off
echo ===================================================
echo Compilando VDisPlay (C + Flutter Windows Desktop)
echo ===================================================

echo.
echo [1/2] Compilando a Biblioteca C Nativa...
if not exist native\build mkdir native\build
cd native\build
cmake -DCMAKE_BUILD_TYPE=Release ..\..
cmake --build . --config Release
if %ERRORLEVEL% neq 0 (
    echo Erro ao compilar o codigo C nativo!
    cd ..\..
    exit /b %ERRORLEVEL%
)
cd ..\..

echo.
echo [2/2] Compilando o Flutter para Windows...
flutter build windows --release
if %ERRORLEVEL% neq 0 (
    echo Erro ao compilar o app Flutter!
    exit /b %ERRORLEVEL%
)

echo.
echo ===================================================
echo Build concluido! O executavel esta em:
echo build\windows\x64\runner\Release\vdisplay.exe
echo ===================================================
