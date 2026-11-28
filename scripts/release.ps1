<#
.SYNOPSIS
    Builds, tests and packages a Windows release of Note.

.DESCRIPTION
    Validates the MSYS2 UCRT64 toolchain, refuses a dirty Git tree, does a clean
    Release build, runs every test, deploys Qt and runtime DLLs, adds licence
    notices, writes a versioned zip with a SHA-256 checksum, then starts note.exe
    from the unzipped package without MSYS2 on PATH. Any failure stops the script
    with exit code 1. Requires Windows PowerShell 5.1 or later.

.PARAMETER Msys2Root
    MSYS2 installation folder (the one containing ucrt64 and usr). Detected from
    the MSYS2 uninstall registry entry or a UCRT64 g++ on PATH when omitted.

.PARAMETER OutputRoot
    Folder for the package and build files. Must be outside the source folder.
    Defaults to a "note-release" folder beside the repository.

.PARAMETER Jobs
    Parallel build jobs (default 2).

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\release.ps1
#>
[CmdletBinding()]
param(
    [string]$Msys2Root,
    [string]$InnoSetupCompiler,
    [string]$OutputRoot,
    [ValidateRange(1, 64)][int]$Jobs = 2
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Step([string]$Message) { Write-Host "==> $Message" -ForegroundColor Cyan }

# Runs a native program and throws if it fails. Native stderr is not treated as an error.
function Invoke-Native {
    param([Parameter(Mandatory = $true)][string]$FilePath, [string[]]$Arguments = @())
    $ErrorActionPreference = 'Continue'
    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$(Split-Path $FilePath -Leaf) $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
}

# Runs a native program and returns its standard output lines.
function Get-NativeOutput {
    param([Parameter(Mandatory = $true)][string]$FilePath, [string[]]$Arguments = @())
    $ErrorActionPreference = 'Continue'
    $output = & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$(Split-Path $FilePath -Leaf) $($Arguments -join ' ') failed with exit code $LASTEXITCODE"
    }
    return @($output)
}

function Find-Msys2Root([string]$Explicit) {
    $candidates = @()
    if ($Explicit) {
        $candidates += $Explicit
    } else {
        $keys = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
                'HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*',
                'HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\*'
        foreach ($entry in @(Get-ItemProperty $keys -ErrorAction SilentlyContinue)) {
            $properties = $entry.PSObject.Properties
            if ($properties['DisplayName'] -and $entry.DisplayName -eq 'MSYS2' -and
                $properties['InstallLocation'] -and $entry.InstallLocation) {
                $candidates += $entry.InstallLocation
            }
        }
        foreach ($command in @(Get-Command g++.exe -All -ErrorAction SilentlyContinue)) {
            $bin = Split-Path $command.Source
            if ((Split-Path $bin -Leaf) -eq 'bin' -and (Split-Path (Split-Path $bin) -Leaf) -eq 'ucrt64') {
                $candidates += Split-Path (Split-Path $bin)
            }
        }
    }
    foreach ($candidate in $candidates) {
        if (Test-Path (Join-Path $candidate 'ucrt64\bin\g++.exe')) {
            return (Resolve-Path $candidate).Path
        }
    }
    if ($Explicit) { throw "No MSYS2 UCRT64 toolchain at '$Explicit' (expected ucrt64\bin\g++.exe)." }
    throw 'MSYS2 UCRT64 toolchain not found. Install MSYS2 and the packages listed in docs/RELEASE.md, or pass -Msys2Root.'
}


