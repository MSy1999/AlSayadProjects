# Activate the installed EIM environment in this process, then build without flashing.
$env:IDF_TOOLS_PATH = 'C:\Espressif'
$env:IDF_PYTHON_ENV_PATH = 'C:\Espressif\tools\python\v6.1\venv'
# EIM installation has no constraints file. Check installed requirements without it.
$env:IDF_PYTHON_CHECK_CONSTRAINTS = 'no'
$env:PYTHONUTF8 = '1'
. C:\esp\v6.1\esp-idf\export.ps1
if (-not $?) { exit 1 }
idf.py build
exit $LASTEXITCODE
