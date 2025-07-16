# Geruest Library Documentation Index

Welcome to the Geruest library documentation! This index helps you find the right documentation for your needs.

## 🚀 I Want to Get Started Quickly

**Start here if you just want to install and use Geruest:**

1. **[QUICK_START.md](QUICK_START.md)** - One-page guide with copy-paste commands
2. **[BUILD_SCRIPTS.md](BUILD_SCRIPTS.md)** - Collection of build scripts for different scenarios

## 📚 Complete Documentation

### Build and Installation
- **[BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md)** - Detailed build process explanation
- **[INSTALLATION_INSTRUCTIONS.md](INSTALLATION_INSTRUCTIONS.md)** - Comprehensive installation guide
- **[WIN_BUILD_INSTRUCTIONS.md](WIN_BUILD_INSTRUCTIONS.md)** - Windows-specific build instructions
- **[LINUX_BUILD_INSTRUCTIONS.md](LINUX_BUILD_INSTRUCTIONS.md)** - Linux-specific build instructions

### Platform-Specific Guides
- **[MSVC_BUILD_GUIDE.md](MSVC_BUILD_GUIDE.md)** - Microsoft Visual C++ build guide
- **[test_workflow.md](test_workflow.md)** - Testing workflow documentation

## 🎯 Quick Reference

### Installation Commands

**Linux/macOS (one-liner):**
```bash
curl -sSL https://raw.githubusercontent.com/UBehrmann/Geruest/main/install.sh | bash
```

**Windows PowerShell (one-liner):**
```powershell
iwr -useb https://raw.githubusercontent.com/UBehrmann/Geruest/main/install.ps1 | iex
```

**Manual installation:**
```bash
# Linux/macOS
git clone https://github.com/UBehrmann/Geruest.git && cd Geruest && mkdir build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . && sudo cmake --install .

# Windows (PowerShell)
git clone https://github.com/UBehrmann/Geruest.git; cd Geruest; mkdir build; cd build; cmake .. -A x64 -DCMAKE_BUILD_TYPE=Release; cmake --build . --config Release; cmake --install . --config Release
```

### Usage in Your Project

**CMake (recommended):**
```cmake
find_package(Geruest REQUIRED)
target_link_libraries(your_target Geruest::Geruest)
```

**Manual compilation:**
```bash
# Linux/macOS
g++ -std=c++17 your_file.cpp -lGeruest -lpthread -o your_app

# Windows
g++ -std=c++17 your_file.cpp -lGeruest -lws2_32 -o your_app.exe
```

## 🔍 Find What You Need

| I want to... | Go to... |
|-------------|----------|
| Install Geruest quickly | [QUICK_START.md](QUICK_START.md) |
| Get copy-paste build commands | [BUILD_SCRIPTS.md](BUILD_SCRIPTS.md) |
| Understand the build process | [BUILD_INSTRUCTIONS.md](BUILD_INSTRUCTIONS.md) |
| Install for production use | [INSTALLATION_INSTRUCTIONS.md](INSTALLATION_INSTRUCTIONS.md) |
| Build on Windows with Visual Studio | [WIN_BUILD_INSTRUCTIONS.md](WIN_BUILD_INSTRUCTIONS.md) |
| Build on Linux | [LINUX_BUILD_INSTRUCTIONS.md](LINUX_BUILD_INSTRUCTIONS.md) |
| Use MSVC specifically | [MSVC_BUILD_GUIDE.md](MSVC_BUILD_GUIDE.md) |
| Run tests | [test_workflow.md](test_workflow.md) |

## 🆘 Getting Help

### Common Issues
1. **Permission denied** - Use local installation or run as admin/sudo
2. **Library not found** - Set `CMAKE_PREFIX_PATH` to installation directory
3. **Compiler errors** - Ensure C++17 support and proper linking flags

### Support Resources
- **GitHub Issues**: Report bugs or ask questions
- **Documentation**: Check the relevant guide above
- **Example Code**: See `exemple/exemple.cpp` for usage examples

## 🎉 Success! What's Next?

After successful installation:
1. ✅ **Verify** - Run the test code from [QUICK_START.md](QUICK_START.md)
2. 📖 **Learn** - Check the example in `exemple/exemple.cpp`
3. 🛠️ **Build** - Start your web application project
4. 🚀 **Deploy** - Use your preferred deployment method

---

**Quick Links:**
- [Main Repository](https://github.com/UBehrmann/Geruest)
- [Quick Start](QUICK_START.md)
- [Build Scripts](BUILD_SCRIPTS.md)
- [Example Code](../exemple/exemple.cpp)
