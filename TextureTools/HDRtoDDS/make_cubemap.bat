@echo off
setlocal enabledelayedexpansion

:: ========================================================
:: [필수] 현재 폴더로 작업 위치 강제 고정 (시스템 에러 방지)
:: ========================================================
cd /d "%~dp0"

:: [설정] 큐브맵 한 면 크기 (1024, 2048 등)
SET FACE_SIZE=2048

ECHO ========================================================
ECHO  Ultimate HDR to BC6H Cubemap Converter
ECHO  Target: "Night.hdr" -> "Night.dds"
ECHO ========================================================

:: ---------------------------------------------------------
:: 0. 준비물 확인 (없으면 시작도 안 함)
:: ---------------------------------------------------------
IF NOT EXIST "cmft.exe" ( ECHO [Error] 'cmft.exe' not found! & PAUSE & EXIT /B )
IF NOT EXIST "texconv.exe" ( ECHO [Error] 'texconv.exe' not found! & PAUSE & EXIT /B )
IF NOT EXIST "Night.hdr" ( ECHO [Error] 'Night.hdr' not found! & PAUSE & EXIT /B )

:: 기존 임시 파일 및 결과 파일 청소 (충돌 방지)
IF EXIST "Night_Temp.dds" DEL "Night_Temp.dds"
IF EXIST "Night_Temp.dds.dds" DEL "Night_Temp.dds.dds"
IF EXIST "Night.dds" DEL "Night.dds"

:: ---------------------------------------------------------
:: [Step 1] cmft: 파노라마 펴기 (고화질 무압축 RGBA16F)
:: ---------------------------------------------------------
ECHO.
ECHO [Step 1] Unwrapping Panorama to Cubemap...

cmft.exe --input "Night.hdr" ^
         --filter none ^
         --dstFaceSize %FACE_SIZE% ^
         --output0 "Night_Temp" ^
         --output0params dds,rgba16f,cubemap

:: [자동 수리] cmft가 바보같이 확장자를 두 번 붙였을 경우 수정
IF EXIST "Night_Temp.dds.dds" (
    ECHO   [Info] Fixing filename (.dds.dds -> .dds)...
    MOVE /Y "Night_Temp.dds.dds" "Night_Temp.dds" >nul
)

:: 1단계 결과 확인
IF NOT EXIST "Night_Temp.dds" (
    ECHO.
    ECHO [ERROR] Step 1 Failed! 'Night_Temp.dds' was not created.
    ECHO Check if 'Night.hdr' is valid.
    PAUSE
    EXIT /B
)

:: ---------------------------------------------------------
:: [Step 2] texconv: BC6H 압축 + DX10 헤더 + 밉맵 생성
:: ---------------------------------------------------------
ECHO.
ECHO [Step 2] Compressing to BC6H (DX10 Header + Mipmaps)...
ECHO (NOTE: If this fails, CLOSE any image tabs in Visual Studio!)

:: -f BC6H_UF16 : HDR용 압축
:: -dx10        : D3D12 호환 헤더
:: -m 0         : 밉맵 자동 생성
:: -y           : 덮어쓰기
texconv.exe -f BC6H_UF16 -dx10 -m 0 -y -o . "Night_Temp.dds"

IF ERRORLEVEL 1 (
    ECHO.
    ECHO [FAIL] Texconv crashed!
    ECHO Possible reasons:
    ECHO  1. 'Night_Temp.dds' is OPEN in Visual Studio. (Close it!)
    ECHO  2. Missing DLLs (vcomp140.dll, etc.)
    PAUSE
    EXIT /B
)

:: ---------------------------------------------------------
:: [Step 3] 최종 마무리
:: ---------------------------------------------------------
ECHO.
ECHO [Step 3] Finalizing...

:: 최종 파일명으로 변경
MOVE /Y "Night_Temp.dds" "Night.dds" >nul

IF EXIST "Night.dds" (
    ECHO.
    ECHO ========================================================
    ECHO  [SUCCESS] "Night.dds" Created Successfully!
    ECHO  Format: BC6H_UF16 / Mips: Yes / Header: DX10
    ECHO  Size: %FACE_SIZE% x %FACE_SIZE% (Cubemap)
    ECHO ========================================================
    ECHO Now copy "Night.dds" to your Engine's Resource folder.
) ELSE (
    ECHO [ERROR] Something went wrong during rename.
)

PAUSE