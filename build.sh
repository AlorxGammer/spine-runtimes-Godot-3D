#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR"
VERSIONS_FILE="$ROOT/build/versions.json"
GODOT_CPP_PATH="$ROOT/godot-cpp"
SPINE_RUNTIMES_PATH="$ROOT/spine-runtimes"
LOG_DIR="$ROOT/logs"

BUILD_FLAVOR=""
GODOT_VERSION=""
SPINE_VERSION=""
PLATFORM=""
ARCH=""
WEB_PRESET="auto"
JOBS=""
CONFIGURE_ONLY=0
LOG_FILE=""
EXIT_MESSAGE_WRITTEN=0

usage() {
    cat <<'EOF'
Usage:
  ./build.sh [-BuildFlavor gdscript|csharp] [-GodotVersion 4.3|4.4|4.5]
             [-SpineVersion 4.3|all] [-Platform macos|ios|android|linux|web|windows]
             [-Arch universal|arm64|x86_64|rv64|wasm32] [-WebPreset auto|threads|nothreads]
             [-Jobs N] [-ConfigureOnly]

Examples:
  ./build.sh -BuildFlavor gdscript -GodotVersion 4.5 -SpineVersion 4.3 -Platform macos -Arch universal
  ./build.sh -BuildFlavor gdscript -GodotVersion 4.5 -SpineVersion all -Platform ios -Arch arm64
  ./build.sh -BuildFlavor csharp -GodotVersion 4.5 -SpineVersion 4.3 -Platform macos -Arch universal
EOF
}

while (($#)); do
    case "$1" in
        -BuildFlavor|--build-flavor|--buildFlavor)
            BUILD_FLAVOR="${2:-}"; shift 2 ;;
        -GodotVersion|--godot-version|--godotVersion)
            GODOT_VERSION="${2:-}"; shift 2 ;;
        -SpineVersion|--spine-version|--spineVersion)
            SPINE_VERSION="${2:-}"; shift 2 ;;
        -Platform|--platform)
            PLATFORM="${2:-}"; shift 2 ;;
        -Arch|--arch)
            ARCH="${2:-}"; shift 2 ;;
        -WebPreset|--web-preset|--webPreset)
            WEB_PRESET="${2:-}"; shift 2 ;;
        -Jobs|--jobs|-j)
            JOBS="${2:-}"; shift 2 ;;
        -ConfigureOnly|--configure-only|--configureOnly)
            CONFIGURE_ONLY=1; shift ;;
        -h|--help)
            usage; exit 0 ;;
        *)
            echo "Unknown argument: $1" >&2
            usage >&2
            exit 2 ;;
    esac
done

if [[ ! -f "$VERSIONS_FILE" ]]; then
    echo "Missing versions file: $VERSIONS_FILE" >&2
    exit 1
fi

require_command() {
    local name="$1"
    local hint="${2:-}"
    if ! command -v "$name" >/dev/null 2>&1; then
        if [[ -n "$hint" ]]; then
            echo "$name was not found. $hint" >&2
        else
            echo "$name was not found in PATH." >&2
        fi
        exit 1
    fi
}

json_value() {
    python3 - "$VERSIONS_FILE" "$@" <<'PY'
import json
import sys

path = sys.argv[1]
query = sys.argv[2:]
with open(path, "r", encoding="utf-8") as file:
    data = json.load(file)

if query[0] == "default":
    print(data["defaults"][query[1]])
elif query[0] == "versions":
    print("\n".join(item["version"] for item in data[query[1]]))
elif query[0] == "ref":
    group, version = query[1], query[2]
    for item in data[group]:
        if item["version"] == version:
            print(item["ref"])
            break
    else:
        raise SystemExit(1)
else:
    raise SystemExit(2)
PY
}

contains_value() {
    local needle="$1"; shift
    local item
    for item in "$@"; do
        [[ "$item" == "$needle" ]] && return 0
    done
    return 1
}

read_versions() {
    GODOT_VERSIONS=()
    while IFS= read -r version; do
        GODOT_VERSIONS+=("$version")
    done < <(json_value versions godot)
    SPINE_VERSIONS=()
    while IFS= read -r version; do
        SPINE_VERSIONS+=("$version")
    done < <(json_value versions spine)
    DEFAULT_GODOT="$(json_value default godot)"
    DEFAULT_SPINE="$(json_value default spine)"
}

