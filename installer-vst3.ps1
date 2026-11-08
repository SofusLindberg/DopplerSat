# installer-vst3.ps1 - kopierer det nyeste Release-build af Pitch Bend Texture (1.0-1.2: Doppler Saturator) til VST3-mappen.
# Hver version har sit eget navn ("Doppler Saturator 1.0.vst3", "Pitch Bend Texture 1.3.vst3" osv.) og sin egen plugin-kode, saa den
# installeres VED SIDEN AF de gamle versioner. Gamle versioner slettes aldrig. Kun ASCII i denne fil.
# Skal koeres som administrator (Program Files er skrivebeskyttet). Scriptet beder selv om det (UAC).
# -NoPause: vent ikke paa Enter (bruges naar Claude koerer scriptet).
param([switch]$NoPause)
$ErrorActionPreference = 'Stop'

# Ableton: alle parametre kan automatiseres uden at rette i en fil (som Multiband Saturator 3.18). Doppler
# Saturator har faa parametre, men det skader ikke. Koeres foer UAC-skiftet (den rigtige brugers APPDATA).
. (Join-Path $PSScriptRoot 'ableton-automation.ps1')
try { Set-AbletonAutomation } catch { Write-Host "Ableton Options.txt: $($_.Exception.Message)" -ForegroundColor Yellow }

$erAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $erAdmin)
{
    Write-Host 'Har ikke administratorrettigheder - godkend UAC-boksen...'
    $args2 = @('-ExecutionPolicy', 'Bypass', '-File', "`"$PSCommandPath`"")
    if ($NoPause) { $args2 += '-NoPause' }
    Start-Process powershell -Verb RunAs -ArgumentList $args2
    return
}

try
{
    $vst3Dir  = 'C:\Program Files\Common Files\VST3'
    $buildDir = Join-Path $PSScriptRoot 'build\DopplerSat_artefacts\Release\VST3'
    # 1.3: pluginnet hedder nu "Pitch Bend Texture 1.x" (foer "Doppler Saturator 1.x"); det nyeste build vinder
    $kilde = Get-ChildItem $buildDir -Directory -Filter '*.vst3' |
             Sort-Object { (Get-Item (Join-Path $_.FullName "Contents\x86_64-win\$($_.Name)")).LastWriteTime } -Descending |
             Select-Object -First 1
    if (-not $kilde) { throw "Fandt intet build i $buildDir (byg Release foerst)" }
    $navn = $kilde.Name
    $maal = Join-Path $vst3Dir $navn

    if (Test-Path $maal)
    {
        $daw = Get-Process | Where-Object { $_.ProcessName -match '^(Live|Ableton Live.*|reaper|Bitwig Studio|FL64|Studio One|Cubase.*|Reason)$' }
        if ($daw) { throw "$navn findes allerede og skal erstattes - luk foerst: $(($daw | ForEach-Object ProcessName) -join ', ')" }
        Remove-Item $maal -Recurse -Force
    }
    Copy-Item $kilde.FullName $vst3Dir -Recurse

    $rel = "Contents\x86_64-win\$navn"
    if ((Get-FileHash (Join-Path $kilde.FullName $rel)).Hash -ne (Get-FileHash (Join-Path $maal $rel)).Hash) { throw 'Den installerede fil er ikke identisk med buildet' }

    $info    = Get-Content (Join-Path $maal 'Contents\Resources\moduleinfo.json') -Raw
    $version = if ($info -match '"Version":\s*"([^"]+)"') { $matches[1] } else { '?' }
    Write-Host "OK: Installeret $navn version $version i $vst3Dir (verificeret med filhash)" -ForegroundColor Green
    Get-ChildItem $vst3Dir -Directory | Where-Object { $_.Name -match '^(Doppler Saturator|Pitch Bend Texture) .*\.vst3$' } | ForEach-Object { Write-Host "  $($_.Name)" }
}
catch
{
    Write-Host "FEJL: $($_.Exception.Message)" -ForegroundColor Red
}

if (-not $NoPause) { Read-Host 'Tryk Enter for at lukke' | Out-Null }
