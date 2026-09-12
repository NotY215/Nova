# Vayu / C++ / Python benchmark harness.
# Run from E:\Vayu with: powershell -ExecutionPolicy Bypass -File examples\bench\run.ps1

$ErrorActionPreference = "Stop"
$vayuc = "build\x64-release\bin\vayuc.exe"

function Time-It($label, [scriptblock]$body) {
    $t0 = Get-Date
    & $body | Out-Null
    $t1 = Get-Date
    $ms = [math]::Round(($t1 - $t0).TotalMilliseconds, 1)
    "{0,-28} {1,10} ms" -f $label, $ms
}

# --- Fibonacci ---
Write-Host "`n=== fib(30) ===`n"
Time-It "Vayu native"  { & $vayuc examples\bench\native_fib.vyu --native }
Time-It "C++ g++ -O2"  {
    if (-not (Test-Path examples\bench\cpp\fib.exe)) {
        g++ -O2 examples\bench\cpp\fib.cpp -o examples\bench\cpp\fib.exe
    }
    & examples\bench\cpp\fib.exe
}
Time-It "Python 3"     { python examples\bench\py\fib.py }

# --- Loop ---
Write-Host "`n=== loop (50M) ===`n"
Time-It "Vayu native"  { & $vayuc examples\bench\native_loop.vyu --native }
Time-It "C++ g++ -O2"  {
    if (-not (Test-Path examples\bench\cpp\loop.exe)) {
        g++ -O2 examples\bench\cpp\loop.cpp -o examples\bench\cpp\loop.exe
    }
    & examples\bench\cpp\loop.exe
}
Time-It "Python 3"     { python examples\bench\py\loop.py }

# --- OOP ---
Write-Host "`n=== oop (5M instances) ===`n"
Time-It "Vayu native"  { & $vayuc examples\bench\native_oop.vyu --native }
Time-It "C++ g++ -O2"  {
    if (-not (Test-Path examples\bench\cpp\oop.exe)) {
        g++ -O2 examples\bench\cpp\oop.cpp -o examples\bench\cpp\oop.exe
    }
    & examples\bench\cpp\oop.exe
}
Time-It "Python 3"     { python examples\bench\py\oop.py }

Write-Host ""