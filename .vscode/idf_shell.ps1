# ESP-IDF environment activation script for VS Code integrated terminal.
# Sourced automatically by the "ESP-IDF PowerShell" terminal profile.
# After this runs, `idf.py` is available as a command in the terminal.

# 1. Set the two root paths everything else derives from.
$env:IDF_PATH = "d:\260604_AIxz\v5.3.4\esp-idf"
$env:IDF_TOOLS_PATH = "C:\Espressif"

# 2. Point IDF_PYTHON_ENV_PATH at the existing complete venv so that
#    idf_tools.py (line 1738) uses it directly instead of looking for the
#    non-existent C:\Espressif\python_env\idf5.3_py3.11_env directory.
#    The venv at tools\python\v5.3.4\venv already has click + idf_component_manager.
$env:IDF_PYTHON_ENV_PATH = Join-Path $env:IDF_TOOLS_PATH "tools\python\v5.3.4\venv"

# 2b. Fix eim-installer directory layout mismatch: idf_tools.py (line 2502)
#     looks for espidf.constraints.v5.3.txt directly under IDF_TOOLS_PATH,
#     but the eim installer put it under IDF_TOOLS_PATH\tools\. Auto-copy.
$constraintFile = "espidf.constraints.v5.3.txt"
$constraintExpected = Join-Path $env:IDF_TOOLS_PATH $constraintFile
$constraintActual = Join-Path $env:IDF_TOOLS_PATH "tools\$constraintFile"
if (-not (Test-Path $constraintExpected) -and (Test-Path $constraintActual)) {
    Copy-Item $constraintActual $constraintExpected -Force
    Write-Host "Copied $constraintFile to expected location" -ForegroundColor DarkGray
}

# 3. Put IDF's bundled Python venv on PATH FIRST, so that `python` resolves
#    to 3.11.9 (IDF v5.3 requires 3.8-3.12; system python may be 3.13+).
$idfPythonVenv = Join-Path $env:IDF_PYTHON_ENV_PATH "Scripts"
if (Test-Path $idfPythonVenv) {
    $env:PATH = "$idfPythonVenv;" + $env:PATH
} else {
    Write-Host "WARNING: IDF python venv not found at $idfPythonVenv" -ForegroundColor Yellow
    Write-Host "         Run: idf_tools.py install-python-env  (in the esp-idf dir)" -ForegroundColor Yellow
}

# 4. Dot-source the official IDF export.ps1. This calls idf_tools.py to
#    add every tool (cmake, ninja, riscv/xtensa gcc, openocd, ...) to PATH.
#    idf_tools.py will respect IDF_PYTHON_ENV_PATH set above.
if (Test-Path "$env:IDF_PATH\export.ps1") {
    . "$env:IDF_PATH\export.ps1"
} else {
    Write-Host "ERROR: $env:IDF_PATH\export.ps1 not found" -ForegroundColor Red
}

# 5. idf.py is on PATH via C:\Espressif\tools\idf-exe\1.0.3\idf.py.exe
#    (added by the IDF export.ps1 above). The idf.py function defined by
#    export.ps1 calls python, which now resolves to the venv set above.

Write-Host ""
Write-Host "ESP-IDF environment ready. Try: idf.py --version" -ForegroundColor Green
Write-Host ""
