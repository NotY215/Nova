# tests\clean.ps1 — remove test artifacts, stale exes, and nva build dirs.
# Does NOT touch the C++ build tree or the compiler exes (vcode/vayu/nva).

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $root

# --- Root-level temp files from --native invocations of vayuc.
Get-ChildItem -Path "." -File -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -match '^_vayu_' } |
    ForEach-Object { Write-Host ("rm " + $_.Name); Remove-Item -Force $_.FullName }

# --- Root-level .ssa / .s / .o test artifacts and their exes.
$stems = @(
    "cf", "oop",
    "vayu_self", "vayu_self2", "vcode_self", "vcode_self2",
    "oop_a", "oop_b", "oop_from_vcode", "vayu_oop",
    "bench_oop"
)
foreach ($stem in $stems) {
    foreach ($ext in @("ssa", "s", "o", "exe")) {
        $p = "$stem.$ext"
        if (Test-Path $p) {
            Write-Host ("rm " + $p)
            Remove-Item -Force $p
        }
    }
}

# --- temp runtime C source (should never survive a compile, but defensive).
$protectedFiles = @("vayu_rt.c")

Get-ChildItem -Path "." -File -Filter "*_rt.c" -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -notin $protectedFiles } |
    ForEach-Object { Write-Host ("rm " + $_.Name); Remove-Item -Force $_.FullName }

# --- build/ directories created by `nva build` inside example projects.
Get-ChildItem -Path "examples" -Recurse -Directory -Filter "build" -ErrorAction SilentlyContinue |
    ForEach-Object {
        Write-Host ("rmdir " + $_.FullName)
        Remove-Item -Recurse -Force $_.FullName
    }

Write-Host ""
Write-Host "Clean." -ForegroundColor Green