function Find-InnoSetupCompiler([string]$Explicit) {
    $candidates = @()

    if ($Explicit) {
        $candidates += $Explicit
    } else {
        $command = Get-Command ISCC.exe -ErrorAction SilentlyContinue
        if ($command) { $candidates += $command.Source }

        if ($env:LOCALAPPDATA) {
            $candidates += Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 7\ISCC.exe'
        }
        if ($env:ProgramFiles) {
            $candidates += Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe'
        }
        if (${env:ProgramFiles(x86)}) {
            $candidates += Join-Path ${env:ProgramFiles(x86)} 'Inno Setup 7\ISCC.exe'
        }
    }

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    if ($Explicit) {
        throw "Inno Setup compiler not found at '$Explicit'."
    }

    throw 'Inno Setup compiler (ISCC.exe) not found. Install Inno Setup 7 or pass -InnoSetupCompiler.'
}
function Test-IsInside([string]$Path, [string]$Folder) {
    $path = [IO.Path]::GetFullPath($Path).TrimEnd('\') + '\'
    $folder = [IO.Path]::GetFullPath($Folder).TrimEnd('\') + '\'
    return $path.StartsWith($folder, [StringComparison]::OrdinalIgnoreCase)
}

# Copies every licence file of the MSYS2 packages that own the bundled DLLs and
# writes THIRD-PARTY-NOTICES.txt.
function Add-LicenseNotices([string]$Stage, [string]$Root, [string]$Version) {
    $pacman = Join-Path $Root 'usr\bin\pacman.exe'
    $bin = Join-Path $Root 'ucrt64\bin'
    $plugins = @(Get-NativeOutput (Join-Path $bin 'qtpaths6.exe') @('--query', 'QT_INSTALL_PLUGINS'))[0]
    # DLLs at the top come from ucrt64\bin; those in subfolders are Qt plugins.
    # Relative paths come from enumeration so 8.3 short names in $Stage do not matter.
    $bundled = @()
    foreach ($dll in @(Get-ChildItem $Stage -Filter *.dll -File)) {
        $bundled += New-Object PSObject -Property @{ Relative = $dll.Name; Source = Join-Path $bin $dll.Name }
    }
    foreach ($folder in @(Get-ChildItem $Stage -Directory)) {
        foreach ($dll in @(Get-ChildItem $folder.FullName -Recurse -Filter *.dll -File)) {
            $relative = $dll.FullName.Substring($folder.Parent.FullName.Length + 1)
            $bundled += New-Object PSObject -Property @{ Relative = $relative; Source = Join-Path $plugins $relative }
        }
    }
    $owners = [ordered]@{}
    foreach ($item in @($bundled | Sort-Object Relative)) {
        $relative = $item.Relative
        $source = $item.Source
        if (-not (Test-Path $source)) { throw "Cannot find the toolchain source of $relative ($source)." }
        # pacman does not understand backslashes in Windows paths.
        $package = @(Get-NativeOutput $pacman @('-Qqo', ($source -replace '\\', '/')))[0]
        if (-not $owners.Contains($package)) { $owners[$package] = @() }
        $owners[$package] += $relative
    }
    $licenses = Join-Path $Stage 'licenses'
    $rows = @()
    foreach ($package in $owners.Keys) {
        $short = $package -replace '^mingw-w64-ucrt-x86_64-', ''
        $packageVersion = (@(Get-NativeOutput $pacman @('-Q', $package))[0] -split ' ')[1]
        $packageFiles = @(Get-NativeOutput $pacman @('-Qql', $package))
        # Usually share/licenses/<package>/...; some packages (ICU) keep a LICENSE elsewhere.
        $files = @($packageFiles | Where-Object { $_ -match '/share/licenses/.*[^/]$' })
        if ($files.Count -eq 0) {
            $files = @($packageFiles | Where-Object { $_ -match '/(LICEN[CS]E|COPYING)[^/]*$' })
        }
        if ($files.Count -eq 0) { throw "Package $package has no licence files in MSYS2." }
        foreach ($file in $files) {
            if ($file -match '/share/licenses/(.+)$') { $relative = $Matches[1] -replace '/', '\' }
            else { $relative = Join-Path $short (Split-Path ($file -replace '/', '\') -Leaf) }
            $target = Join-Path $licenses $relative
            New-Item -ItemType Directory -Force (Split-Path $target) | Out-Null
            Copy-Item (Join-Path $Root ($file.TrimStart('/') -replace '/', '\')) $target
        }
        $rows += '{0,-22} {1,-14} {2}' -f $short, $packageVersion, ($owners[$package] -join ', ')
    }
    foreach ($required in 'qt6-base\LGPL-3.0-only.txt', 'qt6-base\GPL-3.0-only.txt',
                          'libstdc++\COPYING.RUNTIME', 'libgcc\COPYING.RUNTIME') {
        if (-not (Test-Path (Join-Path $licenses $required))) { throw "Missing licence file licenses\$required." }
    }
    $notice = @"
Note $Version - third-party notices

Note itself is released under the MIT License (LICENSE.txt).

This package bundles the libraries below, unmodified, as DLLs built by the
MSYS2 project for its UCRT64 environment. Each keeps its own licence; the full
texts are in the licenses folder, one subfolder per package.

Qt 6 (Qt6Core.dll, Qt6Gui.dll, Qt6Widgets.dll and the plugins in platforms and
styles) is used under the GNU Lesser General Public License version 3. See
licenses\qt6-base\LGPL-3.0-only.txt, which builds on
licenses\qt6-base\GPL-3.0-only.txt. Note links to Qt dynamically; you may
replace these DLLs with your own compatible build of Qt. Qt source code:
https://download.qt.io/official_releases/qt/ . The MSYS2 build recipe:
https://github.com/msys2/MINGW-packages/tree/master/mingw-w64-qt6-base .

The GCC runtime (libgcc_s_seh-1.dll, libstdc++-6.dll) is licensed under the
GNU GPL version 3 with the GCC Runtime Library Exception version 3.1; see
COPYING3 and COPYING.RUNTIME in licenses\libgcc and licenses\libstdc++.
libwinpthread-1.dll: see licenses\libwinpthread.

Source code for every package: https://packages.msys2.org/ (search for the
package name) and https://github.com/msys2/MINGW-packages .

Package                Version        Files
"@
    $notice += "`r`n" + ($rows -join "`r`n") + "`r`n"
    Set-Content -Path (Join-Path $Stage 'THIRD-PARTY-NOTICES.txt') -Value $notice -Encoding Ascii
    return $owners.Count
}

# Starts note.exe from the unzipped package with only Windows folders on PATH,
# checks that it shows its window without loading anything from MSYS2, and
# closes it. The real %APPDATA%\Note is parked so the check cannot change it.
function Test-PackageLaunch([string]$PackageDir, [string]$Root) {
    if (Get-Process note -ErrorAction SilentlyContinue) {
        throw 'Close every running Note first: the launch check must not share %APPDATA%\Note with a running instance.'
    }
    $data = Join-Path $env:APPDATA 'Note'
    $parked = "$data.release-check-parked"
    if (Test-Path $parked) {
        throw "Found $parked from an interrupted run. Restore it to $data by hand, then rerun."
    }
    $savedPath = $env:PATH
    $savedQt = @{}
    foreach ($name in 'QT_PLUGIN_PATH', 'QT_QPA_PLATFORM', 'QT_QPA_PLATFORM_PLUGIN_PATH') {
        $savedQt[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        [Environment]::SetEnvironmentVariable($name, $null, 'Process')
    }
    $hadData = Test-Path $data
    $prepared = $false
    $process = $null
    try {
        if ($hadData) { Rename-Item $data (Split-Path $parked -Leaf) }
        $prepared = $true
        $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem"
        $exe = Join-Path $PackageDir 'note.exe'
        $process = Start-Process -FilePath $exe -WorkingDirectory $PackageDir -PassThru
        $null = $process.Handle # keeps ExitCode readable after exit
        $timer = [Diagnostics.Stopwatch]::StartNew()
        while ($true) {
            $process.Refresh()
            if ($process.HasExited) {
                throw ('note.exe exited during startup with code 0x{0:X8}' -f $process.ExitCode)
            }
            if ($process.MainWindowHandle -ne [IntPtr]::Zero) { break }
            if ($timer.Elapsed.TotalSeconds -gt 30) { throw 'note.exe showed no window within 30 seconds.' }
            Start-Sleep -Milliseconds 100
        }
        Start-Sleep -Seconds 2 # let plugins finish loading
        $process.Refresh()
        $title = $process.MainWindowTitle
        $modules = @($process.Modules | ForEach-Object { $_.FileName })
        $PackageDir = Split-Path $process.MainModule.FileName # long form, as module paths are
        $fromMsys = @($modules | Where-Object { Test-IsInside $_ $Root })
        $fromPackage = @($modules | Where-Object { Test-IsInside $_ $PackageDir })
        $elsewhere = @($modules | Where-Object {
            -not (Test-IsInside $_ $PackageDir) -and -not (Test-IsInside $_ $env:SystemRoot) })
        Write-Host ("    window after {0:N1} s, title '{1}'" -f $timer.Elapsed.TotalSeconds, $title)
        Write-Host ("    {0} modules loaded: {1} from the package, {2} from MSYS2, {3} outside Windows and the package" -f
            $modules.Count, $fromPackage.Count, $fromMsys.Count, $elsewhere.Count)
        foreach ($module in $elsewhere) { Write-Host "      outside: $module" }
        if ($fromMsys.Count -gt 0) { throw "note.exe loaded DLLs from MSYS2: $($fromMsys -join ', ')" }
        if ($title -notlike '*Note*') { throw "Unexpected main window title '$title'." }
        if (-not $process.CloseMainWindow() -or -not $process.WaitForExit(15000)) {
            throw 'note.exe did not close when asked.'
        }
        if ($process.ExitCode -ne 0) { throw "note.exe exited with code $($process.ExitCode)." }
    } finally {
        if ($process -and -not $process.HasExited) { $process.Kill(); $process.WaitForExit(5000) | Out-Null }
        $env:PATH = $savedPath
        foreach ($name in $savedQt.Keys) { [Environment]::SetEnvironmentVariable($name, $savedQt[$name], 'Process') }
        if ($prepared) {
            if (Test-Path $data) { Remove-Item $data -Recurse -Force }
            if ($hadData) { Rename-Item $parked (Split-Path $data -Leaf) }
        }
    }
}

try {
    $started = Get-Date
    $repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

    Write-Step 'Version'
    $version = (Get-Content (Join-Path $repo 'VERSION') -TotalCount 1).Trim()
    if ($version -notmatch '^\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?$') { throw "VERSION holds '$version', not a version." }
    $name = "Note-$version-windows-x64"
    Write-Host "    $version"

    Write-Step 'Toolchain'
    $root = Find-Msys2Root $Msys2Root
    $bin = Join-Path $root 'ucrt64\bin'
    $tools = [ordered]@{}
    foreach ($tool in 'g++', 'cmake', 'ctest', 'ninja', 'windeployqt6', 'qtpaths6', 'objdump') {
        $path = Join-Path $bin "$tool.exe"
        if (-not (Test-Path $path)) { throw "Missing $path. Install the packages listed in docs/RELEASE.md." }
        $tools[$tool] = $path
    }
    $pacman = Join-Path $root 'usr\bin\pacman.exe'
    if (-not (Test-Path $pacman)) { throw "Missing $pacman." }
    $git = (Get-Command git.exe -ErrorAction SilentlyContinue)
    if (-not $git) { throw 'git.exe is not on PATH.' }
    $cmakeVersion = [version]((@(Get-NativeOutput $tools['cmake'] @('--version'))[0] -split ' ')[2] -replace '-.*$', '')
    if ($cmakeVersion -lt [version]'3.24') { throw "CMake $cmakeVersion is older than 3.24." }
    $qtVersion = [version](@(Get-NativeOutput $tools['qtpaths6'] @('--qt-version'))[0])
    if ($qtVersion -lt [version]'6.5') { throw "Qt $qtVersion is older than 6.5." }
    Write-Host "    MSYS2:  $root"
    Write-Host "    $(@(Get-NativeOutput $tools['g++'] @('--version'))[0])"
    Write-Host "    CMake $cmakeVersion, Ninja $(@(Get-NativeOutput $tools['ninja'] @('--version'))[0]), Qt $qtVersion"
    Write-Host "    PowerShell $($PSVersionTable.PSVersion), $(@(Get-NativeOutput $git.Source @('--version'))[0])"

    Write-Step 'Installer compiler'
    $iscc = Find-InnoSetupCompiler $InnoSetupCompiler
    Write-Host "    $iscc"

    Write-Step 'Source tree'
    $dirty = @(Get-NativeOutput $git.Source @('-C', $repo, 'status', '--porcelain'))
    if ($dirty.Count -gt 0) {
        throw "The working tree is not clean; commit or stash first:`n$($dirty -join "`n")"
    }
    $commit = @(Get-NativeOutput $git.Source @('-C', $repo, 'rev-parse', 'HEAD'))[0]
    Write-Host "    clean at $commit"

    if (-not $OutputRoot) { $OutputRoot = Join-Path (Split-Path $repo -Parent) 'note-release' }
    $OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
    if (Test-IsInside $OutputRoot $repo) { throw "OutputRoot $OutputRoot is inside the source folder." }
    $work = Join-Path $OutputRoot "work\$version"
    $build = Join-Path $work 'build'
    $stage = Join-Path $work "Note-$version" # the folder inside the zip
    $zip = Join-Path $OutputRoot "$name.zip"
    $checksum = "$zip.sha256"
    $notes = Join-Path $OutputRoot "Note-$version-RELEASE_NOTES.md"
    $setup = Join-Path $OutputRoot "Note-$version-windows-x64-Setup.exe"
    $setupChecksum = "$setup.sha256"
    foreach ($old in $work, $zip, $checksum, $notes, $setup, $setupChecksum) {
        if (Test-Path $old) { Remove-Item $old -Recurse -Force }
    }
    New-Item -ItemType Directory -Force $work | Out-Null
    $env:PATH = "$bin;$env:PATH"

    Write-Step "Configure (clean Release build in $build)"
    Invoke-Native $tools['cmake'] @('-S', $repo, '-B', $build, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=ON',
        "-DCMAKE_CXX_COMPILER=$($tools['g++'])", "-DCMAKE_MAKE_PROGRAM=$($tools['ninja'])",
        "-DCMAKE_PREFIX_PATH=$(Join-Path $root 'ucrt64')")

    Write-Step "Build ($Jobs jobs)"
    Invoke-Native $tools['cmake'] @('--build', $build, '--parallel', "$Jobs")

    Write-Step 'Test'
    Invoke-Native $tools['ctest'] @('--test-dir', $build, '--output-on-failure', '--timeout', '900')

    Write-Step 'Deploy'
    Invoke-Native $tools['cmake'] @('--install', $build, '--prefix', $stage)
    $productVersion = (Get-Item (Join-Path $stage 'note.exe')).VersionInfo.ProductVersion
    if ($productVersion -ne $version) { throw "note.exe reports version '$productVersion', not '$version'." }
    if (-not (Select-String -Path (Join-Path $stage 'RELEASE_NOTES.md') -SimpleMatch "Note $version release notes" -Quiet)) {
        throw 'RELEASE_NOTES.md does not name this version.'
    }
    $packages = Add-LicenseNotices $stage $root $version
    $files = @(Get-ChildItem $stage -Recurse -File)
    Write-Host ("    {0} files, {1:N1} MiB, licence notices for {2} packages; note.exe version {3}" -f
        $files.Count, (($files | Measure-Object Length -Sum).Sum / 1MB), $packages, $productVersion)

    Write-Step 'Package'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [IO.Compression.ZipFile]::CreateFromDirectory($stage, $zip,
        [IO.Compression.CompressionLevel]::Optimal, $true)
    $hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -Path $checksum -Value "$hash  $name.zip" -Encoding Ascii
    Copy-Item (Join-Path $stage 'RELEASE_NOTES.md') $notes

    Write-Step 'Build installer'
    $icon = Join-Path $repo 'packaging\note.ico'
    $installerScript = Join-Path $repo 'packaging\Note.iss'
    Invoke-Native $iscc @(
        "/DAppVersion=$version",
        "/DSourceDir=$stage",
        "/DOutputDir=$OutputRoot",
        "/DIconFile=`"$icon`"",
        $installerScript
    )
    $setupHash = (Get-FileHash $setup -Algorithm SHA256).Hash.ToLowerInvariant()
    Set-Content -Path $setupChecksum -Value "$setupHash  $(Split-Path $setup -Leaf)" -Encoding Ascii

    Write-Step 'Launch check (unzipped package, MSYS2 not on PATH)'
    $check = Join-Path $work 'launch-check'
    [IO.Compression.ZipFile]::ExtractToDirectory($zip, $check)
    Test-PackageLaunch (Join-Path $check "Note-$version") $root

    Write-Step ('Done in {0:N1} minutes' -f ((Get-Date) - $started).TotalMinutes)
    Write-Host ("    {0} ({1:N1} MiB)" -f $zip, ((Get-Item $zip).Length / 1MB))
    Write-Host "    $checksum"
    Write-Host "    SHA-256 $hash"
    Write-Host "    $setup"
    Write-Host "    $setupChecksum"
    Write-Host "    Installer SHA-256 $setupHash"
    Write-Host "    $notes"
    exit 0
} catch {
    Write-Host "release: FAILED: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
