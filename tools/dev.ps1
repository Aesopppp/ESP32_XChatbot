param(
    [ValidateSet('build', 'menuconfig', 'flash', 'monitor', 'flash-monitor', 'verify', 'size')]
    [string]$Action = 'build',
    [string]$Port
)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'enter-idf.ps1')
$xchatRoot = Split-Path -Parent $PSScriptRoot
$xchatPython = Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts\python.exe'
$xchatIdf = Join-Path $env:IDF_PATH 'tools\idf.py'
Push-Location $xchatRoot
try {
    if ($Action -eq 'build') {
        # 上游入口同时配置目标、VoCat 板型、表情资源、语言，并生成合并固件。
        # 它会重新生成 sdkconfig；自定义配置请用 menuconfig 和 idf.py build。
        & $xchatPython 'scripts\build.py' 'espressif/esp-vocat' '--name' 'esp-vocat' '--language' 'zh-CN' '--wake-word' 'wn9_nihaoxiaoyi_tts2'
    } elseif ($Action -eq 'verify') {
        & $xchatPython 'tools\run_host_tests.py'
    } else {
        $xchatArgs = @()
        if ($Action -in @('flash', 'monitor', 'flash-monitor')) {
            if ([string]::IsNullOrWhiteSpace($Port)) { throw '请使用 -Port COMx 指定实际串口，例如 -Port COM6。' }
            $xchatArgs += @('-p', $Port)
        }
        if ($Action -eq 'flash-monitor') { $xchatArgs += @('flash', 'monitor') }
        else { $xchatArgs += $Action }
        & $xchatPython $xchatIdf @xchatArgs
    }
    $xchatExit = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $xchatExit
