The script below downloads *libffmpeg*, unpacks it, copies the ffmpeg *dll* and builds `libffpp.dll`.

If you want to do one step at a time, read it, select the step and run:

> `GetAndBuild.ps1`

```powershell
# Get 7-zip
Invoke-WebRequest -Uri "https://www.7-zip.org/a/7zr.exe" -OutFile "7zr.exe"

# Get ffmpeg package
Invoke-WebRequest -Uri "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-full-shared.7z" -OutFile "libwin-ffmpeg.7z"

# Unzip ffmpeg
.\7zr.exe x "libwin-ffmpeg.7z" -o"temp-extract"

# Remove sub folder
Move-Item -Path "temp-extract\ffmpeg-7.1.1-full_build-shared\*" -Destination "libs-ffmpeg"
Remove-Item -Recurse -Force "temp-extract"

# Copy ffmpeg dll
Copy-Item -Path "C:\Users\$env:USERNAME\Desktop\FILES\ProjetoFFPP\libs-ffmpeg\*.dll" -Destination .

# Make libffpp.dll
clang++ -I.\libs-ffmpeg\include -c -fPIC -I. ffpp.cpp -o ffpp.o
clang++ -I.\libs-ffmpeg\include -c -fPIC -I. api.cpp -o api.o
clang++ -I.\libs-ffmpeg\include -L.\libs-ffmpeg\lib -shared -fPIC ffpp.o api.o -o libffpp.dll -lavcodec -lavformat -lavutil -lswscale
```

Then just create an example and compile:
```powershell
clang++ -I.\libs-ffmpeg\include -I. -L.\libs-ffmpeg\lib -L. .\main.cpp -lffpp -lavcodec -lavformat -lavutil -lswscale -o main.exe
```