select_version() {
    local title="$1"
    local default_value="$2"
    local allow_all="$3"
    shift 3
    local items=("$@")
    local answer selection max

    echo >&2
    echo "$title" >&2
    if [[ "$allow_all" == "yes" ]]; then
        echo "  0. All supported versions" >&2
    fi
    for i in "${!items[@]}"; do
        local suffix=""
        [[ "${items[$i]}" == "$default_value" ]] && suffix=" [default]"
        printf '  %d. %s%s\n' "$((i + 1))" "${items[$i]}" "$suffix" >&2
    done

    while true; do
        max="${#items[@]}"
        if [[ "$allow_all" == "yes" ]]; then
            printf 'Select 0-%s, or press Enter for %s: ' "$max" "$default_value" >&2
        else
            printf 'Select 1-%s, or press Enter for %s: ' "$max" "$default_value" >&2
        fi
        read -r answer
        [[ -z "$answer" ]] && { echo "$default_value"; return; }
        if [[ "$allow_all" == "yes" && "$answer" == "0" ]]; then
            echo "all"; return
        fi
        if [[ "$answer" =~ ^[0-9]+$ ]]; then
            selection="$answer"
            if (( selection >= 1 && selection <= ${#items[@]} )); then
                echo "${items[$((selection - 1))]}"
                return
            fi
        fi
        echo "Invalid selection." >&2
    done
}

select_option() {
    local title="$1"
    local default_value="$2"
    shift 2
    local items=("$@")
    local answer selection

    echo >&2
    echo "$title" >&2
    for i in "${!items[@]}"; do
        local suffix=""
        [[ "${items[$i]}" == "$default_value" ]] && suffix=" [default]"
        printf '  %d. %s%s\n' "$((i + 1))" "${items[$i]}" "$suffix" >&2
    done

    while true; do
        printf 'Select 1-%s, or press Enter for %s: ' "${#items[@]}" "$default_value" >&2
        read -r answer
        [[ -z "$answer" ]] && { echo "$default_value"; return; }
        if [[ "$answer" =~ ^[0-9]+$ ]]; then
            selection="$answer"
            if (( selection >= 1 && selection <= ${#items[@]} )); then
                echo "${items[$((selection - 1))]}"
                return
            fi
        fi
        echo "Invalid selection." >&2
    done
}

validate_value() {
    local label="$1"
    local value="$2"
    shift 2
    if ! contains_value "$value" "$@"; then
        if [[ "$label" == "Godot/godot-cpp branch" ]]; then
            echo "Unsupported Godot version '$value'. Godot 4.3+ only. Supported: $*" >&2
        else
            echo "Unsupported $label '$value'. Supported: $*" >&2
        fi
        exit 1
    fi
}

select_architecture() {
    local requested="$1"
    local platform="$2"
    local default_value
    local items=()

    case "$platform" in
        windows) items=(x86_64); default_value=x86_64 ;;
        linux) items=(x86_64 arm64 rv64); default_value=x86_64 ;;
        macos) items=(universal x86_64 arm64); default_value=universal ;;
        android) items=(arm64 x86_64); default_value=arm64 ;;
        ios) items=(arm64 x86_64); default_value=arm64 ;;
        web) items=(wasm32); default_value=wasm32 ;;
        *) echo "Unsupported platform '$platform'." >&2; exit 1 ;;
    esac

    if [[ -n "$requested" ]]; then
        validate_value "architecture for $platform" "$requested" "${items[@]}"
        echo "$requested"
    elif ((${#items[@]} == 1)); then
        echo "${items[0]}"
    else
        select_option "$platform architecture" "$default_value" "${items[@]}"
    fi
}

platform_targets() {
    case "$1" in
        android|ios|web) echo "template_debug template_release" ;;
        *) echo "editor template_debug template_release" ;;
    esac
}

