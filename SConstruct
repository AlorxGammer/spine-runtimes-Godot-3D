#!/usr/bin/env python
import os
import subprocess
import sys

from methods import print_error


SUPPORTED_SPINE_RUNTIMES = ("4.2",)
SUPPORTED_GODOT_VERSIONS = ("4.3", "4.4", "4.5")
SUPPORTED_DEMO_FLAVORS = ("gdscript", "csharp")
LIBRARY_NAME = "spine_godot"
DEMO_PROJECT_ROOTS = {
    "gdscript": ("GDScript_Build", "example-v4-extension-Spine-"),
    "csharp": ("CSharp_Build", "example-v4-csharp-Spine-"),
}

PREFERRED_ANDROID_NDK = "23.2.8568313"


def normalize_path(value, env):
    return value if os.path.isabs(value) else os.path.join(env.Dir("#").abspath, value)


def validate_parent_dir(key, value, env):
    parent = os.path.dirname(value)
    if not os.path.isdir(normalize_path(parent, env)):
        raise UserError("'%s' is not a directory: %s" % (key, parent))


def write_demo_extension_list(target, source, env):
    extension_list = str(target[0])
    os.makedirs(os.path.dirname(extension_list), exist_ok=True)
    with open(extension_list, "w", newline="\n") as f:
        f.write("res://bin/spine_godot_extension.gdextension\n")


local_env = Environment(tools=["default"], PLATFORM="")
customs = []

options = Variables(customs, ARGUMENTS)
options.Add(
    BoolVariable(
        key="compiledb",
        help="Generate compile_commands.json for external tools",
        default=local_env.get("compiledb", False),
    )
)
options.Add(
    PathVariable(
        key="compiledb_file",
        help="Path to a custom compile_commands.json file",
        default=local_env.get("compiledb_file", "compile_commands.json"),
        validator=validate_parent_dir,
    )
)
options.Add(
    EnumVariable(
        key="godot_version",
        help="Godot/godot-cpp branch version",
        default=local_env.get("godot_version", "4.5"),
        allowed_values=SUPPORTED_GODOT_VERSIONS,
    )
)
options.Add(
    EnumVariable(
        key="spine_runtime",
        help="Spine runtime version to compile (4.2 only in this tree)",
        default=local_env.get("spine_runtime", "4.2"),
        allowed_values=SUPPORTED_SPINE_RUNTIMES,
    )
)
options.Add(
    EnumVariable(
        key="demo_flavor",
        help="Demo project flavor to install into (gdscript or csharp)",
        default=local_env.get("demo_flavor", "gdscript"),
        allowed_values=SUPPORTED_DEMO_FLAVORS,
    )
)
options.Update(local_env)
Help(options.GenerateHelpText(local_env))

spine_runtime = local_env["spine_runtime"]
godot_version = local_env["godot_version"]
demo_flavor = local_env["demo_flavor"]
install_demo = True
runtime_key = spine_runtime.replace(".", "")
if spine_runtime == "4.3":
    runtime_dir_candidates = (
        "spine-runtimes/spine-cpp",
        "spine-runtimes/spine-cpp/spine-cpp",
    )
else:
    runtime_dir_candidates = (
        "spine-runtimes/spine-cpp/spine-cpp",
        "spine-runtimes/spine-cpp",
    )
runtime_dir = next(
    (
        candidate
        for candidate in runtime_dir_candidates
        if os.path.isdir(candidate + "/include") and os.path.isdir(candidate + "/src/spine")
    ),
    runtime_dir_candidates[0],
)
package_dir = "dist/godot-{}/spine-{}/spine_godot".format(godot_version, spine_runtime)
demo_project_root, demo_project_prefix = DEMO_PROJECT_ROOTS[demo_flavor]
demo_project_dir = "{}/{}{}".format(demo_project_root, demo_project_prefix, spine_runtime)

if install_demo and not os.path.isfile(os.path.join(demo_project_dir, "project.godot")):
    print(
        "WARNING: Godot demo project is missing for Spine {}: {}. "
        "Continuing with the versioned package only.".format(
            spine_runtime, demo_project_dir
        )
    )
    install_demo = False

for required_dir in (runtime_dir + "/include", runtime_dir + "/src/spine"):
    if not os.path.isdir(required_dir):
        print_error("Missing Spine {} runtime directory: {}".format(spine_runtime, required_dir))
        sys.exit(1)

if not os.path.isdir("godot-cpp") or not os.listdir("godot-cpp"):
    print_error(
        "godot-cpp is missing. Run build.bat or build.sh and select a supported Godot version first."
    )
    sys.exit(1)

