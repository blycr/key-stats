from PIL import Image
import sys

# Load source image
img = Image.open(r'C:\Users\blycr\.gemini\antigravity\scratch\key-stats\build\appicon.png')

# Convert to RGBA if needed
if img.mode != 'RGBA':
    img = img.convert('RGBA')

# Create multi-size ICO with standard Windows taskbar sizes
sizes = [(16,16), (24,24), (32,32), (48,48), (64,64), (128,128), (256,256)]
images = []
for size in sizes:
    resized = img.resize(size, Image.LANCZOS)
    images.append(resized)

# Save as multi-size ICO
output = r'C:\Users\blycr\.gemini\antigravity\scratch\key-stats\build\windows\icon.ico'

# Pillow ICO save: first image + append_images for additional sizes
images[0].save(
    output,
    format='ICO',
    sizes=sizes,
    append_images=images[1:]
)

# Verify
ico = Image.open(output)
print(f'Generated ICO with {ico.n_frames} frames')
for i in range(ico.n_frames):
    ico.seek(i)
    print(f'  Frame {i}: {ico.size}')