demo_library_relative_path() {
    local platform="$1"
    local target="$2"
    local arch="$3"
    local web_preset="$4"
    case "$platform" in
        windows) echo "windows/libspine_godot.windows.$target.$arch.dll" ;;
        linux) echo "linux/libspine_godot.linux.$target.$arch.so" ;;
        macos) echo "macos/libspine_godot.macos.$target.framework/libspine_godot.macos.$target" ;;
        android) echo "android/libspine_godot.android.$target.$arch.so" ;;
        ios) echo "ios/libspine_godot.ios.$target.framework/libspine_godot.ios.$target" ;;
        web)
            if [[ "$web_preset" == "nothreads" ]]; then
                echo "web/libspine_godot.web.$target.$arch.nothreads.wasm"
            else
                echo "web/libspine_godot.web.$target.$arch.wasm"
            fi
            ;;
    esac
}

write_log() {
    local message="$1"
    echo "$message"
    printf '%s\n' "$message" >>"$LOG_FILE"
}

record_failure() {
    local status="$1"
    if (( EXIT_MESSAGE_WRITTEN == 1 )); then
        return 0
    fi
    EXIT_MESSAGE_WRITTEN=1
    if [[ -n "${LOG_FILE:-}" && -f "$LOG_FILE" ]]; then
        echo "Status: FAILED" | tee -a "$LOG_FILE" >&2
        printf '%s\n' "Finished: $(date '+%Y-%m-%dT%H:%M:%S%z')" >>"$LOG_FILE"
    else
        echo "Status: FAILED" >&2
    fi
    return "$status"
}

trap 'status=$?; if (( status != 0 )); then record_failure "$status"; fi' EXIT

run_logged() {
    write_log "> $*"
    set +e
    "$@" 2>&1 | tee -a "$LOG_FILE"
    local status=${PIPESTATUS[0]}
    set -e
    if ((status != 0)); then
        record_failure "$status" || true
        exit "$status"
    fi
}

run_logged_allow_failure() {
    write_log "> $*"
    set +e
    "$@" 2>&1 | tee -a "$LOG_FILE"
    local status=${PIPESTATUS[0]}
    set -e
    return "$status"
}

git_is_repo() {
    [[ -d "$1" ]] && git -C "$1" rev-parse --is-inside-work-tree >/dev/null 2>&1
}

git_has_ref() {
    local path="$1"
    local ref="$2"
    git -C "$path" rev-parse --verify --quiet "${ref}^{commit}" >/dev/null 2>&1
}

update_git_remote_branch_ref() {
    local path="$1"
    local branch="$2"
    local name="$3"
    local remote_ref="refs/remotes/origin/$branch"
    local fetch_spec="refs/heads/$branch:$remote_ref"

    if run_logged_allow_failure git -C "$path" fetch --force --depth 1 origin "$fetch_spec"; then
        return
    fi

    if git_has_ref "$path" "$remote_ref"; then
        write_log "$name fetch failed, using cached $remote_ref. Check network if you need the newest upstream commit."
        return
    fi

    echo "$name fetch failed and cached ref is missing: $remote_ref" >&2
    exit 1
}

ensure_dependency_repository() {
    local path="$1"
    local url="$2"
    local name="$3"
    local sparse_path="${4:-}"

    if git_is_repo "$path"; then
        return
    fi
    if [[ -e "$path" && -n "$(find "$path" -mindepth 1 -maxdepth 1 2>/dev/null | head -n 1)" ]]; then
        echo "$name directory exists but is not a Git repository: $path" >&2
        exit 1
    fi
    rm -rf "$path"
    run_logged git clone --filter=blob:none "$url" "$path"
    if [[ -n "$sparse_path" ]]; then
        run_logged git -C "$path" sparse-checkout init --cone
        run_logged git -C "$path" sparse-checkout set "$sparse_path"
    fi
}

assert_clean_dependency() {
    local path="$1"
    local name="$2"
    if [[ -n "$(git -C "$path" status --porcelain --untracked-files=no)" ]]; then
        echo "$name contains tracked local changes. Commit or remove them before switching versions." >&2
        exit 1
    fi
}

