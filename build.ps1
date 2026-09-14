$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$output = Join-Path $root 'NineTick.exe'

& 'C:\MinGW\bin\windres.exe' --input "$root\NineTick.rc" --output "$root\NineTick.res" --output-format=coff
if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
& 'C:\MinGW\bin\g++.exe' -std=c++17 -O2 -s -static-libgcc -mwindows "$root\NineTick.cpp" "$root\NineTick.res" -o $output -lshell32 -ladvapi32 -luser32 -lgdi32
if ($LASTEXITCODE -ne 0) { throw 'C++ compilation failed.' }
Remove-Item "$root\NineTick.res" -ErrorAction SilentlyContinue

$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut((Join-Path $root 'NineTick - Try Me.lnk'))
$shortcut.TargetPath = $output
$shortcut.WorkingDirectory = $root
$shortcut.IconLocation = "$output,0"
$shortcut.Description = 'Start the lightweight NineTick timer'
$shortcut.Save()

Write-Host "Built: $output"
Write-Host "Shortcut: $(Join-Path $root 'NineTick - Try Me.lnk')"
