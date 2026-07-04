# Spine Godot 3D - Spine Runtime 4.0

This branch/tree is the Godot 4.3+ GDExtension build of the Spine Godot 3D runtime for Spine Runtime 4.0 data.

## Project Status

This is an unofficial community extension. It is not affiliated with, endorsed by, or maintained by Esoteric Software.

The project is still being tested. Branches are not marked as complete until they have been personally validated by the maintainer. Development may move slowly because this project is maintained by one person, but issues and feedback will be reviewed when time allows.

Current validation notes:

- Godot 4.3+ is the supported baseline.
- Build/export validation has currently been performed mainly on Windows and Android.
- Runtime/platform testing is still ongoing; Linux is not fully validated yet.
- `custom-material-3D` is still in development and may not behave identically across all render paths.
- The freshest fixes are expected to land first in the `Spine-4.2` branch.
- The `Spine-4.3` branch will be adjusted further after the official Spine 4.3 runtime branch settles upstream.

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

```text
GDScript_Build/example-v4-extension-Spine-4.0/
CSharp_Build/example-v4-csharp-Spine-4.0/
spine_godot/
build/
```

The C++ GDExtension is shared by both demo flavors. The selected demo flavor only controls which Godot project receives the built extension binaries.

## Building

Windows:

```powershell
.\build.bat -BuildFlavor gdscript -GodotVersion 4.5 -SpineVersion 4.0 -Platform windows
.\build.bat -BuildFlavor csharp -GodotVersion 4.5 -SpineVersion 4.0 -Platform windows
```

macOS/Linux shell:

```bash
./build.sh -BuildFlavor gdscript -GodotVersion 4.5 -SpineVersion 4.0 -Platform macos -Arch universal
./build.sh -BuildFlavor csharp -GodotVersion 4.5 -SpineVersion 4.0 -Platform macos -Arch universal
```

Configure dependencies without compiling:

```powershell
.\build.bat -BuildFlavor gdscript -GodotVersion 4.5 -SpineVersion 4.0 -Platform windows -ConfigureOnly
```

GodotVersion selects the official godot-cpp branch. SpineVersion selects the official spine-runtimes branch and must match this tree.

The build scripts clone or update godot-cpp and spine-runtimes automatically, then check out the selected branches. Generated folders such as `bin`, `dist`, `logs`, `.godot`, and C# temp output are intentionally not committed.

## Requirements

| Platform | Requirements |
| --- | --- |
| Windows | Visual Studio Build Tools, Python, SCons, Git |
| macOS/iOS | Xcode command line tools, Python 3, SCons, Git |
| Linux/Web | Godot/godot-cpp compatible C++ toolchain; Emscripten for Web |
| Android | Android SDK and NDK via ANDROID_HOME, ANDROID_SDK_ROOT, or Android Studio |
| C# | .NET SDK and Godot .NET editor for opening C# demo projects |

Android note: the build scripts prefer Android NDK `23.2.8568313` when it is installed, because that is the stable baseline tested with Godot 4.3 Android export templates. If that NDK is missing, the scripts fall back to another installed NDK, but other NDK versions are fallback-only and not release-validated yet. The extension is linked with static libc++ and 16 KB LOAD segment alignment to avoid NDK libc++ mismatches and Android 15+ page-size warnings for `libspine_godot`; Godot's own export-template libraries still depend on the selected Godot export templates.

The Extended/extra Spine runtime variant is not supported.

## Licensing

This repository contains modified Spine Runtime code and preserves the Spine Runtimes License and Esoteric Software copyright notices. This modified runtime package remains governed by the Spine Runtimes License.

This project is unofficial and is not affiliated with Esoteric Software. Users integrating this runtime in their own projects need an appropriate Spine Editor license from Esoteric Software, as required by the Spine Runtimes License.

See `LICENSE`, the official Spine Runtimes License, and the Spine Editor License for legal terms.
