param(
    [string]$TaskName = "elfhv-BootPrompt"
)

function Fail($Message) {
    Write-Error $Message
    exit 1
}

if (-not ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
    ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Fail "Run this script in an elevated PowerShell (Administrator)."
}

$promptScript = Join-Path $PSScriptRoot "elfhv_boot_prompt.ps1"
if (-not (Test-Path -LiteralPath $promptScript)) {
    Fail "Prompt script not found: $promptScript"
}

$action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument "-NoProfile -ExecutionPolicy Bypass -File `"$promptScript`""
$trigger = New-ScheduledTaskTrigger -AtLogOn
$principal = New-ScheduledTaskPrincipal -UserId $env:USERNAME -RunLevel Highest

try {
    Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger -Principal $principal -Force | Out-Null
    Write-Host "Scheduled task created: $TaskName"
    exit 0
} catch {
    Write-Host "Register-ScheduledTask failed, falling back to schtasks.exe"
}

$taskCmd = "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$promptScript`""
$schtasks = @(
    "/Create",
    "/TN", $TaskName,
    "/TR", $taskCmd,
    "/SC", "ONLOGON",
    "/RL", "HIGHEST",
    "/F"
)

$proc = Start-Process -FilePath "schtasks.exe" -ArgumentList $schtasks -Wait -PassThru -NoNewWindow
if ($proc.ExitCode -ne 0) {
    Fail "Failed to create scheduled task using schtasks.exe. Exit code: $($proc.ExitCode)"
}

Write-Host "Scheduled task created: $TaskName"
