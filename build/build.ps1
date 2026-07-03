[CmdletBinding()]
param(
    [ValidateSet("gdscript", "csharp")]
    [string]$BuildFlavor,
    [string]$GodotVersion,
    [string]$SpineVersion,
    [ValidateSet("windows", "linux", "macos", "android", "ios", "web")]
    [string]$Platform,
    [string]$Arch,
    [ValidateSet("auto", "threads", "nothreads")]
    [string]$WebPreset = "auto",
    [int]$Jobs = [Math]::Max(1, [Environment]::ProcessorCount - 1),
    [switch]$ConfigureOnly
)

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding($false)
$OutputEncoding = [Console]::OutputEncoding

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$VersionsFile = Join-Path $PSScriptRoot "versions.json"
$Versions = Get-Content -LiteralPath $VersionsFile -Raw | ConvertFrom-Json
$GodotCppPath = Join-Path $Root "godot-cpp"
$SpineRuntimesPath = Join-Path $Root "spine-runtimes"
$LogDirectory = Join-Path $Root "logs"

function Select-Version {
    param(
        [string]$Title,
        [array]$Items,
        [string]$DefaultVersion,
        [switch]$AllowAll
    )

    Write-Host ""
    Write-Host $Title -ForegroundColor Cyan
    if ($AllowAll) {
        Write-Host "  0. All supported versions"
    }
    for ($index = 0; $index -lt $Items.Count; $index++) {
        $suffix = if ($Items[$index].version -eq $DefaultVersion) { " [default]" } else { "" }
        Write-Host ("  {0}. {1}{2}" -f ($index + 1), $Items[$index].version, $suffix)
    }

    while ($true) {
        $rangeText = if ($AllowAll) { "0-$($Items.Count)" } else { "1-$($Items.Count)" }
        $answer = Read-Host "Select $rangeText, or press Enter for $DefaultVersion"
        if ([string]::IsNullOrWhiteSpace($answer)) {
            return $DefaultVersion
        }

        $selection = 0
        if ($AllowAll -and [int]::TryParse($answer, [ref]$selection) -and $selection -eq 0) {
            return "all"
        }
        if ([int]::TryParse($answer, [ref]$selection) -and $selection -ge 1 -and $selection -le $Items.Count) {
            return [string]$Items[$selection - 1].version
        }
        Write-Host "Invalid selection." -ForegroundColor Yellow
    }
}

function Select-Option {
    param(
        [string]$Title,
        [array]$Items,
        [string]$DefaultValue
    )

    Write-Host ""
    Write-Host $Title -ForegroundColor Cyan
    for ($index = 0; $index -lt $Items.Count; $index++) {
        $suffix = if ($Items[$index].value -eq $DefaultValue) { " [default]" } else { "" }
        Write-Host ("  {0}. {1}{2}" -f ($index + 1), $Items[$index].label, $suffix)
    }

    while ($true) {
        $answer = Read-Host "Select 1-$($Items.Count), or press Enter for $DefaultValue"
        if ([string]::IsNullOrWhiteSpace($answer)) {
            return $DefaultValue
        }

        $selection = 0
        if ([int]::TryParse($answer, [ref]$selection) -and $selection -ge 1 -and $selection -le $Items.Count) {
            return [string]$Items[$selection - 1].value
        }
        Write-Host "Invalid selection." -ForegroundColor Yellow
    }
}

function Find-Version {
    param([array]$Items, [string]$Version, [string]$DependencyName)

    $match = $Items | Where-Object { $_.version -eq $Version } | Select-Object -First 1
    if (-not $match) {
        $allowed = ($Items | ForEach-Object { $_.version }) -join ", "
        if ($DependencyName -eq "Godot") {
            throw "Unsupported Godot version '$Version'. Godot 4.3+ only. Supported versions: $allowed"
        }
        throw "Unsupported $DependencyName version '$Version'. Supported versions: $allowed"
    }
    return $match
}

function Get-PlatformTargets {
    param([string]$SelectedPlatform)

    if ($SelectedPlatform -in @("android", "ios", "web")) {
        return @("template_debug", "template_release")
    }
    return @("editor", "template_debug", "template_release")
}

