# Spine Godot 3D - Spine Runtime 4.0

This branch/tree is the Godot 4.3+ GDExtension build of the Spine Godot 3D runtime for Spine Runtime 4.0 data.

## Support Matrix

| Area | Supported |
| --- | --- |
| Godot editor/launcher | 4.3, 4.4, 4.5+ compatible launchers |
| godot-cpp build branches | 4.3, 4.4, 4.5 |
| Spine Runtime branch | 4.0 only in this tree |
| GDScript exports | Windows, Linux, macOS, Android, iOS, Web |
| C# exports | Windows, Linux, macOS, Android, iOS |
| C# Web | Not supported by Godot 4 C# |

Godot/godot-cpp 4.1 and 4.2 are intentionally not supported in this release layout. Spine Runtime 4.2 remains a valid Spine data/runtime version in its own Spine-4.2 tree; that is separate from Godot 4.2 support.

## Layout

`	ext
GDScript_Build/example-v4-extension-Spine-4.0/
CSharp_Build/example-v4-csharp-Spine-4.0/
spine_godot/
build/
`

The C++ GDExtension is shared by both demo flavors. The selected demo flavor only controls which Godot project receives the built extension binaries.

## Building

Windows:

`powershell
.\build.bat -BuildFlavor gdscript -GodotVersion 4.5 -SpineVersion 4.0 -Platform windows
.\build.bat -BuildFlavor csharp -GodotVersion 4.5 -SpineVersion 4.0 -Platform windows
`

macOS/Linux shell:

`ash
./build.sh -BuildFlavor gdscript -GodotVersion 4.5 -SpineVersion 4.0 -Platform macos -Arch universal
./build.sh -BuildFlavor csharp -GodotVersion 4.5 -SpineVersion 4.0 -Platform macos -Arch universal
`

Configure dependencies without compiling:

`powershell
.\build.bat -BuildFlavor gdscript -GodotVersion 4.5 -SpineVersion 4.0 -Platform windows -ConfigureOnly
`

GodotVersion selects the official godot-cpp branch. SpineVersion selects the official spine-runtimes branch and must match this tree.

The build scripts clone or update godot-cpp and spine-runtimes automatically, then check out the selected branches. Generated folders such as in, dist, logs, .godot, and C# temp output are intentionally not committed.

## Requirements

| Platform | Requirements |
| --- | --- |
| Windows | Visual Studio Build Tools, Python, SCons, Git |
| macOS/iOS | Xcode command line tools, Python 3, SCons, Git |
| Linux/Web | Godot/godot-cpp compatible C++ toolchain; Emscripten for Web |
| Android | Android SDK and NDK via ANDROID_HOME, ANDROID_SDK_ROOT, or Android Studio |
| C# | .NET SDK and Godot .NET editor for opening C# demo projects |

The Extended/extra Spine runtime variant is not supported.

## Licensing

This project contains modified Spine Runtime code and preserves the Spine Runtimes License and copyright notices. Users integrating the runtime in their own projects need an appropriate Spine Editor license from Esoteric Software.

See LICENSE and the official Spine Runtimes License for legal terms.