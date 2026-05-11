# Generate multi-size ICO from PNG using .NET System.Drawing
$pngPath = Resolve-Path "$PSScriptRoot\..\build\appicon.png"
$icoPath = "$PSScriptRoot\..\build\windows\icon.ico"

# Load source PNG
$src = [System.Drawing.Image]::FromFile($pngPath)

# Standard Windows icon sizes (16-256)
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$bitmaps = @()

foreach ($size in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.DrawImage($src, 0, 0, $size, $size)
    $g.Dispose()
    $bitmaps += $bmp
}

# Multi-frame ICO encoding
# ICO header: 6 bytes (reserved + type + count)
# ICONDIRENTRY: 16 bytes per frame
# Followed by PNG data for each frame

$ms = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($ms)

# Write ICO header
$writer.Write([UInt16]0)   # Reserved
$writer.Write([UInt16]1)   # Type: 1 = ICO
$writer.Write([UInt16]$bitmaps.Count)

# Calculate header size to know where image data starts
$headerSize = 6 + ($bitmaps.Count * 16)
$offset = $headerSize

# First pass: write directory entries
for ($i = 0; $i -lt $bitmaps.Count; $i++) {
    $bmp = $bitmaps[$i]
    $size = $bmp.Width

    # Save each bitmap as PNG to get byte size
    $pngMs = New-Object System.IO.MemoryStream
    $bmp.Save($pngMs, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngBytes = $pngMs.ToArray()
    $pngMs.Dispose()

    $writer.Write([Byte](if ($size -ge 256) { 0 } else { $size }))  # Width
    $writer.Write([Byte](if ($size -ge 256) { 0 } else { $size }))  # Height
    $writer.Write([Byte]0)   # Color palette count (0 for PNG)
    $writer.Write([Byte]0)   # Reserved
    $writer.Write([UInt16]1) # Color planes
    $writer.Write([UInt16]32) # Bits per pixel
    $writer.Write([UInt32]$pngBytes.Length)  # Size in bytes
    $writer.Write([UInt32]$offset)           # Offset

    $offset += $pngBytes.Length
}

# Second pass: write actual PNG data
for ($i = 0; $i -lt $bitmaps.Count; $i++) {
    $bmp = $bitmaps[$i]
    $pngMs = New-Object System.IO.MemoryStream
    $bmp.Save($pngMs, [System.Drawing.Imaging.ImageFormat]::Png)
    $writer.Write($pngMs.ToArray())
    $pngMs.Dispose()
    $bmp.Dispose()
}

$writer.Flush()
$bytes = $ms.ToArray()
$ms.Dispose()
$writer.Dispose()

[System.IO.File]::WriteAllBytes($icoPath, $bytes)
$src.Dispose()

Write-Host "Generated multi-size ICO: $icoPath ($($sizes.Count) frames: $($sizes -join ', ')x$($sizes -join ', '))"
