param(
    [string]$Guid = "{3f7a9fe2-0e84-4de4-8692-a29f31666ab5}",
    [string]$VarName = "elfhvBooted",
    [string]$Message = "elf-nigel runs windows"
)

$signature = @"
using System;
using System.Runtime.InteropServices;
public static class FirmwareEnv {
  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
  public static extern uint GetFirmwareEnvironmentVariable(string lpName, string lpGuid, byte[] pBuffer, uint nSize);

  [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Unicode)]
  public static extern bool SetFirmwareEnvironmentVariable(string lpName, string lpGuid, byte[] pBuffer, uint nSize);
}
"@

try {
    Add-Type -TypeDefinition $signature -ErrorAction Stop | Out-Null
    Add-Type -AssemblyName System.Windows.Forms -ErrorAction Stop | Out-Null
    Add-Type -AssemblyName System.Drawing -ErrorAction Stop | Out-Null
} catch {
    exit 0
}

$bootedBuf = New-Object byte[] 4
$bootedLen = [FirmwareEnv]::GetFirmwareEnvironmentVariable($VarName, $Guid, $bootedBuf, $bootedBuf.Length)
$bootedFlag = ($bootedLen -gt 0 -and $bootedBuf[0] -ne 0)
if (-not $bootedFlag) {
    exit 0
}

$cfgName = "elfhvConfig"
$cfgLen = 22
$cfg = New-Object byte[] $cfgLen
$cfgLenRead = [FirmwareEnv]::GetFirmwareEnvironmentVariable($cfgName, $Guid, $cfg, $cfg.Length)
if ($cfgLenRead -lt $cfgLen) {
    $cfg = [byte[]](1,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,1,0,1,0,3,0)
}

function Write-Config($arr) {
    [FirmwareEnv]::SetFirmwareEnvironmentVariable($cfgName, $Guid, $arr, $arr.Length) | Out-Null
}

$debugText = @"
debug info
booted flag : $bootedFlag
config      : hv=$($cfg[0]) mode=$($cfg[1]) vmx=$($cfg[2]) svm=$($cfg[3]) ept=$($cfg[4]) npt=$($cfg[5]) vpid=$($cfg[6])
            hooks=$($cfg[7]) syscall=$($cfg[8]) msr=$($cfg[9]) cpuid=$($cfg[10]) memint=$($cfg[11]) guard=$($cfg[12])
            protect=$($cfg[13]) antitamper=$($cfg[14]) stealth=$($cfg[15]) verbose=$($cfg[16]) serial=$($cfg[17])
            log=$($cfg[18]) vmexit=$($cfg[19]) sample=$($cfg[20]) bootdelay=$($cfg[21])
guid        : $Guid
var booted  : $VarName
var config  : $cfgName
"@
[System.Windows.Forms.MessageBox]::Show($debugText, "debug", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information) | Out-Null

$osInfo = ""
try {
    $os = Get-CimInstance -ClassName Win32_OperatingSystem
    $uptime = (Get-Date) - $os.LastBootUpTime
    $osInfo = @"
system info
user        : $env:USERNAME
computer    : $env:COMPUTERNAME
os          : $($os.Caption)
version     : $($os.Version)
build       : $($os.BuildNumber)
uptime      : $([int]$uptime.TotalHours)h $($uptime.Minutes)m
"@
} catch {
    $osInfo = "system info`nunavailable"
}
[System.Windows.Forms.MessageBox]::Show($osInfo, "system", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information) | Out-Null

