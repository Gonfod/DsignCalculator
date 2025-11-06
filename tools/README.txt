Tools to automate copying SFML DLLs into the project output and running the executable.

Usage:
- Set environment variable SFML_BIN to your SFML bin folder (e.g., C:\SFML\bin) OR edit tools/copy_sfml_dlls.ps1 default.
- Run tools/run_with_dlls.bat [Configuration] [Platform]
 e.g., tools\run_with_dlls.bat Debug x64

What it does:
- Copies all sfml-*.dll files from SFML bin into the project's output folder
- Runs the built executable

You can also call the PowerShell script directly:
powershell -ExecutionPolicy Bypass -File tools\copy_sfml_dlls.ps1 -SfmlBin "C:\Path\To\SFML\bin" -OutDir "path\\to\\output"
