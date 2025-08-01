# Build

```bash
Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
mkdir build
cd build
cmake -G "MinGW Makefiles" ..
cmake --build .
```

# Run

```bash
./exemple.exe
```