select_dependencies() {
    local godot_ref="$1"
    local spine_ref="$2"

    ensure_dependency_repository "$GODOT_CPP_PATH" "https://github.com/godotengine/godot-cpp.git" "godot-cpp"
    ensure_dependency_repository "$SPINE_RUNTIMES_PATH" "https://github.com/EsotericSoftware/spine-runtimes.git" "spine-runtimes" "spine-cpp"

    assert_clean_dependency "$GODOT_CPP_PATH" "godot-cpp"
    assert_clean_dependency "$SPINE_RUNTIMES_PATH" "spine-runtimes"

    update_git_remote_branch_ref "$GODOT_CPP_PATH" "$godot_ref" "godot-cpp"
    run_logged git -C "$GODOT_CPP_PATH" checkout --detach "refs/remotes/origin/$godot_ref"

    run_logged git -C "$SPINE_RUNTIMES_PATH" sparse-checkout set spine-cpp
    update_git_remote_branch_ref "$SPINE_RUNTIMES_PATH" "$spine_ref" "spine-runtimes"
    run_logged git -C "$SPINE_RUNTIMES_PATH" checkout --detach "refs/remotes/origin/$spine_ref"
}

initialize_android_environment() {
    local android_home="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
    local default_android_home=""
    case "$(uname -s)" in
        Darwin) default_android_home="$HOME/Library/Android/sdk" ;;
        Linux) default_android_home="$HOME/Android/Sdk" ;;
        *) default_android_home="${LOCALAPPDATA:-}/Android/Sdk" ;;
    esac

    if [[ -z "$android_home" && -d "$default_android_home" ]]; then
        android_home="$default_android_home"
    fi
    if [[ -n "$android_home" ]]; then
        if [[ ! -d "$android_home" ]]; then
            echo "Android SDK path does not exist: $android_home" >&2
            exit 1
        fi
        export ANDROID_HOME="$android_home"
        export ANDROID_SDK_ROOT="$android_home"
        if [[ -z "${ANDROID_NDK_ROOT:-}" && -d "$android_home/ndk" ]]; then
            local latest_ndk
            latest_ndk="$(find "$android_home/ndk" -mindepth 1 -maxdepth 1 -type d | sort | tail -n 1 || true)"
            if [[ -n "$latest_ndk" ]]; then
                export ANDROID_NDK_ROOT="$latest_ndk"
            fi
        fi
    fi

    if [[ -z "${ANDROID_NDK_ROOT:-}" || ! -d "${ANDROID_NDK_ROOT:-}" ]]; then
        echo "Android build requires Android SDK/NDK. Set ANDROID_HOME or ANDROID_SDK_ROOT to the Android SDK folder, or set ANDROID_NDK_ROOT to an installed NDK folder." >&2
        exit 1
    fi

    write_log "Android SDK: ${ANDROID_HOME:-not set}"
    write_log "Android NDK: $ANDROID_NDK_ROOT"
}

clear_demo_platform_bin_directory() {
    local bin_path="$1"
    local selected_platform="$2"
    local path="$bin_path/$selected_platform"
    [[ -d "$path" ]] || return 0
    write_log "Removing binaries from the previous $selected_platform demo build: $path"
    rm -rf "$path"
}

clear_demo_generated_godot_state() {
    local project_path="$1"
    [[ -d "$project_path" ]] || return 0
    local name
    for name in .godot .mono .godot-mono; do
        if [[ -e "$project_path/$name" ]]; then
            write_log "Removing generated Godot state: $project_path/$name"
            rm -rf "$project_path/$name"
        fi
    done
}

require_command python3 "Install Python 3, or use Xcode command line tools on macOS."
require_command git "Install Git first."
if ((CONFIGURE_ONLY == 0)); then
    require_command scons "Install it with: python3 -m pip install scons"
fi
read_versions

if [[ -z "$BUILD_FLAVOR" ]]; then
    BUILD_FLAVOR="$(select_option "Demo/build flavor" "gdscript" gdscript csharp)"
fi
validate_value "build flavor" "$BUILD_FLAVOR" gdscript csharp

if [[ -z "$GODOT_VERSION" ]]; then
    GODOT_VERSION="$(select_version "Godot/godot-cpp branch" "$DEFAULT_GODOT" no "${GODOT_VERSIONS[@]}")"
fi
validate_value "Godot/godot-cpp branch" "$GODOT_VERSION" "${GODOT_VERSIONS[@]}"

if [[ -z "$SPINE_VERSION" ]]; then
    SPINE_VERSION="$(select_version "Spine runtime version" "$DEFAULT_SPINE" yes "${SPINE_VERSIONS[@]}")"
