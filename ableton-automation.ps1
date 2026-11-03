# ableton-automation.ps1 - saa Ableton Live altid viser alle pluginnets parametre til automation.
# Live viser kun parametrene af sig selv (enhedens panel og automation-listen), naar et plugin har hoejst 64.
# Multiband Saturator har over 100. Linjen "-_PluginAutoPopulateThreshold=128" i Options.txt i hver Live-versions
# Preferences-mappe haever graensen til 128. Brugeren (2026-11-03): "Kan vi sørge for, at Ableton altid kan tilgå
# automations, uden at brugeren skal rette i en fil?" - "Alle 108, installer retter filen".
# Bruges af installer-vst3.ps1 og af installeren i dele-zip'en (dot-source: . .\ableton-automation.ps1).
# Kun ASCII i denne fil (Windows PowerShell 5.1 laeser en .ps1 uden BOM som ANSI).

function Set-AbletonAutomation
{
    $linje  = '-_PluginAutoPopulateThreshold=128'
    $rod    = Join-Path $env:APPDATA 'Ableton'
    $mapper = @()
    if (Test-Path $rod)
    {
        $mapper = Get-ChildItem $rod -Directory | Where-Object { $_.Name -match '^Live \d' -and (Test-Path (Join-Path $_.FullName 'Preferences')) }
    }
    if (-not $mapper)
    {
        Write-Host 'Ableton Live: ingen Live-version fundet for denne bruger. Aabn Live een gang og koer installeren igen,' -ForegroundColor Yellow
        Write-Host '  saa kan alle parametre automatiseres.' -ForegroundColor Yellow
        return
    }
    foreach ($m in $mapper)
    {
        $fil = Join-Path $m.FullName 'Preferences\Options.txt'
        $linjer = @()
        if (Test-Path $fil) { $linjer = @(Get-Content $fil) }
        $fundet = $false
        $ny = foreach ($l in $linjer)
        {
            if ($l -match '^\s*-_PluginAutoPopulateThreshold\s*=\s*(\d+)')
            {
                $fundet = $true
                if ([int]$matches[1] -ge 128) { $l } else { $linje }      # en hoejere graense beholdes
            }
            else { $l }
        }
        if (-not $fundet) { $ny = @($ny) + $linje }
        $ny = @($ny | Where-Object { $_ -ne $null })
        if (($linjer -join "`n") -ne ($ny -join "`n"))
        {
            Set-Content -Path $fil -Value $ny -Encoding Ascii
            Write-Host "Ableton $($m.Name): automation af alle parametre slaaet til (Options.txt). Genstart Live." -ForegroundColor Green
        }
        else
        {
            Write-Host "Ableton $($m.Name): automation af alle parametre var allerede slaaet til."
        }
    }
}
