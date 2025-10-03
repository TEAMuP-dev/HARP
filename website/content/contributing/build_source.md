# Building HARP from Source

HARP can be built from scratch with the following steps:

### 1. Clone Repository
```bash
git clone --recurse-submodules https://github.com/TEAMuP-dev/HARP
```

### 2. Enter Project
```bash
cd HARP/
```

### 3. Configure
```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

#### ARM vs. x86 MacOS
The OSX architecture for the build can be specified explicitly by setting `CMAKE_OSX_ARCHITECTURES` to either `arm64` or `x86_64`:
```bash
cmake .. -DCMAKE_OSX_ARCHITECTURES=x86_64
```

#### Linux
Ensure your system satisfies all [JUCE dependencies](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md).

### 4. Build
#### MacOS/Linux
```bash
make -j <NUM_PROCESSORS>
```

#### Windows
```bash
cmake --build . --config Debug -j <NUM_PROCESSORS>
```