fi
if [[ "$SPINE_VERSION" != "all" ]]; then
    validate_value "Spine runtime version" "$SPINE_VERSION" "${SPINE_VERSIONS[@]}"
fi

if [[ -z "$PLATFORM" ]]; then
    PLATFORM="$(select_option "Target platform" "macos" windows linux macos android ios web)"
fi
validate_value "platform" "$PLATFORM" windows linux macos android ios web

if [[ "$BUILD_FLAVOR" == "csharp" && "$PLATFORM" == "web" ]]; then
    echo "C# flavor cannot be built for Web. Godot 4 C# projects do not support Web export; use the GDScript flavor for Web." >&2
    exit 1
fi

ARCH="$(select_architecture "$ARCH" "$PLATFORM")"
if [[ "$PLATFORM" == "web" ]]; then
    validate_value "web preset" "$WEB_PRESET" auto threads nothreads
fi

if [[ -z "$JOBS" ]]; then
    if command -v sysctl >/dev/null 2>&1; then
        JOBS="$(sysctl -n hw.ncpu 2>/dev/null || echo 2)"
    else
        JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
    fi
    if ((JOBS > 1)); then
        JOBS=$((JOBS - 1))
    fi
fi

mkdir -p "$LOG_DIR"
SPINE_LABEL="$SPINE_VERSION"
LOG_KIND="build"
if ((CONFIGURE_ONLY == 1)); then
    LOG_KIND="configure"
fi
TIMESTAMP="$(date +%Y%m%d-%H%M%S)"
LOG_FILE="$LOG_DIR/$LOG_KIND-$BUILD_FLAVOR-godot-$GODOT_VERSION-spine-$SPINE_LABEL-$PLATFORM-$ARCH-$TIMESTAMP.log"
: >"$LOG_FILE"

GODOT_REF="$(json_value ref godot "$GODOT_VERSION")"
if [[ "$SPINE_VERSION" == "all" ]]; then
    SPINE_SELECTIONS=("${SPINE_VERSIONS[@]}")
else
    SPINE_SELECTIONS=("$SPINE_VERSION")
fi

TARGETS=($(platform_targets "$PLATFORM"))

write_log "Spine Godot dependency/build tool"
write_log "Started: $(date '+%Y-%m-%dT%H:%M:%S%z')"
write_log "Flavor: $BUILD_FLAVOR"
write_log "Godot/godot-cpp branch: $GODOT_VERSION ($GODOT_REF)"
write_log "Spine: $SPINE_LABEL"
write_log "Platform: $PLATFORM"
write_log "Architecture: $ARCH"
write_log "Targets: ${TARGETS[*]}"
write_log "Jobs: $JOBS"
write_log "Log: $LOG_FILE"

if [[ "$PLATFORM" == "android" ]]; then
    initialize_android_environment
fi

for index in "${!SPINE_SELECTIONS[@]}"; do
    CURRENT_SPINE_VERSION="${SPINE_SELECTIONS[$index]}"
    SPINE_REF="$(json_value ref spine "$CURRENT_SPINE_VERSION")"
    DEMO_ROOT="GDScript_Build"
    DEMO_NAME="example-v4-extension-Spine-$CURRENT_SPINE_VERSION"
    if [[ "$BUILD_FLAVOR" == "csharp" ]]; then
        DEMO_ROOT="CSharp_Build"
        DEMO_NAME="example-v4-csharp-Spine-$CURRENT_SPINE_VERSION"
    fi
    DEMO_PROJECT_PATH="$ROOT/$DEMO_ROOT/$DEMO_NAME"
    DEMO_BIN_PATH="$DEMO_PROJECT_PATH/bin"
    INSTALL_DEMO=1

    write_log ""
    write_log "Spine build $((index + 1))/${#SPINE_SELECTIONS[@]}: $CURRENT_SPINE_VERSION (official branch $SPINE_REF)"
    write_log "Demo project: $DEMO_NAME"
    if [[ ! -f "$DEMO_PROJECT_PATH/project.godot" ]]; then
        write_log "WARNING: Demo project is missing. Continuing with the versioned package only: $DEMO_PROJECT_PATH"
        INSTALL_DEMO=0
    fi

    select_dependencies "$GODOT_REF" "$SPINE_REF"
    GODOT_COMMIT="$(git -C "$GODOT_CPP_PATH" rev-parse HEAD)"
    SPINE_COMMIT="$(git -C "$SPINE_RUNTIMES_PATH" rev-parse HEAD)"
    write_log "godot-cpp commit: $GODOT_COMMIT"
    write_log "spine-runtimes commit: $SPINE_COMMIT"

    python3 - "$ROOT/build/last-selection.json" <<PY