env = local_env.Clone()
env["compiledb"] = False
selected_platform = ARGUMENTS.get("platform", local_env.get("platform", ""))
if selected_platform == "android":
    default_android_home = os.path.join(
        os.environ.get("LOCALAPPDATA", ""), "Android", "Sdk"
    )
    detected_android_home = os.environ.get(
        "ANDROID_HOME",
        os.environ.get("ANDROID_SDK_ROOT", local_env.get("ANDROID_HOME", "")),
    )
    if not detected_android_home and os.path.isdir(default_android_home):
        detected_android_home = default_android_home
    android_ndk_root = os.environ.get("ANDROID_NDK_ROOT", "")
    if not android_ndk_root and detected_android_home:
        ndk_dir = os.path.join(detected_android_home, "ndk")
        if os.path.isdir(ndk_dir):
            installed_ndks = sorted(
                name
                for name in os.listdir(ndk_dir)
                if os.path.isdir(os.path.join(ndk_dir, name))
            )
            if installed_ndks:
                selected_ndk = (
                    PREFERRED_ANDROID_NDK
                    if PREFERRED_ANDROID_NDK in installed_ndks
                    else installed_ndks[-1]
                )
                android_ndk_root = os.path.join(ndk_dir, selected_ndk)
                os.environ["ANDROID_NDK_ROOT"] = android_ndk_root
    if android_ndk_root:
        # godot-cpp 4.x hardcodes an NDK version when ANDROID_HOME is set.
        # Prefer an explicit NDK root so side-by-side installed NDK versions work.
        selected_ndk = os.path.basename(os.path.normpath(android_ndk_root))
        if selected_ndk:
            env["ndk_version"] = selected_ndk
            ARGUMENTS["ndk_version"] = selected_ndk
        os.environ["ANDROID_NDK_ROOT"] = android_ndk_root
        os.environ.pop("ANDROID_HOME", None)
        os.environ.pop("ANDROID_SDK_ROOT", None)
        ARGUMENTS["ANDROID_HOME"] = ""
        env["ANDROID_HOME"] = ""
    else:
        env["ANDROID_HOME"] = os.environ.get(
            "ANDROID_HOME", os.environ.get("ANDROID_SDK_ROOT", detected_android_home)
        )

# godot-cpp does not know this project-specific option.
ARGUMENTS.pop("spine_runtime", None)
ARGUMENTS.pop("godot_version", None)
ARGUMENTS.pop("demo_flavor", None)

env.Tool("compilation_db")
compilation_db = env.CompilationDatabase(
    normalize_path(local_env["compiledb_file"], local_env)
)
env.Alias("compiledb", compilation_db)

env = SConscript("godot-cpp/SConstruct", {"env": env, "customs": customs}).Clone()
env.Append(CPPDEFINES=["SPINE_GODOT_EXTENSION"])
env.Append(CPPDEFINES=["SPINE_RUNTIME_{}".format(runtime_key)])
if spine_runtime != "3.8":
    env.Append(CPPDEFINES=["SPINE_RUNTIME_4_PLUS"])
if spine_runtime in ("4.1", "4.2", "4.3"):
    env.Append(CPPDEFINES=["SPINE_RUNTIME_41_PLUS"])
if spine_runtime in ("4.2", "4.3"):
    env.Append(CPPDEFINES=["SPINE_RUNTIME_42_PLUS"])
if spine_runtime == "4.3":
    env.Append(CPPPATH=[runtime_dir + "/include", "spine_godot"])
else:
    env.Append(CPPPATH=["spine_godot", runtime_dir + "/include"])

if env["platform"] == "ios":
    env.Append(CCFLAGS=["-miphoneos-version-min=12.0"])
    env.Append(LINKFLAGS=["-miphoneos-version-min=12.0"])

if env["platform"] == "android":
    # Android 15+ devices can use 16 KB memory pages. Align our shared
    # library LOAD segments so the extension does not trigger compatibility
    # mode warnings on those devices. Static libc++ also avoids depending on
    # the libc++_shared.so bundled by the selected Godot export template.
    env.AppendUnique(LINKFLAGS=["-static-libstdc++", "-Wl,-z,max-page-size=16384"])

sources = Glob(runtime_dir + "/src/spine/*.cpp")
extension_sources = [
    "GodotSpineExtension.cpp",
    "SpineAnimation.cpp",
    "SpineAnimationState.cpp",
    "SpineAnimationTrack.cpp",
    "SpineAtlasResource.cpp",
    "SpineAttachment.cpp",
    "SpineBone.cpp",
    "SpineBoneData.cpp",
    "SpineBoneNode.cpp",
    "SpineBoneNode3D.cpp",
    "SpineConstant.cpp",
    "SpineConstraintData.cpp",
    "SpineEditorPlugin.cpp",
    "SpineEvent.cpp",
    "SpineEventData.cpp",
    "SpineIkConstraint.cpp",
    "SpineIkConstraintData.cpp",
    "SpinePathConstraint.cpp",
    "SpinePathConstraintData.cpp",
    "SpineRenderWorld3D.cpp",
    "SpineSkeleton.cpp",
    "SpineSkeletonDataResource.cpp",
    "SpineSkeletonFileResource.cpp",
    "SpineSkin.cpp",
    "SpineSlot.cpp",
    "SpineSlotData.cpp",
    "SpineSlotNode.cpp",
    "SpineSlotNode3D.cpp",
    "SpineSprite.cpp",
    "SpineSprite3D.cpp",
    "SpineTimeline.cpp",
    "SpineTrackEntry.cpp",
    "SpineTransformConstraint.cpp",
    "SpineTransformConstraintData.cpp",
    "register_types.cpp",
]

