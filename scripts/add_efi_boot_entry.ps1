param(
    [string]$EfiPath,

    [string]$EntryName = "ELF",
    [string]$TargetDir = "EFI\\elf",
    [string]$TargetFile = "elfhv.efi",
    [string]$PromptTaskName = "elfhv-BootPrompt",
    [string]$PromptScriptName = "elfhv_boot_prompt.ps1",
    [switch]$SetDefault = $true,
    [switch]$InstallBootPrompt = $true,
    [switch]$Reboot = $true
)

function Fail($Message) {
    Write-Error $Message
    exit 1
}

function Find-EfiPath {
    $candidates = @()
    $candidates += "C:\\Users\\admin\\Downloads\\elfhv.efi"
    $candidates += Join-Path $env:USERPROFILE "Downloads\\elfhv.efi"
    if ($PSScriptRoot) {
        $candidates += Join-Path $PSScriptRoot "elfhv.efi"
        $candidates += (Resolve-Path (Join-Path $PSScriptRoot "..\\build\\elfhv.efi") -ErrorAction SilentlyContinue).Path
    }
    foreach ($c in $candidates) {
        if ($c -and (Test-Path -LiteralPath $c)) {
            return $c
        }
    }
    return $null
}

if (-not $EfiPath) {
    $EfiPath = Find-EfiPath
    if ($EfiPath) {
        Write-Host "EfiPath not provided. Using: $EfiPath"
    }
}

if (-not $EfiPath -or -not (Test-Path -LiteralPath $EfiPath)) {
    $EfiPath = Read-Host "EFI file not found. Enter full path to your EFI"
}

if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
    ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Fail "Run this script in an elevated PowerShell (Administrator)."
}

if (-not (Test-Path -LiteralPath $EfiPath)) {
    Fail "EFI file not found: $EfiPath"
}

function Get-EspDriveLetter {
    $espGuid = "{C12A7328-F81F-11D2-BA4B-00A0C93EC93B}"
    $esp = Get-Partition | Where-Object { $_.GptType -eq $espGuid } | Select-Object -First 1
    if (-not $esp) { return $null }
    if ($esp.DriveLetter) { return $esp.DriveLetter }

    $used = (Get-Volume | Where-Object { $_.DriveLetter }) | ForEach-Object { $_.DriveLetter }
    foreach ($letter in [char[]]([char]'Z'..[char]'D')) {
        if ($used -notcontains $letter) {
            Set-Partition -DiskNumber $esp.DiskNumber -PartitionNumber $esp.PartitionNumber -NewDriveLetter $letter | Out-Null
            return $letter
        }
    }
    return $null
}

function Install-BootPromptTask {
    $promptScript = Join-Path $PSScriptRoot $PromptScriptName
    if (-not (Test-Path -LiteralPath $promptScript)) {
        Fail "Prompt script not found: $promptScript"
    }

    $action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument "-NoProfile -ExecutionPolicy Bypass -File `"$promptScript`""
    $trigger = New-ScheduledTaskTrigger -AtLogOn
    $principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -RunLevel Highest

    try {
        Register-ScheduledTask -TaskName $PromptTaskName -Action $action -Trigger $trigger -Principal $principal -Force | Out-Null
        Write-Host "Scheduled task created: $PromptTaskName"
        return
    } catch {
        Write-Host "Register-ScheduledTask failed, falling back to schtasks.exe"
    }

    $taskCmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$promptScript`""
    $schtasks = @(
        "/Create",
        "/TN", $PromptTaskName,
        "/TR", $taskCmd,
        "/SC", "ONLOGON",
        "/RL", "HIGHEST",
        "/F"
    )
    $proc = Start-Process -FilePath "schtasks.exe" -ArgumentList $schtasks -Wait -PassThru -NoNewWindow
    if ($proc.ExitCode -ne 0) {
        Fail "Failed to create scheduled task using schtasks.exe. Exit code: $($proc.ExitCode)"
    }
    Write-Host "Scheduled task created: $PromptTaskName"
}

$drive = Get-EspDriveLetter
if (-not $drive) {
    Fail "Failed to locate or mount the EFI System Partition. Ensure the VM is booting in UEFI mode and an ESP exists."
}
$mount = "$drive`:"
Write-Host "Using ESP at $mount"

$destDir = "$mount\\$TargetDir"
if (-not (Test-Path -LiteralPath $destDir)) {
    New-Item -ItemType Directory -Path $destDir | Out-Null
}

$destPath = "$destDir\\$TargetFile"
Copy-Item -LiteralPath $EfiPath -Destination $destPath -Force

Write-Host "Creating boot entry: $EntryName"
$createOut = bcdedit /create /d $EntryName /application EFI 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "bcdedit /create with /application EFI failed. Trying BOOTAPP..."
    $createOut = bcdedit /create /d $EntryName /application BOOTAPP 2>&1
}
if ($LASTEXITCODE -ne 0) {
    Write-Host "bcdedit /create with BOOTAPP failed. Trying to copy {bootmgr}..."
    $createOut = bcdedit /copy {bootmgr} /d $EntryName 2>&1
    if ($LASTEXITCODE -ne 0) {
        Fail "bcdedit /create failed. Output: $createOut"
    }
}

$guidMatch = [regex]::Match(($createOut -join " "), "\{[0-9a-fA-F-]+\}")
$guid = $guidMatch.Value
if (-not $guid) {
    Fail "Could not parse GUID from bcdedit output. Output: $createOut"
}

bcdedit /set $guid device partition=$mount | Out-Null
bcdedit /set $guid path "\\$TargetDir\\$TargetFile" | Out-Null
bcdedit /displayorder $guid /addfirst | Out-Null
if ($SetDefault) {
    bcdedit /default $guid | Out-Null
    bcdedit /timeout 0 | Out-Null
}

Write-Host "Boot entry created: $guid"
Write-Host "EFI copied to: $destPath"
if ($InstallBootPrompt) {
    Install-BootPromptTask
}
if ($Reboot) {
    Write-Host "Rebooting now..."
    shutdown /r /t 0 | Out-Null
} else {
    Write-Host "Use the boot menu to select '$EntryName' on next reboot."
}
