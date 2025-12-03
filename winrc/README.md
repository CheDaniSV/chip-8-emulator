This folder contains .rc - windows resources files, used as metadata during compilation.

Compiling using `windres`:
- `windres raylib.rc -O coff -o app.res`

Linking with g++:
- `g++ -std=c++17 main.cpp raylib.re.data -o main.exe -IC:/raylib/raylib/src -LC:/raylib/raylib/src -lraylib -lopengl32 -ldgi32 -lwinmm -g -Wl,--subsystem,windows`