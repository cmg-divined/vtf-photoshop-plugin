# VTF Photoshop Plugin (v2)

This plugin adds support for Valve Texture Format (`.vtf`) files to Adobe Photoshop (64-bit).
It allows you to Open and Save VTF files directly, with full support for:
- DXT1, DXT5, RGBA8888, BGRA8888 formats
- Import/Export of Alpha Channels (as separate channels)
- Mipmap generation
- All standard VTF flags (Point Sample, Clamp, No LOD, etc.)
- **Version 7.2 Compliant** (80-byte header support for Source Engine / Garry's Mod)

## Prerequisites for Building

1.  **Visual Studio 2022** (Community Edition is fine)
    - Install the "Desktop development with C++" workload.
2.  **Adobe Photoshop SDK 2024** (or compatible)
    - You must have the SDK [downloaded](https://developer.adobe.com/console/4073574/servicesandapis/ps) and extracted somewhere on your computer.

## Build Instructions

1.  **Set Environment Variable**:
    You must set the `PHOTOSHOP_SDK_PATH` environment variable to point to your SDK folder.
    - Example: If your SDK is at `C:\Photoshop2021SDK`, set `PHOTOSHOP_SDK_PATH` to `C:\Photoshop2021SDK`.
    - The project expects to find `pluginsdk\samplecode\common\includes` inside this path.

2.  **Compile Resources (First Time Only)**:
    Run `build_resources.bat` in the root folder.
    - This script compiles the `src/VTFFormat.r` PiPL file into a resource file.
    - *Note:* This requires `Cnvtpipl.exe` from the Photoshop SDK to be in your path or located by the script. If the script fails, you may need to edit it to point to your SDK's `Cnvtpipl.exe`.

3.  **Open Solution**:
    Open `VTFFormat.sln` in Visual Studio 2022.

4.  **Build**:
    - Select **Release** configuration.
    - Select **x64** platform.
    - Build Solution.

5.  **Install**:
    Copy the output file (usually in `Output\x64\Release\VTFFormat.8bi`) to your Photoshop Plug-ins folder (e.g., `C:\Program Files\Adobe\Adobe Photoshop 2024\Plug-ins`).

## Project Structure

- `src/`: C++ source code and headers. `VTFPlugin.cpp` is the main entry point.
- `win/`: Visual Studio project files and Windows resources (`.rc`).
- `build_resources.bat`: Helper script to compile Photoshop PiPL resources.

## Credits

Based on Adobe Photoshop SDK samples.
VTF [wiki](https://developer.valvesoftware.com/wiki/VTF_(Valve_Texture_Format)) entry.

## Updates

Updated to add Alpha Channels, Flags, and Header compatibility.