$bootInfo = ""
try {
    $secureBoot = $null
    try { $secureBoot = Confirm-SecureBootUEFI } catch { $secureBoot = "unknown" }
    $bootInfo = @"
boot info
secure boot : $secureBoot
time        : $(Get-Date -Format "yyyy-MM-dd hh:mm:ss")
"@
} catch {
    $bootInfo = "boot info`nunavailable"
}
[System.Windows.Forms.MessageBox]::Show($bootInfo, "boot", [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Information) | Out-Null

[FirmwareEnv]::SetFirmwareEnvironmentVariable($VarName, $Guid, $null, 0) | Out-Null

$form = New-Object System.Windows.Forms.Form
$form.Text = "elf hv"
$form.StartPosition = "CenterScreen"
$form.Size = New-Object System.Drawing.Size(560, 540)
$form.FormBorderStyle = "FixedDialog"
$form.MaximizeBox = $false
$form.MinimizeBox = $false

$title = New-Object System.Windows.Forms.Label
$title.Text = "elf hypervisor"
$title.Font = New-Object System.Drawing.Font("segoe ui", 14, [System.Drawing.FontStyle]::Bold)
$title.AutoSize = $true
$title.Location = New-Object System.Drawing.Point(18, 16)
$form.Controls.Add($title)

$credits = New-Object System.Windows.Forms.Label
$credits.Text = "credits: elf-nigel, elf-icarus"
$credits.Font = New-Object System.Drawing.Font("segoe ui", 9)
$credits.AutoSize = $true
$credits.Location = New-Object System.Drawing.Point(20, 46)
$form.Controls.Add($credits)

$status = New-Object System.Windows.Forms.Label
$status.Text = "hv active. changes apply next reboot."
$status.Font = New-Object System.Drawing.Font("segoe ui", 9)
$status.AutoSize = $true
$status.Location = New-Object System.Drawing.Point(20, 70)
$form.Controls.Add($status)

$panelCore = New-Object System.Windows.Forms.GroupBox
$panelCore.Text = "core"
$panelCore.Location = New-Object System.Drawing.Point(18, 100)
$panelCore.Size = New-Object System.Drawing.Size(260, 150)
$form.Controls.Add($panelCore)

$panelMem = New-Object System.Windows.Forms.GroupBox
$panelMem.Text = "memory"
$panelMem.Location = New-Object System.Drawing.Point(290, 100)
$panelMem.Size = New-Object System.Drawing.Size(240, 150)
$form.Controls.Add($panelMem)

$panelHooks = New-Object System.Windows.Forms.GroupBox
$panelHooks.Text = "hooks"
$panelHooks.Location = New-Object System.Drawing.Point(18, 260)
$panelHooks.Size = New-Object System.Drawing.Size(260, 150)
$form.Controls.Add($panelHooks)

$panelProtect = New-Object System.Windows.Forms.GroupBox
$panelProtect.Text = "protection"
$panelProtect.Location = New-Object System.Drawing.Point(290, 260)
$panelProtect.Size = New-Object System.Drawing.Size(240, 150)
$form.Controls.Add($panelProtect)

$panelDebug = New-Object System.Windows.Forms.GroupBox
$panelDebug.Text = "debug"
$panelDebug.Location = New-Object System.Drawing.Point(18, 420)
$panelDebug.Size = New-Object System.Drawing.Size(512, 80)
$form.Controls.Add($panelDebug)

function New-Check($panel, $text, $x, $y, $checked) {
    $cb = New-Object System.Windows.Forms.CheckBox
    $cb.Text = $text
    $cb.Location = New-Object System.Drawing.Point($x, $y)
    $cb.AutoSize = $true
    $cb.Checked = $checked
    $panel.Controls.Add($cb)
    return $cb
}

$cbHv = New-Check $panelCore "enable hv" 12 24 ($cfg[0] -ne 0)
$cbVmx = New-Check $panelCore "enable vmx" 12 48 ($cfg[2] -ne 0)
$cbSvm = New-Check $panelCore "enable svm" 12 72 ($cfg[3] -ne 0)
$cbVpid = New-Check $panelCore "enable vpid" 12 96 ($cfg[6] -ne 0)

$modeLabel = New-Object System.Windows.Forms.Label
$modeLabel.Text = "hv mode"
$modeLabel.AutoSize = $true
$modeLabel.Location = New-Object System.Drawing.Point(140, 26)
$panelCore.Controls.Add($modeLabel)

$modeBox = New-Object System.Windows.Forms.ComboBox
$modeBox.Location = New-Object System.Drawing.Point(140, 44)
$modeBox.Size = New-Object System.Drawing.Size(100, 20)
$modeBox.DropDownStyle = "DropDownList"
$modeBox.Items.AddRange(@("auto","vmx","svm")) | Out-Null
$modeBox.SelectedIndex = [int]$cfg[1]
$panelCore.Controls.Add($modeBox)

$cbEpt = New-Check $panelMem "enable ept" 12 24 ($cfg[4] -ne 0)
$cbNpt = New-Check $panelMem "enable npt" 12 48 ($cfg[5] -ne 0)
$cbMemInt = New-Check $panelMem "mem integrity" 12 72 ($cfg[11] -ne 0)
$cbGuard = New-Check $panelMem "guard pages" 12 96 ($cfg[12] -ne 0)

$cbHooks = New-Check $panelHooks "hooks master" 12 24 ($cfg[7] -ne 0)
$cbHookSys = New-Check $panelHooks "hook syscall" 12 48 ($cfg[8] -ne 0)
$cbHookMsr = New-Check $panelHooks "hook msr" 12 72 ($cfg[9] -ne 0)
$cbHookCpuid = New-Check $panelHooks "hook cpuid" 12 96 ($cfg[10] -ne 0)

$cbSelf = New-Check $panelProtect "self protect" 12 24 ($cfg[13] -ne 0)
$cbAnti = New-Check $panelProtect "anti tamper" 12 48 ($cfg[14] -ne 0)
$cbStealth = New-Check $panelProtect "stealth" 12 72 ($cfg[15] -ne 0)

$cbVerbose = New-Check $panelDebug "verbose" 12 24 ($cfg[16] -ne 0)
$cbSerial = New-Check $panelDebug "serial debug" 100 24 ($cfg[17] -ne 0)
$cbVmexit = New-Check $panelDebug "vmexit logging" 220 24 ($cfg[19] -ne 0)

$logLabel = New-Object System.Windows.Forms.Label
$logLabel.Text = "log level"
$logLabel.AutoSize = $true
$logLabel.Location = New-Object System.Drawing.Point(12, 50)
$panelDebug.Controls.Add($logLabel)

$logBox = New-Object System.Windows.Forms.ComboBox
$logBox.Location = New-Object System.Drawing.Point(70, 46)
$logBox.Size = New-Object System.Drawing.Size(80, 20)
$logBox.DropDownStyle = "DropDownList"
$logBox.Items.AddRange(@("off","error","info","debug")) | Out-Null
$logBox.SelectedIndex = [int]$cfg[18]
$panelDebug.Controls.Add($logBox)

$sampleLabel = New-Object System.Windows.Forms.Label
$sampleLabel.Text = "vmexit sample"
$sampleLabel.AutoSize = $true
$sampleLabel.Location = New-Object System.Drawing.Point(170, 50)
$panelDebug.Controls.Add($sampleLabel)

$sampleNum = New-Object System.Windows.Forms.NumericUpDown
$sampleNum.Location = New-Object System.Drawing.Point(260, 46)
$sampleNum.Size = New-Object System.Drawing.Size(50, 20)
$sampleNum.Minimum = 0
$sampleNum.Maximum = 10
$sampleNum.Value = [int]$cfg[20]
$panelDebug.Controls.Add($sampleNum)

$delayLabel = New-Object System.Windows.Forms.Label
$delayLabel.Text = "boot delay"
$delayLabel.AutoSize = $true
$delayLabel.Location = New-Object System.Drawing.Point(330, 50)
$panelDebug.Controls.Add($delayLabel)

$delayNum = New-Object System.Windows.Forms.NumericUpDown
$delayNum.Location = New-Object System.Drawing.Point(400, 46)
$delayNum.Size = New-Object System.Drawing.Size(50, 20)
$delayNum.Minimum = 0
$delayNum.Maximum = 50
$delayNum.Value = [int]$cfg[21]
$panelDebug.Controls.Add($delayNum)

$btnSave = New-Object System.Windows.Forms.Button
$btnSave.Text = "save"
$btnSave.Location = New-Object System.Drawing.Point(20, 290)
$btnSave.Size = New-Object System.Drawing.Size(90, 28)
$btnSave.Add_Click({
    $arr = [byte[]]@(
        [byte]($cbHv.Checked),
        [byte]($modeBox.SelectedIndex),
        [byte]($cbVmx.Checked),
        [byte]($cbSvm.Checked),
        [byte]($cbEpt.Checked),
        [byte]($cbNpt.Checked),
        [byte]($cbVpid.Checked),
        [byte]($cbHooks.Checked),
        [byte]($cbHookSys.Checked),
        [byte]($cbHookMsr.Checked),
        [byte]($cbHookCpuid.Checked),
        [byte]($cbMemInt.Checked),
        [byte]($cbGuard.Checked),
        [byte]($cbSelf.Checked),
        [byte]($cbAnti.Checked),
        [byte]($cbStealth.Checked),
        [byte]($cbVerbose.Checked),
        [byte]($cbSerial.Checked),
        [byte]($logBox.SelectedIndex),
        [byte]($cbVmexit.Checked),
        [byte]($sampleNum.Value),
        [byte]($delayNum.Value)
    )
    Write-Config $arr
    $status.Text = "saved. changes apply next reboot."
})
$form.Controls.Add($btnSave)

$btnSaveReboot = New-Object System.Windows.Forms.Button
$btnSaveReboot.Text = "save and reboot"
$btnSaveReboot.Location = New-Object System.Drawing.Point(120, 290)
$btnSaveReboot.Size = New-Object System.Drawing.Size(120, 28)
$btnSaveReboot.Add_Click({
    $arr = [byte[]]@(
        [byte]($cbHv.Checked),
        [byte]($modeBox.SelectedIndex),
        [byte]($cbVmx.Checked),
        [byte]($cbSvm.Checked),
        [byte]($cbEpt.Checked),
        [byte]($cbNpt.Checked),
        [byte]($cbVpid.Checked),
        [byte]($cbHooks.Checked),
        [byte]($cbHookSys.Checked),
        [byte]($cbHookMsr.Checked),
        [byte]($cbHookCpuid.Checked),
        [byte]($cbMemInt.Checked),
        [byte]($cbGuard.Checked),
        [byte]($cbSelf.Checked),
        [byte]($cbAnti.Checked),
        [byte]($cbStealth.Checked),
        [byte]($cbVerbose.Checked),
        [byte]($cbSerial.Checked),
        [byte]($logBox.SelectedIndex),
        [byte]($cbVmexit.Checked),
        [byte]($sampleNum.Value),
        [byte]($delayNum.Value)
    )
    Write-Config $arr
    shutdown /r /t 0 | Out-Null
})
$form.Controls.Add($btnSaveReboot)

$btnDisableReboot = New-Object System.Windows.Forms.Button
$btnDisableReboot.Text = "disable and reboot"
$btnDisableReboot.Location = New-Object System.Drawing.Point(250, 290)
$btnDisableReboot.Size = New-Object System.Drawing.Size(140, 28)
$btnDisableReboot.Add_Click({
    $arr = [byte[]](0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0)
    Write-Config $arr
    shutdown /r /t 0 | Out-Null
})
$form.Controls.Add($btnDisableReboot)

$btnClose = New-Object System.Windows.Forms.Button
$btnClose.Text = "close"
$btnClose.Location = New-Object System.Drawing.Point(20, 324)
$btnClose.Size = New-Object System.Drawing.Size(90, 28)
$btnClose.Add_Click({ $form.Close() })
$form.Controls.Add($btnClose)

$anim = New-Object System.Windows.Forms.Timer
$anim.Interval = 300
$dots = 0
$anim.Add_Tick({
    $dots = ($dots + 1) % 4
    $credits.Text = "credits: elf-nigel, elf-icarus" + ("." * $dots)
})
$anim.Start()

$form.Add_FormClosed({ $anim.Stop() })
$form.ShowDialog() | Out-Null
