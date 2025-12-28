@echo off
SETLOCAL ENABLEDELAYEDEXPANSION

ECHO Starting Recursive PNG/JPG to DDS Compression...
ECHO.

REM --- Main Loop for PNG and JPG files in all subdirectories ---
REM (*.png *.jpg *.jpeg)를 통해 세 가지 확장자를 모두 찾습니다.
FOR /R %%F IN (*.png *.jpg *.jpeg) DO (
    ECHO.
    ECHO ======================================================
    ECHO Processing file: "%%F"
    ECHO ======================================================

    SET "fullpath=%%~dpnF"
    SET "filename=%%~nF"
    SET "output_dds=%%~dpnF.dds"

    REM 파일 이름 끝자리에 따른 압축 방식 분기 (기존 로직 유지)
    IF /I "!filename:~-2!"=="_N" (
        ECHO   -^> Detected Normal Map. Compressing with BC5...
        nvcompress.exe -bc5 "%%F" "!output_dds!"
    ) ELSE IF /I "!filename:~-3!"=="_BC" (
        ECHO   -^> Detected Base Color. Compressing with BC1...
        nvcompress.exe -bc1 "%%F" "!output_dds!"
    ) ELSE IF /I "!filename:~-2!"=="_D" (
        ECHO   -^> Detected Base Color. Compressing with BC1...
        nvcompress.exe -bc1 "%%F" "!output_dds!"
    ) ELSE (
        ECHO   -^> Detected standard texture. Compressing with BC3...
        nvcompress.exe -alpha -bc3 "%%F" "!output_dds!"
    )

    REM DDS 파일이 성공적으로 생성되었는지 확인 후 원본(PNG or JPG) 삭제
    IF EXIST "!output_dds!" (
        ECHO   -^> Success! Deleting source: "%%F"
        DEL "%%F"
    ) ELSE (
        ECHO   [ERROR] Failed to create DDS for "%%F"
    )
)

ECHO.
ECHO ###############################################################
ECHO ### All tasks are complete! PNG and JPG are now DDS.        ###
###############################################################
pause