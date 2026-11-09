# serve.ps1 - viser hjemmeside-mockuppet paa http://localhost:8765/ (en lille statisk server, kun til test).
# Koer: powershell -ExecutionPolicy Bypass -File website\serve.ps1   (stop med Ctrl+C). Kun ASCII i denne fil.
param([int]$Port = 8765)
$root = $PSScriptRoot
$types = @{ '.html' = 'text/html; charset=utf-8'; '.css' = 'text/css'; '.js' = 'text/javascript';
            '.png' = 'image/png'; '.jpg' = 'image/jpeg'; '.svg' = 'image/svg+xml'; '.ico' = 'image/x-icon' }
$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:$Port/")
$listener.Start()
Write-Host "SoundSo-mockup: http://localhost:$Port/"
try
{
    while ($listener.IsListening)
    {
        $ctx = $listener.GetContext()
        try
        {
            $rel = [Uri]::UnescapeDataString($ctx.Request.Url.AbsolutePath.TrimStart('/'))
            if ($rel -eq '') { $rel = 'index.html' }
            $path = [IO.Path]::GetFullPath((Join-Path $root $rel))
            if ($path.StartsWith($root) -and (Test-Path $path -PathType Leaf))
            {
                $bytes = [IO.File]::ReadAllBytes($path)
                $ext = [IO.Path]::GetExtension($path).ToLower()
                $ctx.Response.ContentType = if ($types.ContainsKey($ext)) { $types[$ext] } else { 'application/octet-stream' }
                $ctx.Response.ContentLength64 = $bytes.Length
                $ctx.Response.OutputStream.Write($bytes, 0, $bytes.Length)
            }
            else { $ctx.Response.StatusCode = 404 }
        }
        catch { Write-Host "Fejl: $($_.Exception.Message)" }
        finally { $ctx.Response.Close() }
    }
}
finally { $listener.Stop() }
