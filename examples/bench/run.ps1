# Vayu / C++ / Python benchmark harness.
# From E:\Vayu:
#   powershell -ExecutionPolicy Bypass -File examples\bench\run.ps1

$ErrorActionPreference = "Continue"
$vayuc = "build\x64-release\bin\vayuc.exe"

function Time-It($label, [scriptblock]$body) {
    $body | Out-Null      # warm-up / correctness check (Vayu re-invokes compiler,
                          # so a warm-up run primes the OS cache)
    $t0 = Get-Date
    $body | Out-Null
    $t1 = Get-Date
    $ms = [math]::Round(($t1 - $t0).TotalMilliseconds, 1)
    "{0,-22} {1,10} ms" -f $label, $ms
}

function Ensure-Cpp($src, $exe) {
    if (-not (Test-Path $exe)) {
        Write-Host "  (building $exe)..." -ForegroundColor DarkGray
        g++ -O2 $src -o $exe
    }
}

Write-Host "`n================ fib(30) ================`n"
Time-It "Vayu native"  { & $vayuc examples\bench\native_fib.vyu --native }
Ensure-Cpp "examples\bench\cpp\fib.cpp" "examples\bench\cpp\fib.exe"
Time-It "C++ g++ -O2"  { & examples\bench\cpp\fib.exe }
Time-It "Python 3"     { python examples\bench\py\fib.py }

Write-Host "`n================ 50M loop ================`n"
Time-It "Vayu native"  { & $vayuc examples\bench\native_loop.vyu --native }
Ensure-Cpp "examples\bench\cpp\loop.cpp" "examples\bench\cpp\loop.exe"
Time-It "C++ g++ -O2"  { & examples\bench\cpp\loop.exe }
Time-It "Python 3"     { python examples\bench\py\loop.py }

Write-Host "`n================ 5M oop =================`n"
Time-It "Vayu native"  { & $vayuc examples\bench\native_oop.vyu --native }
Ensure-Cpp "examples\bench\cpp\oop.cpp" "examples\bench\cpp\oop.exe"
Time-It "C++ g++ -O2"  { & examples\bench\cpp\oop.exe }
Time-It "Python 3"     { python examples\bench\py\oop.py }

Write-Host ""