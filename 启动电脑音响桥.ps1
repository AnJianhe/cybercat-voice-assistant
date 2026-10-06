$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$python = Join-Path $projectRoot "diagnostics\pio-venv\Scripts\python.exe"
if (-not (Test-Path -LiteralPath $python)) {
    throw "未找到项目 Python 环境：$python"
}
& $python (Join-Path $projectRoot "pc_tts_bridge.py") @args
