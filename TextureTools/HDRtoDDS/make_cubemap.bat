@echo off
cd /d "%~dp0"

ECHO ========================================================
ECHO  Simple Converter (No Loop)
ECHO ========================================================

:: 1. cmft 실행 (파일 이름 직접 박아넣기!)
ECHO [Step 1] Processing cmft...
cmft.exe --input "Night.hdr" --filter none --dstFaceSize 1024 --output0 "Night_Temp" --output0params dds,rgba16f,cubemap

:: 파일명 꼬임 방지 보정
IF EXIST "Night_Temp.dds.dds" MOVE /Y "Night_Temp.dds.dds" "Night_Temp.dds" >nul

:: 2. texconv 실행
IF EXIST "Night_Temp.dds" (
    ECHO [Step 2] Processing texconv (BC6H)...
    texconv.exe -f BC6H_UF16 -o . -y "Night_Temp.dds"
    
    :: 최종 이름 변경
    MOVE /Y "Night_Temp.dds" "Night.dds" >nul
    
    ECHO.
    ECHO [SUCCESS] "Night.dds" Created!
) ELSE (
    ECHO [ERROR] Step 1 Failed.
)

PAUSE