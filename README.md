# Visual Studio Community C++ Console App

## Configuration

1. **Install OpenCV**
Download the pre-built OpenCV binaries and extract them into a folder of your choice (e.g., `C:\OpenCV\`).

2. **Include Directories**
Project Properties → **VC++ Directories → Include Directories**:

```
C:\OpenCV\opencv\build\include
```

3. **Library Directories**
Project Properties → **Linker → General → Additional Library Directories**:

```
C:\OpenCV\opencv\build\x64\vc16\lib
```

4. **Linker Input**
Project Properties → **Linker → Input → Additional Dependencies**:

```
opencv_world4120.lib       # Release build
opencv_world4120d.lib      # Debug build
```

5. **Runtime DLLs**
To run the project, copy the DLLs from ```C:\OpenCV\opencv\build\x64\vc16\bin``` into the same folder as your executable.
