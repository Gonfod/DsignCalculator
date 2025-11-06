param(
 [string]$SfmlBin = $env:SFML_BIN,
 [string]$OutDir = "$PWD"
)

if (-not $SfmlBin) {
 # Default common location — change if needed
 $SfmlBin = "C:\\SFML\\bin"
}

if (-not (Test-Path $SfmlBin)) {
 Write-Error "SFML bin folder not found: $SfmlBin. Set SFML_BIN environment variable or pass -SfmlBin parameter."
 exit1
}

if (-not (Test-Path $OutDir)) {
 New-Item -ItemType Directory -Path $OutDir | Out-Null
}

$files = Get-ChildItem -Path $SfmlBin -Filter "sfml-*.dll" -File -ErrorAction SilentlyContinue
if (-not $files) {
 Write-Error "No SFML DLLs found in $SfmlBin"
 exit1
}

foreach ($f in $files) {
 $dest = Join-Path $OutDir $f.Name
 Copy-Item -Path $f.FullName -Destination $dest -Force
 Write-Host "Copied $($f.Name) -> $OutDir"
}

Write-Host "Done. Copied $($files.Count) DLL(s) from $SfmlBin to $OutDir"