# Clear-IconCache.ps1
# Clears Windows icon and thumbnail caches so the taskbar shows the updated app icon.
# Requires Administrator privileges.

$ErrorActionPreference = "Stop"

Write-Host "Stopping Explorer to release icon cache locks..."
Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

$paths = @(
    "$env:LOCALAPPDATA\IconCache.db",
    "$env:LOCALAPPDATA\Microsoft\Windows\Explorer\iconcache_*.db",
    "$env:LOCALAPPDATA\Microsoft\Windows\Explorer\thumbcache_*.db"
)

foreach ($p in $paths) {
    Get-Item $p -ErrorAction SilentlyContinue | ForEach-Object {
        try {
            Remove-Item $_.FullName -Force
            Write-Host "Deleted: $($_.Name)"
        } catch {
            Write-Warning "Could not delete $($_.Name): $_"
        }
    }
}

Write-Host "Restarting Explorer..."
Start-Process explorer

Write-Host "Icon cache cleared. Launch the app now and the taskbar should show the correct icon."
