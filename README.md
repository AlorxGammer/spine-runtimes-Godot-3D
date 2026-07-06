# Spine Godot 3D - Spine Runtime 4.0

This branch/tree is the Godot 4.3+ GDExtension build of the Spine Godot 3D runtime for Spine Runtime 4.0 data.

![Spine Godot 3D preview](docs/Scene.gif)

## Project Status

This is an unofficial community extension. It is not affiliated with, endorsed by, or maintained by Esoteric Software.

The project is still being tested. Branches are not marked as complete until they have been personally validated by the maintainer. Development may move slowly because this project is maintained by one person, but issues and feedback will be reviewed when time allows.

Current validation notes:

- Godot 4.3+ is the supported baseline.
- Build/export validation has currently been performed mainly on Windows and Android.
- Runtime/platform testing is still ongoing; Linux is not fully validated yet.
- `custom-material-3D` now uses the Spine 3D material adapter for lighting, slot textures, alpha cutoff, and double-sided shader injection.
- The `SpineSprite3D.double_sided` GPU view-stack rendering fix has been ported to all maintained Spine runtime trees.
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

## 3D Double-Sided Rendering

`SpineSprite3D` includes a `double_sided` option for true two-sided 3D rendering. When enabled, the runtime keeps the normal front-side Spine slot order and also renders the back side with the inverse stack order, so the character remains assembled correctly from both sides instead of relying on Godot material culling alone.

The slot stack depth is now sent to the shader and offset per render pass on the GPU. This removes the old CPU dependency on a selected camera for slot assembly, so mirrors, SubViewports, editor cameras, and multiple runtime cameras can render the same object with the correct local stack direction in their own pass.

Generated and runtime custom shaders also handle backface lighting by flipping the normal and binormal on back faces. This keeps normal maps and 2D-lighting-style materials readable on the rear side when `double_sided` is enabled.

## 3D Custom Materials

`SpineSprite3D` and `SpineSlotNode3D` can use custom `Material` resources per Spine blend mode, similar to the 2D runtime. In 3D, the runtime adapts material copies per slot so custom materials still receive the correct atlas texture, vertex color, render priority, Spine blend mode, alpha cutoff, lighting mode, optional normal map, and double-sided view-stack code.

Custom spatial shaders should sample `spine_texture` or a common alias such as `texture_albedo`, `albedo_texture`, or `diffuse_texture`. The demo `new_shader.gdshader` files use tint/saturation effects that keep all RGB channels intact, so colored lights remain readable.

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