import json
import sys
data = {
    "godot_version": "$GODOT_VERSION",
    "godot_ref": "$GODOT_REF",
    "godot_commit": "$GODOT_COMMIT",
    "spine_version": "$CURRENT_SPINE_VERSION",
    "spine_ref": "$SPINE_REF",
    "spine_commit": "$SPINE_COMMIT",
    "build_flavor": "$BUILD_FLAVOR",
    "platform": "$PLATFORM",
    "arch": "$ARCH",
    "demo_project": "$DEMO_NAME",
}
with open(sys.argv[1], "w", encoding="utf-8") as file:
    json.dump(data, file, indent=2)
    file.write("\\n")
PY

    if ((CONFIGURE_ONLY == 1)); then
        write_log "Dependencies configured. No build was started for Spine $CURRENT_SPINE_VERSION."
        continue
    fi

    if [[ "$BUILD_FLAVOR" == "csharp" && "$INSTALL_DEMO" == "1" ]]; then
        require_command dotnet "Install .NET SDK before building the C# flavor."
        CSPROJ="$DEMO_PROJECT_PATH/spine-godot-examples.csproj"
        if [[ ! -f "$CSPROJ" ]]; then
            echo "C# project file is missing: $CSPROJ" >&2
            exit 1
        fi
        run_logged dotnet --version
        run_logged dotnet restore "$CSPROJ"
    fi

    if [[ "$INSTALL_DEMO" == "1" ]]; then
        clear_demo_platform_bin_directory "$DEMO_BIN_PATH" "$PLATFORM"
        case "$PLATFORM" in
            windows|macos|linux)
                clear_demo_generated_godot_state "$DEMO_PROJECT_PATH"
                ;;
        esac
    fi

    for target in "${TARGETS[@]}"; do
        write_log "Building target: $target"
        SCONS_ARGS=(
            "-j$JOBS"
            "platform=$PLATFORM"
            "arch=$ARCH"
            "target=$target"
            "godot_version=$GODOT_VERSION"
            "spine_runtime=$CURRENT_SPINE_VERSION"
            "demo_flavor=$BUILD_FLAVOR"
        )
        if [[ "$PLATFORM" == "web" && "$WEB_PRESET" != "auto" ]]; then
            if [[ "$WEB_PRESET" == "threads" ]]; then
                SCONS_ARGS+=("threads=yes")
            else
                SCONS_ARGS+=("threads=no")
            fi
        fi
        run_logged scons "${SCONS_ARGS[@]}"
        if [[ "$INSTALL_DEMO" == "1" ]]; then
            RELATIVE_DEMO_LIBRARY="$(demo_library_relative_path "$PLATFORM" "$target" "$ARCH" "$WEB_PRESET")"
            EXPECTED_DEMO_LIBRARY="$DEMO_BIN_PATH/$RELATIVE_DEMO_LIBRARY"
            if [[ ! -e "$EXPECTED_DEMO_LIBRARY" ]]; then
                echo "Expected demo library was not installed: $EXPECTED_DEMO_LIBRARY" >&2
                exit 1
            fi
            write_log "Installed demo library: $EXPECTED_DEMO_LIBRARY"
        fi
    done

    write_log "Versioned package: $ROOT/dist/godot-$GODOT_VERSION/spine-$CURRENT_SPINE_VERSION/spine_godot"
    if [[ "$INSTALL_DEMO" == "1" ]]; then
        write_log "Integrated Godot project: $DEMO_PROJECT_PATH"
    else
        write_log "Demo project installation: skipped"
    fi
done

write_log "Status: SUCCESS"
write_log "Finished: $(date '+%Y-%m-%dT%H:%M:%S%z')"
