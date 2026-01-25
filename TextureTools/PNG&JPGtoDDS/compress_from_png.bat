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

        REM Normal Map (_N) 처리
        IF /I "!filename:~-2!"=="_N" (
            ECHO    -^> Detected Normal Map. Compressing with BC5...
            nvcompress.exe -bc5 "%%F" "!output_dds!"
        ) ELSE (
            REM Base Color (_BC) 처리
            IF /I "!filename:~-3!"=="_BC" (
                ECHO    -^> Detected Base Color. Compressing with BC1...
                nvcompress.exe -bc1 "%%F" "!output_dds!"
            ) ELSE IF /I "!filename:~-2!"=="_D" (
                ECHO    -^> Detected Base Color. Compressing with BC1...
                nvcompress.exe -bc1 "%%F" "!output_dds!"
            ) ELSE (
                REM ORM Map (_ORM) 처리
                IF /I "!filename:~-4!"=="_ORM" (
                    ECHO    -^> Detected ORM Map. Compressing with BC1...
                    nvcompress.exe -bc1 "%%F" "!output_dds!"
                ) ELSE (
                    REM Standard Texture 처리
                    ECHO    -^> Detected standard texture. Compressing with BC3...
                    nvcompress.exe -alpha -bc3 "%%F" "!output_dds!"
                )
            )
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