function Get-DemoLibraryRelativePath {
    param(
        [string]$SelectedPlatform,
        [string]$BuildTarget,
        [string]$SelectedArch,
        [string]$SelectedWebPreset
    )

    switch ($SelectedPlatform) {
        "windows" {
            return "windows\libspine_godot.windows.$BuildTarget.$SelectedArch.dll"
        }
        "linux" {
            return "linux\libspine_godot.linux.$BuildTarget.$SelectedArch.so"
        }
        "macos" {
            return "macos\libspine_godot.macos.$BuildTarget.framework\libspine_godot.macos.$BuildTarget"
        }
        "android" {
            return "android\libspine_godot.android.$BuildTarget.$SelectedArch.so"
        }
        "ios" {
            return "ios\libspine_godot.ios.$BuildTarget.framework\libspine_godot.ios.$BuildTarget"
        }
        "web" {
            $suffix = if ($SelectedWebPreset -eq "nothreads") { "nothreads.wasm" } else { "wasm" }
            return "web\libspine_godot.web.$BuildTarget.$SelectedArch.$suffix"
        }
        default {
            return $null
        }
    }
}

function Select-Architecture {
    param(
        [string]$SelectedPlatform,
        [string]$RequestedArch
    )

    $platformArchitectures = @{
        windows = @(
            @{ value = "x86_64"; label = "x86_64" }
        )
        linux = @(
            @{ value = "x86_64"; label = "x86_64" },
            @{ value = "arm64"; label = "arm64" },
            @{ value = "rv64"; label = "rv64" }
        )
        macos = @(
            @{ value = "universal"; label = "universal" },
            @{ value = "x86_64"; label = "x86_64" },
            @{ value = "arm64"; label = "arm64" }
        )
        android = @(
            @{ value = "arm64"; label = "arm64" },
            @{ value = "x86_64"; label = "x86_64" }
        )
        ios = @(
            @{ value = "arm64"; label = "arm64" },
            @{ value = "x86_64"; label = "x86_64 simulator" }
        )
        web = @(
            @{ value = "wasm32"; label = "wasm32" }
        )
    }

    $defaults = @{
        windows = "x86_64"
        linux = "x86_64"
        macos = "universal"
        android = "arm64"
        ios = "arm64"
        web = "wasm32"
    }

    $items = @($platformArchitectures[$SelectedPlatform])
    if ($RequestedArch) {
        if ($RequestedArch -notin @($items | ForEach-Object { $_.value })) {
            $allowed = (@($items | ForEach-Object { $_.value }) -join ", ")
            throw "Unsupported architecture '$RequestedArch' for platform '$SelectedPlatform'. Supported: $allowed"
        }
        return $RequestedArch
    }

    if ($items.Count -le 1) {
        return [string]$items[0].value
    }
    return Select-Option "$SelectedPlatform architecture" $items ([string]$defaults[$SelectedPlatform])
}

function Test-GitRepository {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    & git -C $Path rev-parse --is-inside-work-tree *> $null
    return $LASTEXITCODE -eq 0
}

