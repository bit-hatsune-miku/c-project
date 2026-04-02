param(
    [Parameter(Mandatory = $true)]
    [string]$ExePath,
    [string]$ObjdumpPath,
    [string[]]$SearchDirectories = @()
)

$ErrorActionPreference = "Stop"

function Resolve-ExistingPath {
    param([string]$PathValue)

    if (-not $PathValue) {
        return $null
    }

    if (Test-Path -LiteralPath $PathValue) {
        return (Resolve-Path -LiteralPath $PathValue).Path
    }

    return $null
}

function Get-ObjdumpPath {
    param([string]$HintPath)

    $candidates = New-Object System.Collections.Generic.List[string]
    if ($HintPath) {
        $candidates.Add($HintPath)
    }
    if ($env:MSYSTEM_PREFIX) {
        $candidates.Add((Join-Path $env:MSYSTEM_PREFIX "bin\\objdump.exe"))
    }
    $candidates.Add("C:\msys64\ucrt64\bin\objdump.exe")
    $candidates.Add("C:\msys64\mingw64\bin\objdump.exe")

    foreach ($candidate in $candidates) {
        $resolved = Resolve-ExistingPath $candidate
        if ($resolved) {
            return $resolved
        }
    }

    $command = Get-Command objdump.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    throw "Could not find objdump.exe. Install the MSYS2 UCRT64 toolchain or pass -ObjdumpPath explicitly."
}

function Get-ImportedDllNames {
    param(
        [string]$BinaryPath,
        [string]$ObjdumpExe
    )

    $dllNames = New-Object System.Collections.Generic.List[string]
    $output = & $ObjdumpExe -p $BinaryPath 2>&1

    foreach ($line in $output) {
        if ($line -match "DLL Name:\s+(.+)$") {
            $dllNames.Add($matches[1].Trim())
        }
    }

    return $dllNames
}

function Find-DllPath {
    param(
        [string]$DllName,
        [string[]]$Directories
    )

    foreach ($directory in $Directories) {
        if (-not $directory) {
            continue
        }

        $candidate = Join-Path $directory $DllName
        if (Test-Path -LiteralPath $candidate) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    return $null
}

function Should-SkipDllName {
    param([string]$DllName)

    return $DllName -like "api-ms-win-*" -or $DllName -like "ext-ms-*"
}

$resolvedExePath = Resolve-ExistingPath $ExePath
if (-not $resolvedExePath) {
    throw "Executable not found: $ExePath"
}

$objdumpExe = Get-ObjdumpPath $ObjdumpPath
$exeDirectory = Split-Path -Parent $resolvedExePath
$buildDirectory = Split-Path -Parent $exeDirectory

$pathSearchDirectories = @()
if ($env:PATH) {
    $pathSearchDirectories = $env:PATH.Split(";") | Where-Object { $_ -and $_.Trim() -ne "" }
}

$allSearchDirectories = @(
    $exeDirectory
    $buildDirectory
    $SearchDirectories
    $pathSearchDirectories
    "$env:SystemRoot\System32"
    "$env:SystemRoot\SysWOW64"
) | ForEach-Object { Resolve-ExistingPath $_ } | Where-Object { $_ } | Select-Object -Unique

Write-Host "Checking runtime dependencies for: $resolvedExePath"
Write-Host "Using objdump: $objdumpExe"
Write-Host "Search directories:"
foreach ($directory in $allSearchDirectories) {
    Write-Host "  $directory"
}

$queue = New-Object System.Collections.Generic.Queue[string]
$queue.Enqueue($resolvedExePath)

$visitedBinaries = New-Object System.Collections.Generic.HashSet[string]([System.StringComparer]::OrdinalIgnoreCase)
$missingDeps = New-Object System.Collections.Generic.List[string]

while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $visitedBinaries.Add($binary)) {
        continue
    }

    $imports = Get-ImportedDllNames -BinaryPath $binary -ObjdumpExe $objdumpExe
    $binaryName = Split-Path -Leaf $binary

    foreach ($dllName in $imports) {
        if (Should-SkipDllName $dllName) {
            Write-Host ("[APISET] {0} -> {1}" -f $binaryName, $dllName)
            continue
        }

        $resolvedDll = Find-DllPath -DllName $dllName -Directories $allSearchDirectories

        if ($resolvedDll) {
            Write-Host ("[OK] {0} -> {1} ({2})" -f $binaryName, $dllName, $resolvedDll)

            $resolvedDir = Split-Path -Parent $resolvedDll
            if ($resolvedDir -notlike "$env:SystemRoot\System32*" -and $resolvedDir -notlike "$env:SystemRoot\SysWOW64*") {
                $queue.Enqueue($resolvedDll)
            }
        } else {
            $message = ("[MISSING] {0} -> {1}" -f $binaryName, $dllName)
            Write-Host $message
            $missingDeps.Add($message)
        }
    }
}

if ($missingDeps.Count -gt 0) {
    Write-Host ""
    Write-Host "Missing runtime dependencies:"
    foreach ($missing in $missingDeps | Select-Object -Unique) {
        Write-Host "  $missing"
    }
    exit 1
}

Write-Host ""
Write-Host "All runtime dependencies resolved."
