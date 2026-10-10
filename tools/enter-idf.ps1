# 点加载本文件，只配置当前 PowerShell 进程的 ESP-IDF 6.1 环境。
$ErrorActionPreference = 'Stop'
$env:IDF_PATH = 'D:\Develop_C\Espnew\v6.1\esp-idf'
$env:IDF_TOOLS_PATH = 'D:\Develop_C\Espnew\610_Tool'
$env:IDF_PYTHON_ENV_PATH = Join-Path $env:IDF_TOOLS_PATH 'python_env\idf6.1_py3.11_env'
$env:IDF_PY_BUILD_JOBS = '2'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$env:IDF_TARGET = 'esp32s3'

$xchatPython = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
$xchatExport = Join-Path $env:IDF_PATH 'export.ps1'
if (!(Test-Path -LiteralPath $xchatPython) -or !(Test-Path -LiteralPath $xchatExport)) {
    throw 'ESP-IDF 6.1 SDK 或 Python 环境尚未安装完整，请先完成本工程的独立环境安装。'
}
$env:Path = (Split-Path -Parent $xchatPython) + ';C:\Program Files\Git\cmd;C:\Program Files\Git\usr\bin;' + $env:Path
. $xchatExport
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF 6.1 环境导出失败。' }

# export.ps1 用于交互终端；后续脚本统一使用当前 SDK 的绝对路径。
function global:idf.py {
    & (Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe') (Join-Path $env:IDF_PATH 'tools\idf.py') @args
}
