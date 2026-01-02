@echo off
TITLE Dark File Tool Installer
CLS

ECHO ------------------------------------------------
ECHO     DARK FILE TOOL - WINDOWS SETUP
ECHO ------------------------------------------------

:: 1. Check for GCC
WHERE gcc >nul 2>nul
IF %ERRORLEVEL% NEQ 0 (
    ECHO [ERROR] GCC Compiler not found!
    ECHO.
    ECHO Please install MinGW-w64 to compile this tool.
    ECHO Or download the pre-compiled .exe from GitHub Releases.
    PAUSE
    EXIT /B
)

:: 2. Compile
ECHO [-] Compiling source code...
if exist src\darkfile.c (
    gcc src\darkfile.c -o darkfile.exe -O3 -pthread
) else (
    ECHO [ERROR] src\darkfile.c not found!
    PAUSE
    EXIT /B
)

IF EXIST darkfile.exe (
    ECHO [SUCCESS] Compilation complete.
) ELSE (
    ECHO [ERROR] Compilation failed.
    PAUSE
    EXIT /B
)

:: 3. 'Install' (Move to System32 is dangerous, so we suggest adding to PATH)
ECHO.
ECHO [-] Installation Note:
ECHO Windows does not have a standard "bin" folder like Linux.
ECHO To use 'darkfile' from anywhere, copy 'darkfile.exe' to:
ECHO C:\Windows\System32  (Requires Administrator)
ECHO.
ECHO Would you like to try copying it automatically? (Run as Admin required)
SET /P COPYCONFIRM="Type Y to copy, N to finish: "

IF /I "%COPYCONFIRM%"=="Y" (
    COPY darkfile.exe C:\Windows\System32
    IF %ERRORLEVEL% EQU 0 (
        ECHO [SUCCESS] Installed! You can now type 'darkfile' in CMD.
    ) ELSE (
        ECHO [FAIL] Could not copy. Please run this script as Administrator.
    )
)

ECHO.
ECHO Setup finished.
PAUSE
