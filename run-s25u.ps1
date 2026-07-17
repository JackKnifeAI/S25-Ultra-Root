Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ksud = Join-Path $scriptDir 'kernelsu\ksud-s25u-kdp'
$expectedKsudHash = 'fa3edcc7d168637394877b30cb1f909d762dda788ec14051f4ae79edd6562d63'

if (-not (Test-Path -LiteralPath $ksud)) {
    throw 'Missing file: kernelsu\ksud-s25u-kdp'
}
$actualKsudHash = (Get-FileHash -LiteralPath $ksud -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualKsudHash -ne $expectedKsudHash) {
    throw 'SHA-256 mismatch: kernelsu\ksud-s25u-kdp'
}

$adb = (Get-Command adb.exe -ErrorAction Stop).Source

function Invoke-Adb {
    param([string[]]$Arguments)

    & $adb @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "adb failed with exit code $LASTEXITCODE"
    }
}

Write-Host '[*] Verifying bootstrap root'
$rootOutput = & $adb shell "/data/local/tmp/cve-2026-43499-root -c 'id'"
$rootOutput | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0 -or ($rootOutput -join "`n") -notmatch 'uid=0\(root\)') {
    throw 'Bootstrap root is not active; run the exploit first'
}

$currentModules = & $adb shell cat /proc/modules
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to read /proc/modules'
}
if ($currentModules | Select-String -Pattern '^kernelsu\s') {
    throw 'KernelSU is already loaded; reboot before deploying this ABI-matched runtime'
}

Write-Host '[*] Uploading patched KernelSU loader'
Invoke-Adb -Arguments @('push', $ksud, '/data/local/tmp/ksud-s25u-kdp')
Invoke-Adb -Arguments @('shell', 'rm -f /data/local/tmp/.ksud-stage')
Invoke-Adb -Arguments @('push', $ksud, '/data/local/tmp/.ksud-stage')
Invoke-Adb -Arguments @('shell', 'chmod 755 /data/local/tmp/ksud-s25u-kdp /data/local/tmp/.ksud-stage')

Write-Host '[*] Loading KernelSU'
& $adb shell '/data/local/tmp/cve-2026-43499-root --late-load'
$lateLoadExit = $LASTEXITCODE
if ($lateLoadExit -ne 0) {
    throw "KernelSU late-load failed with exit code $lateLoadExit"
}

$selinux = (& $adb shell getenforce).Trim()
$modules = & $adb shell cat /proc/modules
$kernelSuLoaded = [bool]($modules | Select-String -Pattern '^kernelsu\s')
$daemonSizeOutput = & $adb shell "/data/local/tmp/cve-2026-43499-root -c 'stat -c %s /data/adb/ksud'"
$daemonStatExit = $LASTEXITCODE
$daemonSize = 0L
$daemonSizeValid = [long]::TryParse(($daemonSizeOutput -join '').Trim(), [ref]$daemonSize)
$daemonInfo = & $adb shell "/data/local/tmp/cve-2026-43499-root -c 'ls -lZ /data/adb/ksud'"
$daemonInfoExit = $LASTEXITCODE

Write-Host "[*] SELinux: $selinux"
$daemonInfo | ForEach-Object { Write-Host $_ }
if (-not $kernelSuLoaded) {
    throw 'KernelSU module is not present in /proc/modules'
}
if ($daemonStatExit -ne 0 -or -not $daemonSizeValid -or $daemonSize -le 0 -or $daemonInfoExit -ne 0) {
    throw 'KernelSU daemon was not installed correctly'
}

Write-Host '[+] KernelSU late-load completed'
Write-Host '[*] The Manager was not started automatically'
Write-Host '[*] Validate first: adb shell "su -c ''id; pwd; getenforce''"'
