@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

ECHO ========================================================
ECHO  [Step 2 Only] Finishing the Job (BC6H Compression)
ECHO ========================================================

:: 1. 재료 확인
IF NOT EXIST "Night_Temp.dds" (
    ECHO [ERROR] 'Night_Temp.dds' file is missing!
    ECHO Please run the previous step again.
    PAUSE
    EXIT /B
)

:: 2. 툴 확인
IF NOT EXIST "texconv.exe" (
    ECHO [ERROR] 'texconv.exe' is missing!
    PAUSE
    EXIT /B
)

ECHO [Processing] Compressing 'Night_Temp.dds' to BC6H...
ECHO (If this fails, CLOSE Visual Studio image tabs!)
ECHO.

:: 3. 압축 실행 (에러 메시지를 보기 위해 pause 추가)
texconv.exe -f BC6H_UF16 -dx10 -m 0 -y "Night_Temp.dds"

IF ERRORLEVEL 1 (
    ECHO.
    ECHO [FAIL] Texconv crashed!
    ECHO Possible reasons:
    ECHO  1. 'Night_Temp.dds' is OPEN in another program.
    ECHO  2. Missing DLLs for texconv.
) ELSE (
    :: 4. 성공 시 이름 변경
    MOVE /Y "Night_Temp.dds" "Night.dds" >nul
    ECHO.
    ECHO [SUCCESS] Final 'Night.dds' Created!
    ECHO Copy this file to your engine folder.
)

PAUSE