@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

echo LVGL Image Converter
echo.

set PYTHON_SCRIPT="%~dp0LVGLImage_bmp_support.py"
set LZ4_COUSTOM_SCRIPT="%~dp0lz4_compress.py"
set MERGE_SCRIPT="%~dp0merge_lvgl_c_arrays.py"
set BMP_CHECK_SCRIPT="%~dp0check_bmp_bitdepth.py"
set CONFIG_READER="%~dp0read_config.py"
set CONFIG_FILE="%~dp0converter_config.json"
set INPUT_FOLDER=".\UI"
set INPUT_LZ4_FOLDER=".\UI_custom"
set OUTPUT_FOLDER="%~dp0output"

if not exist %PYTHON_SCRIPT% (
    echo Error: LVGLImage_bmp_support.py not found
    pause
    exit /b 1
)

if not exist %INPUT_FOLDER% (
    echo Error: UI folder not found
    pause
    exit /b 1
)

if not exist %OUTPUT_FOLDER% mkdir %OUTPUT_FOLDER%

echo Input: %INPUT_FOLDER%
echo Output: %OUTPUT_FOLDER%
echo.

set BMP_1BIT=0
set BMP_OTHER=0
set PNG_COUNT=0
set TOTAL_COUNT=0
set FAIL_COUNT=0

echo Processing images...
echo.

rem ==================== 添加清理output文件夹的步骤 ====================
echo.
echo Cleaning output folder...
if exist %OUTPUT_FOLDER% (
    @REM echo Deleting files in %OUTPUT_FOLDER%...

    rem 使用更安全的方式删除文件
    for /f "delims=" %%f in ('dir /b /a-d "%OUTPUT_FOLDER%\*" 2^>nul') do (
        @REM echo   Deleting: %%f
        del "%OUTPUT_FOLDER%\%%f" >nul 2>&1
    )

    rem 删除子目录（如果有）
    for /f "delims=" %%d in ('dir /b /ad "%OUTPUT_FOLDER%\*" 2^>nul') do (
        @REM echo   Removing directory: %%d
        rmdir /s /q "%OUTPUT_FOLDER%\%%d" >nul 2>&1
    )

    @REM echo Output folder cleaned.
)
rem ==================== 清理结束 ====================

rem 创建BMP检测脚本（如果不存在）
if not exist %BMP_CHECK_SCRIPT% (
    (
        echo import struct
        echo import sys
        echo.
        echo def get_bmp_bitdepth^(filename^):
        echo     try:
        echo         with open^(filename, 'rb'^) as f:
        echo             signature = f.read^(2^)
        echo             if signature != b'BM':
        echo                 return -1
        echo             f.seek^(28^)
        echo             bit_depth = struct.unpack^('<H', f.read^(2^)^)[0]
        echo             return bit_depth
        echo     except:
        echo         return -1
        echo.
        echo if __name__ == "__main__":
        echo     if len^(sys.argv^) ^> 1:
        echo         bitdepth = get_bmp_bitdepth^(sys.argv[1]^)
        echo         print^(bitdepth^)
    ) > %BMP_CHECK_SCRIPT%
)

rem 处理所有图片文件
for /f "delims=" %%f in ('dir /s /b /a-d "%INPUT_FOLDER%\*.bmp" "%INPUT_FOLDER%\*.png" 2^>nul') do (
    set "FILENAME=%%~nxf"
    set "BASENAME=%%~nf"
    set "EXTENSION=%%~xf"

    set /a TOTAL_COUNT+=1
    @REM echo [!TOTAL_COUNT!] !FILENAME!

    if /i "!EXTENSION!"==".bmp" (
        rem 检测BMP位深度
        for /f %%i in ('python %BMP_CHECK_SCRIPT% "%%f"') do set "BITDEPTH=%%i"

        if "!BITDEPTH!"=="1" (
            @REM echo   Detected: 1-bit BMP
            rem 从配置文件读取参数
            for /f %%a in ('python %CONFIG_READER% bmp_1bit cf') do set "CF_1BIT=%%a"
            for /f %%a in ('python %CONFIG_READER% bmp_1bit ofmt') do set "OFMT_1BIT=%%a"
            for /f %%a in ('python %CONFIG_READER% bmp_1bit compress') do set "COMPRESS_1BIT=%%a"

            python %PYTHON_SCRIPT% --cf !CF_1BIT! --ofmt !OFMT_1BIT! --compress !COMPRESS_1BIT! --output %OUTPUT_FOLDER% --name "!BASENAME!" "%%f" >nul 2>&1
            if !errorlevel! equ 0 (
                set /a BMP_1BIT+=1
                @REM echo   OK: !CF_1BIT! !COMPRESS_1BIT!
            ) else (
                set /a FAIL_COUNT+=1
                @REM echo   Failed
            )
        ) else if "!BITDEPTH!"=="-1" (
            @REM echo   Invalid BMP file
            set /a FAIL_COUNT+=1
            @REM echo   Failed
        ) else (
            @REM echo   Detected: !BITDEPTH!-bit BMP
            rem 从配置文件读取参数
            for /f %%a in ('python %CONFIG_READER% bmp_other cf') do set "CF_OTHER=%%a"
            for /f %%a in ('python %CONFIG_READER% bmp_other ofmt') do set "OFMT_OTHER=%%a"
            for /f %%a in ('python %CONFIG_READER% bmp_other compress') do set "COMPRESS_OTHER=%%a"

            python %PYTHON_SCRIPT% --cf !CF_OTHER! --ofmt !OFMT_OTHER! --compress !COMPRESS_OTHER! --output %OUTPUT_FOLDER% --name "!BASENAME!" "%%f" >nul 2>&1
            if !errorlevel! equ 0 (
                set /a BMP_OTHER+=1
                @REM echo   OK: !CF_OTHER! !COMPRESS_OTHER!
            ) else (
                set /a FAIL_COUNT+=1
                @REM echo   Failed
            )
        )
    ) else (
        @REM echo   Detected: PNG
        rem 从配置文件读取PNG参数
        for /f %%a in ('python %CONFIG_READER% png cf') do set "CF_PNG=%%a"
        for /f %%a in ('python %CONFIG_READER% png ofmt') do set "OFMT_PNG=%%a"
        for /f %%a in ('python %CONFIG_READER% png compress') do set "COMPRESS_PNG=%%a"

        python %PYTHON_SCRIPT% --cf !CF_PNG! --ofmt !OFMT_PNG! --compress !COMPRESS_PNG! --output %OUTPUT_FOLDER% --name "!BASENAME!" "%%f" >nul 2>&1
        if !errorlevel! equ 0 (
            set /a PNG_COUNT+=1
            @REM echo   OK: !CF_PNG! !COMPRESS_PNG!
        ) else (
            set /a FAIL_COUNT+=1
            @REM echo   Failed
        )
    )
    @REM echo.
)

echo.
python %LZ4_COUSTOM_SCRIPT% %INPUT_LZ4_FOLDER%
echo.

echo.
python %MERGE_SCRIPT% %OUTPUT_FOLDER%
echo.

pause
