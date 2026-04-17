@echo off
SETLOCAL ENABLEDELAYEDEXPANSION

ECHO Starting Image to DDS Compression (PNG/JPG)...
ECHO.

FOR %%E IN (png jpg jpeg) DO (
    FOR %%F IN (*.%%E) DO (
        ECHO.
        ECHO ======================================================
        ECHO Processing file: "%%F"
        ECHO ======================================================

        SET "filename=%%~nF"
        SET "output_dds=%%~nF.dds"

        REM 1. Normal Map (_N) 처리
        IF /I "!filename:~-2!"=="_N" (
            ECHO    -^> Detected Normal Map. Compressing with BC5...
            nvcompress.exe -bc5 "%%F" "!output_dds!"

        REM 2. Base Color (_BC) 처리
        ) ELSE IF /I "!filename:~-3!"=="_BC" (
            ECHO    -^> Detected Base Color. Compressing with BC1...
            nvcompress.exe -bc1 "%%F" "!output_dds!"

        REM 3. Base Color (_D) 처리
        ) ELSE IF /I "!filename:~-2!"=="_D" (
            ECHO    -^> Detected Base Color. Compressing with BC1...
            nvcompress.exe -bc1 "%%F" "!output_dds!"

        REM 4. ORM Map (_ORM) 처리
        ) ELSE IF /I "!filename:~-4!"=="_ORM" (
            ECHO    -^> Detected ORM Map. Compressing with BC1...
            nvcompress.exe -bc1 "%%F" "!output_dds!"

        REM 5. BRDF LUT (_LUT) 처리
        ) ELSE IF /I "!filename:~-4!"=="_LUT" (
            ECHO    -^> Detected BRDF LUT. Converting WITHOUT compression and mipmaps...
            nvcompress.exe -nomips -rgb "%%F" "!output_dds!"

        REM 6. Standard Texture (그 외 나머지) 처리
        ) ELSE (
            ECHO    -^> Detected standard texture. Compressing with BC3...
            nvcompress.exe -alpha -bc3 "%%F" "!output_dds!"
        )
    )
)

ECHO.
ECHO --- All compression tasks finished! ---
ECHO Deleting source PNG and JPG files...

DEL *.png
DEL *.jpg
DEL *.jpeg

ECHO.
ECHO ###############################################################
ECHO ### All tasks are complete! Final DDS files are created.    ###
ECHO ###############################################################
pause