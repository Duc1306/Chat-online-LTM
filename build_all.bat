@echo off
echo ========================================
echo   Building ALL - Chat Server and Client
echo ========================================
echo.

REM Build Server
echo [1/2] Building Server...
cd server
gcc -o chat_server.exe server.c server_handlers.c server_utils.c ..\client\protocol.c -I..\client -lws2_32 -Wall -Wextra
if %ERRORLEVEL% NEQ 0 (
    echo   Server build FAILED!
    cd ..
    pause
    exit /b 1
)
echo   Server build OK
cd ..

echo.

REM Build Client
echo [2/2] Building Client...
cd client
gcc -o client.exe client.c protocol.c -lws2_32 -Wall -Wextra
gcc -o client_gui.exe client_gui.c protocol.c -lws2_32 -lgdi32 -luser32 -lcomctl32 -Wall -Wextra
if %ERRORLEVEL% NEQ 0 (
    echo   Client build FAILED!
    cd ..
    pause
    exit /b 1
)
echo   Client build OK
cd ..

echo.
echo ========================================
echo   ALL BUILDS SUCCESSFUL!
echo ========================================
echo.
echo Server: server\chat_server.exe
echo Client Console: client\client.exe
echo Client GUI: client\client_gui.exe
echo.
echo To test:
echo   1. Open terminal 1: cd server ^&^& chat_server.exe
echo   2. Open terminal 2: cd client ^&^& client.exe
echo   3. OR run GUI: cd client ^&^& client_gui.exe
echo.
pause