function Get-GodotProcessesUsingProject {
	param([string]$ProjectPath)

    if (-not (Get-Command Get-CimInstance -ErrorAction SilentlyContinue)) {
        return @()
    }

	$normalizedProjectPath = $ProjectPath.Replace("\", "/")
    return @(
        Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Name -like "Godot*.exe" -and
                $_.CommandLine -and
                $_.CommandLine.Replace("\", "/") -like "*$normalizedProjectPath*"
            }
	)
}

function Get-PlatformBinaryCleanupPatterns {
    param(
        [string]$SelectedPlatform,
        [string]$SelectedArch,
        [string]$SelectedWebPreset
    )

    switch ($SelectedPlatform) {
        "windows" {
            return @(
                "libspine_godot.windows.*.$SelectedArch.dll",
                "~libspine_godot.windows.*.$SelectedArch.dll*"
            )
        }
        "linux" {
            return @("libspine_godot.linux.*.$SelectedArch.so")
        }
        "android" {
            return @("libspine_godot.android.*.$SelectedArch.so")
        }
        "web" {
            $suffix = if ($SelectedWebPreset -eq "nothreads") { "$SelectedArch.nothreads.wasm" } else { "$SelectedArch.wasm" }
            return @("libspine_godot.web.*.$suffix")
        }
        default {
            return @("*")
        }
    }
}

function Clear-PlatformBinDirectory {
    param(
        [string]$BinPath,
        [string]$SelectedPlatform,
        [string]$SelectedArch,
        [string]$SelectedWebPreset,
        [string]$Description
    )

    $path = Join-Path $BinPath $SelectedPlatform
    if (-not (Test-Path -LiteralPath $path)) {
        return
    }

    $resolvedBinPath = (Resolve-Path -LiteralPath $BinPath).Path
    $resolvedPath = (Resolve-Path -LiteralPath $path).Path
    if (-not $resolvedPath.StartsWith($resolvedBinPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to clean path outside ${Description}: $resolvedPath"
    }

    $skippedHotReloadFiles = @()
    $failedFiles = @()
    $patterns = Get-PlatformBinaryCleanupPatterns $SelectedPlatform $SelectedArch $SelectedWebPreset
    $filesToRemove = @(
        foreach ($pattern in $patterns) {
            Get-ChildItem -LiteralPath $resolvedPath -Recurse -Force -File -Filter $pattern
        }
    ) | Sort-Object FullName -Unique

    $filesToRemove |
        Sort-Object FullName -Descending |
        ForEach-Object {
            $file = $_
            try {
                Remove-Item -LiteralPath $file.FullName -Force
            } catch {
                if ($_.Exception -is [System.UnauthorizedAccessException] -and $file.Name -like "~lib*.dll") {
                    $skippedHotReloadFiles += $file.FullName
                } else {
                    $failedFiles += ("{0}: {1}" -f $file.FullName, $_.Exception.Message)
                }
            }
        }

    if ($failedFiles.Count -gt 0) {
        throw "Could not clean ${Description}:`n$($failedFiles -join "`n")"
    }

    if ($skippedHotReloadFiles.Count -gt 0) {
        Write-Log "Skipped locked Godot hot-reload DLL(s). Close Godot to remove them fully:" Yellow
        foreach ($file in $skippedHotReloadFiles) {
            Write-Log "  $file" Yellow
        }
    }
}

function Clear-DemoPlatformBinDirectory {
    param(
        [string]$BinPath,
        [string]$SelectedPlatform,
        [string]$SelectedArch,
        [string]$SelectedWebPreset
    )

    Clear-PlatformBinDirectory $BinPath $SelectedPlatform $SelectedArch $SelectedWebPreset "demo bin directory"
}

function Clear-DemoGeneratedGodotState {
    param([string]$ProjectPath)

    if (-not (Test-Path -LiteralPath $ProjectPath)) {
        return
    }

    $resolvedProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
    $generatedDirectories = @(".godot", ".mono", ".godot-mono")
    foreach ($directoryName in $generatedDirectories) {
        $path = Join-Path $resolvedProjectPath $directoryName
        if (-not (Test-Path -LiteralPath $path)) {
            continue
        }

        $resolvedPath = (Resolve-Path -LiteralPath $path).Path
        if (-not $resolvedPath.StartsWith($resolvedProjectPath, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to clean generated Godot state outside demo project: $resolvedPath"
        }

        Write-Log "Removing generated Godot state: $resolvedPath" DarkGray
        Remove-Item -LiteralPath $resolvedPath -Recurse -Force
    }
}

function Write-Log {
	param([string]$Message, [ConsoleColor]$Color = [ConsoleColor]::Gray)

	Write-Host $Message -ForegroundColor $Color
    Add-Content -LiteralPath $script:LogFile -Value $Message -Encoding UTF8
}

function Invoke-LoggedCommand {
    param([string]$File, [string[]]$Arguments)

    Write-Log ("> {0} {1}" -f $File, ($Arguments -join " ")) DarkGray
    $commandParts = (@($File) + $Arguments) | ForEach-Object {
        if ($_ -match '[\s&|<>^]') {
            '"' + ($_ -replace '"', '\"') + '"'
        } else {
            $_
        }
    }
    $commandLine = ($commandParts -join " ") + " 2>&1"
    & cmd.exe /d /c $commandLine | Tee-Object -FilePath $script:LogFile -Append
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        throw "Command failed with exit code ${exitCode}: $File"
    }
}

function Invoke-LoggedCommandAllowFailure {
    param([string]$File, [string[]]$Arguments)

    Write-Log ("> {0} {1}" -f $File, ($Arguments -join " ")) DarkGray
    $commandParts = (@($File) + $Arguments) | ForEach-Object {
        if ($_ -match '[\s&|<>^]') {
            '"' + ($_ -replace '"', '\"') + '"'
        } else {
            $_
        }
    }
    $commandLine = ($commandParts -join " ") + " 2>&1"
    & cmd.exe /d /c $commandLine | Tee-Object -FilePath $script:LogFile -Append
    return $LASTEXITCODE
}

function Test-GitRef {
    param([string]$Path, [string]$Ref)

    & git -C $Path rev-parse --verify --quiet "$Ref^{commit}" *> $null
    return $LASTEXITCODE -eq 0
}

function Update-GitRemoteBranchRef {
    param(
        [string]$Path,
        [string]$Branch,
        [string]$Name
    )

    $remoteRef = "refs/remotes/origin/$Branch"
    $fetchSpec = "refs/heads/${Branch}:${remoteRef}"
    $exitCode = Invoke-LoggedCommandAllowFailure "git" @("-C", $Path, "fetch", "--force", "--depth", "1", "origin", $fetchSpec)
    if ($exitCode -eq 0) {
        return
    }

    if (Test-GitRef $Path $remoteRef) {
        Write-Log "$Name fetch failed, using cached $remoteRef. Check network if you need the newest upstream commit." Yellow
        return
    }

    throw "$Name fetch failed and cached ref is missing: $remoteRef"
}

function Assert-CleanDependency {
    param([string]$Path, [string]$Name)

    $changes = & git -C $Path status --porcelain --untracked-files=no
    if ($changes) {
        throw "$Name contains tracked local changes. Commit or remove them before switching versions."
    }
}

function Ensure-DependencyRepository {
    param(
        [string]$Path,
        [string]$Url,
        [string]$Name,
        [string]$SparsePath
    )

    if (Test-GitRepository $Path) {
        return
    }

    if ((Test-Path -LiteralPath $Path) -and (Get-ChildItem -LiteralPath $Path -Force | Select-Object -First 1)) {
        throw "$Name directory exists but is not a Git repository: $Path"
    }

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Force
    }

    Invoke-LoggedCommand "git" @("clone", "--filter=blob:none", $Url, $Path)
    if ($SparsePath) {
        Invoke-LoggedCommand "git" @("-C", $Path, "sparse-checkout", "init", "--cone")
        Invoke-LoggedCommand "git" @("-C", $Path, "sparse-checkout", "set", $SparsePath)
    }
}

function Select-Dependencies {
    param($GodotSelection, $SpineSelection)

    Ensure-DependencyRepository $GodotCppPath "https://github.com/godotengine/godot-cpp.git" "godot-cpp" ""
    Ensure-DependencyRepository $SpineRuntimesPath "https://github.com/EsotericSoftware/spine-runtimes.git" "spine-runtimes" "spine-cpp"

    Assert-CleanDependency $GodotCppPath "godot-cpp"
    Assert-CleanDependency $SpineRuntimesPath "spine-runtimes"

    Update-GitRemoteBranchRef $GodotCppPath $GodotSelection.ref "godot-cpp"
    Invoke-LoggedCommand "git" @("-C", $GodotCppPath, "checkout", "--detach", "refs/remotes/origin/$($GodotSelection.ref)")

    Invoke-LoggedCommand "git" @("-C", $SpineRuntimesPath, "sparse-checkout", "set", "spine-cpp")
    Update-GitRemoteBranchRef $SpineRuntimesPath $SpineSelection.ref "spine-runtimes"
    Invoke-LoggedCommand "git" @("-C", $SpineRuntimesPath, "checkout", "--detach", "refs/remotes/origin/$($SpineSelection.ref)")
}

function Initialize-AndroidEnvironment {
    $androidHome = $env:ANDROID_HOME
    if (-not $androidHome) {
        $androidHome = $env:ANDROID_SDK_ROOT
    }
    if (-not $androidHome) {
        $defaultAndroidHome = Join-Path $env:LOCALAPPDATA "Android\Sdk"
        if (Test-Path -LiteralPath $defaultAndroidHome) {
            $androidHome = $defaultAndroidHome
        }
    }

    if ($androidHome) {
        $resolvedAndroidHome = (Resolve-Path -LiteralPath $androidHome -ErrorAction SilentlyContinue)
        if (-not $resolvedAndroidHome) {
            throw "Android SDK path does not exist: $androidHome"
        }
        $env:ANDROID_HOME = $resolvedAndroidHome.Path
        $env:ANDROID_SDK_ROOT = $resolvedAndroidHome.Path

        $ndkRoot = $env:ANDROID_NDK_ROOT
        if (-not $ndkRoot) {
            $ndkDirectory = Join-Path $resolvedAndroidHome.Path "ndk"
            if (Test-Path -LiteralPath $ndkDirectory) {
                $latestNdk = Get-ChildItem -LiteralPath $ndkDirectory -Directory |
                    Sort-Object Name -Descending |
                    Select-Object -First 1
                if ($latestNdk) {
                    $env:ANDROID_NDK_ROOT = $latestNdk.FullName
                    $env:ANDROID_NDK_VERSION = $latestNdk.Name
                }
            }
        }

        if (-not $env:ANDROID_NDK_ROOT) {
            throw "Android SDK was found at '$($env:ANDROID_HOME)', but no NDK is installed. Install Android NDK side-by-side in Android Studio SDK Manager, or run: sdkmanager `"ndk;28.1.13356709`""
        }

        Write-Log "Android SDK: $($env:ANDROID_HOME)" Green
        if ($env:ANDROID_NDK_ROOT) {
            Write-Log "Android NDK: $($env:ANDROID_NDK_ROOT)" Green
        }
        if ($env:ANDROID_NDK_VERSION) {
            Write-Log "Android NDK version: $($env:ANDROID_NDK_VERSION)" Green
        }
        return
    }

    if ($env:ANDROID_NDK_ROOT) {
        Write-Log "Android NDK: $($env:ANDROID_NDK_ROOT)" Green
        return
    }

    throw "Android build requires Android SDK/NDK. Set ANDROID_HOME or ANDROID_SDK_ROOT to the Android SDK folder, or set ANDROID_NDK_ROOT to an installed NDK folder."
}

if (-not $BuildFlavor) {
    $BuildFlavor = Select-Option "Demo/build flavor" @(
        @{ value = "gdscript"; label = "GDScript" },
        @{ value = "csharp"; label = "C#" }
    ) "gdscript"
}
if (-not $GodotVersion) {
    $GodotVersion = Select-Version "Godot/godot-cpp branch" @($Versions.godot) ([string]$Versions.defaults.godot)
}
if (-not $SpineVersion) {
    $SpineVersion = Select-Version "Spine runtime version" @($Versions.spine) ([string]$Versions.defaults.spine) -AllowAll
}
if (-not $Platform) {
    $Platform = Select-Option "Target platform" @(
        @{ value = "windows"; label = "Windows" },
        @{ value = "linux"; label = "Linux" },
        @{ value = "macos"; label = "macOS" },
        @{ value = "android"; label = "Android" },
        @{ value = "ios"; label = "iOS" },
        @{ value = "web"; label = "Web" }
    ) "windows"
}
if ($BuildFlavor -eq "csharp" -and $Platform -eq "web") {
    Write-Host "C# flavor cannot be built for Web. Godot 4 C# projects do not support Web export; use the GDScript flavor for Web." -ForegroundColor Red
    exit 1
}
$SelectedArch = Select-Architecture $Platform $Arch
if ($Platform -eq "web" -and $WebPreset -eq "auto" -and -not $PSBoundParameters.ContainsKey("WebPreset")) {
    $WebPreset = Select-Option "Web threading preset" @(
        @{ value = "threads"; label = "threads" },
        @{ value = "nothreads"; label = "nothreads" }
    ) "threads"
}

$GodotSelection = Find-Version @($Versions.godot) $GodotVersion "Godot"
$BuildAllSpineVersions = $SpineVersion -in @("0", "all", "*")
if ($BuildAllSpineVersions) {
    $SpineBuildSelections = @($Versions.spine)
    $SpineVersionLabel = "all"
} else {
    $SpineBuildSelections = @(Find-Version @($Versions.spine) $SpineVersion "Spine")
    $SpineVersionLabel = $SpineVersion
}
$Targets = Get-PlatformTargets $Platform

New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$operation = if ($ConfigureOnly) { "configure" } else { "build" }
$script:LogFile = Join-Path $LogDirectory "$operation-$BuildFlavor-godot-$GodotVersion-spine-$SpineVersionLabel-$Platform-$SelectedArch-$timestamp.log"

try {
    Write-Log "Spine Godot dependency/build tool" Cyan
    Write-Log "Started: $((Get-Date).ToString('o'))"
    Write-Log "Flavor: $BuildFlavor"
    Write-Log "Godot/godot-cpp branch: $GodotVersion ($($GodotSelection.ref))"
    Write-Log "Spine: $SpineVersionLabel"
    Write-Log "Platform: $Platform"
    Write-Log "Architecture: $SelectedArch"
    if ($Platform -eq "web") {
        Write-Log "Web preset: $WebPreset"
    }
    Write-Log "Targets: $($Targets -join ', ')"
    Write-Log "Jobs: $Jobs"
    Write-Log "Log: ${script:LogFile}"

    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        throw "Git was not found in PATH."
    }

    if (-not $ConfigureOnly -and -not (Get-Command scons -ErrorAction SilentlyContinue)) {
        throw "SCons was not found in PATH. Install it with: python -m pip install scons"
    }

    if ($Platform -eq "android") {
        Initialize-AndroidEnvironment
    }

    for ($spineIndex = 0; $spineIndex -lt $SpineBuildSelections.Count; $spineIndex++) {
        $SpineSelection = $SpineBuildSelections[$spineIndex]
        $CurrentSpineVersion = [string]$SpineSelection.version
        $DemoProjectRootName = if ($BuildFlavor -eq "csharp") { "CSharp_Build" } else { "GDScript_Build" }
        $DemoProjectName = if ($BuildFlavor -eq "csharp") { "example-v4-csharp-Spine-$CurrentSpineVersion" } else { "example-v4-extension-Spine-$CurrentSpineVersion" }
        $DemoProjectPath = Join-Path (Join-Path $Root $DemoProjectRootName) $DemoProjectName
        $DemoBinPath = Join-Path $DemoProjectPath "bin"
        $BuildBinPath = Join-Path $Root "bin\godot-$GodotVersion\spine-$CurrentSpineVersion"
        $PackageBinPath = Join-Path $Root "dist\godot-$GodotVersion\spine-$CurrentSpineVersion\spine_godot\bin"
        $InstallDemo = $true

        Write-Log ""
        Write-Log ("Spine build {0}/{1}: {2} (official branch {3})" -f ($spineIndex + 1), $SpineBuildSelections.Count, $CurrentSpineVersion, $SpineSelection.ref) Cyan
        Write-Log "Demo project: $DemoProjectName"
        if ($InstallDemo -and -not (Test-Path -LiteralPath (Join-Path $DemoProjectPath "project.godot"))) {
            Write-Log "WARNING: Demo project is missing. Continuing with the versioned package only: $DemoProjectPath" Yellow
            $InstallDemo = $false
        }

        Select-Dependencies $GodotSelection $SpineSelection

        $godotCommit = (& git -C $GodotCppPath rev-parse HEAD).Trim()
        $spineCommit = (& git -C $SpineRuntimesPath rev-parse HEAD).Trim()
        Write-Log "godot-cpp commit: $godotCommit" Green
        Write-Log "spine-runtimes commit: $spineCommit" Green

        $selectionState = [ordered]@{
            godot_version = $GodotVersion
            godot_ref = [string]$GodotSelection.ref
            godot_commit = $godotCommit
            spine_version = $CurrentSpineVersion
            spine_ref = [string]$SpineSelection.ref
            spine_commit = $spineCommit
            build_flavor = $BuildFlavor
            platform = $Platform
            arch = $SelectedArch
            demo_project = $DemoProjectName
        }
        if ($BuildAllSpineVersions) {
            $selectionState.spine_versions_requested = @($SpineBuildSelections | ForEach-Object { [string]$_.version })
        }
        $selectionState | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PSScriptRoot "last-selection.json") -Encoding UTF8

        if ($ConfigureOnly) {
            Write-Log "Dependencies configured. No build was started for Spine $CurrentSpineVersion." Green
            continue
        }

        if ($BuildFlavor -eq "csharp" -and $InstallDemo) {
            if (-not (Get-Command dotnet -ErrorAction SilentlyContinue)) {
                throw ".NET SDK was not found in PATH. Install .NET SDK before building the C# flavor."
            }
            $csproj = Join-Path $DemoProjectPath "spine-godot-examples.csproj"
            if (-not (Test-Path -LiteralPath $csproj)) {
                throw "C# project file is missing: $csproj"
            }
            Invoke-LoggedCommand "dotnet" @("--version")
            Invoke-LoggedCommand "dotnet" @("restore", $csproj)
        }

        if ($InstallDemo) {
            $openGodotProcesses = Get-GodotProcessesUsingProject $DemoProjectPath
            if ($openGodotProcesses.Count -gt 0) {
                $processIds = ($openGodotProcesses | ForEach-Object { $_.ProcessId }) -join ", "
                throw "Close Godot instances using '$DemoProjectName' before installing its DLL (process IDs: $processIds)."
            }

            if (Test-Path -LiteralPath $DemoBinPath) {
                Write-Log "Removing binaries from the previous $Platform/$SelectedArch demo build: $(Join-Path $DemoBinPath $Platform)" DarkGray
                Clear-DemoPlatformBinDirectory $DemoBinPath $Platform $SelectedArch $WebPreset
            }
            if ($Platform -eq "windows") {
                Clear-DemoGeneratedGodotState $DemoProjectPath
            }
        }

        if (Test-Path -LiteralPath $BuildBinPath) {
            Write-Log "Removing binaries from the previous $Platform/$SelectedArch build output: $(Join-Path $BuildBinPath $Platform)" DarkGray
            Clear-PlatformBinDirectory $BuildBinPath $Platform $SelectedArch $WebPreset "versioned build output"
        }
        if (Test-Path -LiteralPath $PackageBinPath) {
            Write-Log "Removing binaries from the previous $Platform/$SelectedArch package: $(Join-Path $PackageBinPath $Platform)" DarkGray
            Clear-PlatformBinDirectory $PackageBinPath $Platform $SelectedArch $WebPreset "versioned package bin directory"
        }

        Push-Location $Root
        try {
            foreach ($buildTarget in $Targets) {
                Write-Log "Building target: $buildTarget" Cyan
                $sconsArguments = @(
                    "-j$Jobs",
                    "platform=$Platform",
                    "arch=$SelectedArch",
                    "target=$buildTarget",
                    "godot_version=$GodotVersion",
                    "spine_runtime=$CurrentSpineVersion",
                    "demo_flavor=$BuildFlavor"
                )
                if ($Platform -eq "web" -and $WebPreset -ne "auto") {
                    if ($WebPreset -eq "threads") {
                        $sconsArguments += "threads=yes"
                    } else {
                        $sconsArguments += "threads=no"
                    }
                }
                if ($Platform -eq "android" -and $env:ANDROID_NDK_ROOT) {
                    $sconsArguments += "ANDROID_HOME="
                }
                if ($Platform -eq "windows") {
                    # godot-cpp's MSVC output capture can crash on non-UTF console text.
                    $sconsArguments += "silence_msvc=no"
                }
                Invoke-LoggedCommand "scons" $sconsArguments
                if ($InstallDemo) {
                    $relativeDemoLibrary = Get-DemoLibraryRelativePath $Platform $buildTarget $SelectedArch $WebPreset
                    if ($relativeDemoLibrary) {
                        $expectedDemoLibrary = Join-Path $DemoBinPath $relativeDemoLibrary
                        if (-not (Test-Path -LiteralPath $expectedDemoLibrary)) {
                            throw "Expected demo library was not installed: $expectedDemoLibrary"
                        }
                        Write-Log "Installed demo library: $expectedDemoLibrary" Green
                    }
                }
            }
        } finally {
            Pop-Location
        }

        $outputPath = Join-Path $Root "dist\godot-$GodotVersion\spine-$CurrentSpineVersion\spine_godot"
        Write-Log "Versioned package: $outputPath" Green
        if ($InstallDemo) {
            Write-Log "Integrated Godot project: $DemoProjectPath" Green
        } else {
            Write-Log "Demo project installation: skipped" Yellow
        }
    }

    Write-Log "Status: SUCCESS" Green
    exit 0
} catch {
    Write-Log "Status: FAILED" Red
    Write-Log (($_ | Out-String).Trim()) Red
    exit 1
} finally {
    Write-Log "Finished: $((Get-Date).ToString('o'))"
}
