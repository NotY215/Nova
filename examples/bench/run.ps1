# Vayu / C++ / Python benchmark harness.
# From E:\Vayu:
#   powershell -ExecutionPolicy Bypass -File examples\bench\run.ps1

$ErrorActionPreference = "Stop"
$vayuc = "build\x64-release\bin\vayuc.exe"
$tmp   = "$env:TEMP\vayu_bench"

if (-not (Test-Path $tmp)) { New-Item -ItemType Directory -Path $tmp | Out-Null }

function Time-It($label, [scriptblock]$body) {
    # warm-up (JIT, file cache, etc.)
    $null = Measure-Command { & $body | Out-Null }
    $ms = (Measure-Command { & $body | Out-Null }).TotalMilliseconds
    "{0,-22} {1,10:F1} ms" -f $label, $ms
}

function Ensure-Cpp($src, $exe) {
    if (-not (Test-Path $exe) -or
        (Get-Item $src).LastWriteTime -gt (Get-Item $exe).LastWriteTime) {
        Write-Host "  (compiling $src)" -ForegroundColor DarkGray
        g++ -O2 $src -o $exe
    }
}

function Compile-VayuNative($vyu, $exe) {
    Write-Host "  (compiling $vyu -> $exe)" -ForegroundColor DarkGray
    & $vayuc $vyu --native-out $exe
    if (-not (Test-Path $exe)) {
        throw "Vayu native compile failed for $vyu"
    }
}

# ===========================================================================
Write-Host "`n================ fib(30) ================`n"

$vyuFib = "examples\bench\native_fib.vyu"
$exeFib = "$tmp\fib.exe"
Compile-VayuNative $vyuFib $exeFib
Time-It "Vayu native"  { & $exeFib }

Ensure-Cpp "examples\bench\cpp\fib.cpp" "$tmp\fib_cpp.exe"
Time-It "C++ g++ -O2"  { & "$tmp\fib_cpp.exe" }

Time-It "Python 3"     { python examples\bench\py\fib.py }

# ===========================================================================
Write-Host "`n================ 50M loop ================`n"

$vyuLoop = "examples\bench\native_loop.vyu"
$exeLoop = "$tmp\loop.exe"
Compile-VayuNative $vyuLoop $exeLoop
Time-It "Vayu native"  { & $exeLoop }

Ensure-Cpp "examples\bench\cpp\loop.cpp" "$tmp\loop_cpp.exe"
Time-It "C++ g++ -O2"  { & "$tmp\loop_cpp.exe" }

Time-It "Python 3"     { python examples\bench\py\loop.py }

# ===========================================================================
Write-Host "`n================ 5M oop =================`n"

$vyuOop = "examples\bench\oop.vyu"
$exeOop = "$tmp\oop.exe"
Compile-VayuNative $vyuOop $exeOop
Time-It "Vayu native"  { & $exeOop }

Ensure-Cpp "examples\bench\cpp\oop.cpp" "$tmp\oop_cpp.exe"
Time-It "C++ g++ -O2"  { & "$tmp\oop_cpp.exe" }

Time-It "Python 3"     { python examples\bench\py\oop.py }

Write-Host ""