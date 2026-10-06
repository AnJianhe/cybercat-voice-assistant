@echo off
setlocal
if exist "%~dp0CyberCatControlCenter.exe" (
    start "" "%~dp0CyberCatControlCenter.exe"
    exit /b
)
where py >nul 2>&1
if not errorlevel 1 (
    set "PYTHON_LAUNCHER=py -3"
) else (
    where python >nul 2>&1
    if errorlevel 1 (
        echo 未找到 Python 3。请先安装 Python 3，或使用已打包的 CyberCatControlCenter.exe。
        pause
        exit /b 1
    )
    set "PYTHON_LAUNCHER=python"
)
%PYTHON_LAUNCHER% -c "import serial" >nul 2>&1
if errorlevel 1 (
    echo 缺少图形配置器依赖，请运行：%PYTHON_LAUNCHER% -m pip install -r "%~dp0bridge_wroom\tools\requirements.txt"
    pause
    exit /b 1
)
%PYTHON_LAUNCHER% "%~dp0bridge_wroom\tools\cybercat_bridge_config.py"
if errorlevel 1 pause
endlocal
