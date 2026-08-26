# Quit any running Photoshop instance (COM-launched or attached). No args.
$ErrorActionPreference = 'SilentlyContinue'
$app = New-Object -ComObject Photoshop.Application
try {
    $app.DoJavaScript('if (app.documents.length > 0) { for (var i = app.documents.length - 1; i >= 0; i--) { try { app.documents[i].close(SaveOptions.DONOTSAVECHANGES); } catch (e) {} } }')
} catch {}
try { $app.Quit() } catch {}
Write-Output 'Photoshop quit request issued'