if spine_runtime in ("4.2", "4.3"):
    extension_sources += ["SpinePhysicsConstraint.cpp", "SpinePhysicsConstraintData.cpp"]
if spine_runtime == "4.3":
    extension_sources += [
        "SpineBoneLocal.cpp",
        "SpineBonePose.cpp",
        "SpineIkConstraintPose.cpp",
        "SpinePathConstraintPose.cpp",
        "SpinePhysicsConstraintPose.cpp",
        "SpineSlider.cpp",
        "SpineSliderData.cpp",
        "SpineSliderPose.cpp",
        "SpineSlotPose.cpp",
        "SpineTransformConstraintPose.cpp",
    ]

sources += ["spine_godot/" + source for source in extension_sources]

if env["target"] in ("editor", "template_debug"):
    try:
        doc_data = env.GodotCPPDocData(
            "src/gen/doc_data.gen.cpp", source=Glob("spine_godot/docs/*.xml")
        )
        sources.append(doc_data)
    except AttributeError:
        print("Not including class reference for a pre-4.3 godot-cpp baseline.")

# Plugin objects differ between Spine versions and must never be reused.
env["OBJPREFIX"] = "rt{}_".format(runtime_key)

library_file_name = "lib{}{}{}".format(LIBRARY_NAME, env["suffix"], env["SHLIBSUFFIX"])
framework_path = ""
plist_file = None

if env["platform"] in ("macos", "ios"):
    framework_path = "lib{}.{}.{}.framework/".format(
        LIBRARY_NAME, env["platform"], env["target"]
    )
    library_file_name = "lib{}.{}.{}".format(LIBRARY_NAME, env["platform"], env["target"])
    env.Append(LINKFLAGS=["-Wl,-install_name,@rpath/{}{}".format(framework_path, library_file_name)])

build_library_path = "bin/godot-{}/spine-{}/{}/{}{}".format(
    godot_version, spine_runtime, env["platform"], framework_path, library_file_name
)
package_library_path = "bin/{}/{}{}".format(
    env["platform"], framework_path, library_file_name
)
library = env.SharedLibrary(build_library_path, source=sources)

default_targets = [
    library,
    env.InstallAs("{}/{}".format(package_dir, package_library_path), library),
    env.InstallAs(
        "{}/bin/spine_godot_extension.gdextension".format(package_dir),
        "spine_godot_extension.gdextension",
    ),
]
default_targets += env.Install(
    "{}/icons".format(package_dir), Glob("spine_godot/icons/*.svg")
)

if install_demo:
    default_targets += [
        env.Command(
            "{}/.godot/extension_list.cfg".format(demo_project_dir),
            [],
            write_demo_extension_list,
        )
    ]
    default_targets += [
        env.InstallAs("{}/{}".format(demo_project_dir, package_library_path), library),
        env.InstallAs(
            "{}/bin/spine_godot_extension.gdextension".format(demo_project_dir),
            "spine_godot_extension.gdextension",
        ),
    ]
    default_targets += env.Install(
        "{}/spine_godot/icons".format(demo_project_dir),
        Glob("spine_godot/icons/*.svg"),
    )

if env["platform"] in ("macos", "ios"):
    plist_values = {
        "${BUNDLE_LIBRARY}": library_file_name,
        "${BUNDLE_NAME}": "spine-godot",
        "${BUNDLE_IDENTIFIER}": "com.esotericsoftware.spine.spine-godot",
        "${BUNDLE_VERSION}": subprocess.check_output(
            ["git", "rev-parse", "--abbrev-ref", "HEAD"]
        ).decode("utf-8").strip().split("-")[0] + ".0",
        "${MIN_MACOS_VERSION}": "10.12",
        "${MIN_IOS_VERSION}": "12.0",
    }

    if env["platform"] == "macos":
        plist_file = "bin/godot-{}/spine-{}/macos/{}Resources/Info.plist".format(
            godot_version, spine_runtime, framework_path
        )
        plist = env.Substfile(plist_file, "Info.macos.plist", SUBST_DICT=plist_values)
        package_plist = "bin/macos/{}Resources/Info.plist".format(framework_path)
    else:
        plist_file = "bin/godot-{}/spine-{}/ios/{}Info.plist".format(
            godot_version, spine_runtime, framework_path
        )
        plist = env.Substfile(plist_file, "Info.ios.plist", SUBST_DICT=plist_values)
        package_plist = "bin/ios/{}Info.plist".format(framework_path)

    env.Depends(library, plist)
    default_targets.append(env.InstallAs("{}/{}".format(package_dir, package_plist), plist_file))
    if install_demo:
        default_targets.append(
            env.InstallAs("{}/{}".format(demo_project_dir, package_plist), plist_file)
        )

if local_env.get("compiledb", False):
    default_targets.append(compilation_db)

Default(*default_targets)
