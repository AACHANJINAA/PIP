@echo off
SETLOCAL ENABLEDELAYEDEXPANSION

ECHO Starting PNG to DDS Compression...
ECHO.

REM --- Main Loop for PNG files ---
FOR %%F IN (*.png) DO (
    ECHO.
    ECHO ======================================================
    ECHO Processing file: "%%F"
    ECHO ======================================================

    SET "filename=%%~nF"
    SET "output_dds=%%~nF.dds"

    REM Using delayed expansion for robust substring check
    IF /I "!filename:~-2!"=="_N" (
        ECHO   -> Detected Normal Map. Compressing with BC5...
        nvcompress.exe -bc5 "%%F" "!output_dds!"
    ) ELSE IF /I "!filename:~-3!"=="_BC" (
        ECHO   -> Detected Base Color. Compressing with BC1...
        nvcompress.exe -bc1 "%%F" "!output_dds!"
    ) ELSE IF /I "!filename:~-2!"=="_D" (
            ECHO   -> Detected Base Color. Compressing with BC1...
        nvcompress.exe -bc1 "%%F" "!output_dds!"
    ) ELSE (
        ECHO   -> Detected standard texture. Compressing with BC3...
        nvcompress.exe -alpha -bc3 "%%F" "!output_dds!"
    )
)

ECHO.
ECHO --- All compression tasks finished! ---
ECHO Deleting source PNG files...

REM --- Final Step: Delete the source PNG files ---
DEL *.png

ECHO.
ECHO ###############################################################
ECHO ### All tasks are complete! Final DDS files are created.    ###
ECHO ###############################################################
pause