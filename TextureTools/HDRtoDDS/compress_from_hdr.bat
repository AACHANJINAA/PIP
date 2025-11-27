@echo off
:: 변수 확장 따위 필요 없다. 그냥 바로 박아넣는다.

ECHO Starting HDR to DDS (BC6) Compression...
ECHO.

:: --- Main Loop for HDR files ---
:: %%F : 원본 파일명 (예: Night.hdr)
:: %%~nF : 파일명만 추출 (예: Night)
:: %%~nF.dds : 확장자 변경 (예: Night.dds)

FOR %%F IN (*.hdr) DO (
    ECHO.
    ECHO [Processing]: "%%F"
    
    :: 바로 명령어로 때려박기
    nvcompress.exe -bc6 "%%F" "%%~nF.dds"
    
    IF ERRORLEVEL 1 (
        ECHO [ERROR] Failed to convert "%%F". 
        ECHO         Make sure the file isn't corrupted and FreeImage.dll is present.
        PAUSE
    )
)

ECHO.
ECHO --- Finished! Check if .dds files are created. ---

:: 원본 삭제는 혹시 모르니 주석 처리해뒀다. 필요하면 REM 지우고 써라.
:: DEL *.hdr

PAUSE