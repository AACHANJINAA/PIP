@echo off
setlocal enabledelayedexpansion

:: 1. 경로 강제 고정 (시스템 에러 방지)
cd /d "%~dp0"

ECHO ========================================================
ECHO  Direct Converter for "Night.hdr"
ECHO  No Loops, No Variables, Just Action.
ECHO ========================================================

:: ---------------------------------------------------------
:: [Step 1] cmft: 모양 잡기 (직접 입력!)
:: ---------------------------------------------------------
ECHO.
ECHO [Step 1] cmft running...

:: 기존 임시 파일 청소
IF EXIST "Night_Temp.dds" DEL "Night_Temp.dds"
IF EXIST "Night_Temp.dds.dds" DEL "Night_Temp.dds.dds"

:: Night.hdr을 직접 지정해서 실행
cmft.exe --input "Night.hdr" ^
         --filter none ^
         --dstFaceSize 1024 ^
         --output0 "Night_Temp" ^
         --output0params dds,rgba16f,cubemap

:: 확장자 꼬임(.dds.dds) 자동 수리
IF EXIST "Night_Temp.dds.dds" (
    ECHO   [Info] Renaming .dds.dds to .dds
    MOVE /Y "Night_Temp.dds.dds" "Night_Temp.dds" >nul
)

:: ---------------------------------------------------------
:: [Step 2] texconv: BC6H + DX10 헤더 (직접 입력!)
:: ---------------------------------------------------------
IF EXIST "Night_Temp.dds" (
    ECHO.
    ECHO [Step 2] texconv running (BC6H + DX10)...
    
    :: Night_Temp.dds를 입력받아 처리
    texconv.exe -f BC6H_UF16 -dx10 -m 0 -y -o . "Night_Temp.dds"
    
    :: 최종 결과물을 Night.dds로 이름 변경
    MOVE /Y "Night_Temp.dds" "Night.dds" >nul
    
    ECHO.
    ECHO ========================================================
    ECHO  [SUCCESS] "Night.dds" Created Successfully!
    ECHO  Format: BC6H (DX10 Header)
    ECHO ========================================================
    
) ELSE (
    ECHO.
    ECHO [ERROR] Step 1 Failed. "Night.hdr" file might be missing or locked.
)

PAUSE