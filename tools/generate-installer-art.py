"""Build native wizard artwork from Camber's existing approved offset-C mark."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
output = root / "installer/assets"
output.mkdir(parents=True, exist_ok=True)
font_root = Path("C:/Windows/Fonts")
regular = lambda size: ImageFont.truetype(str(font_root / "segoeui.ttf"), size)
bold = lambda size: ImageFont.truetype(str(font_root / "segoeuib.ttf"), size)
mark = Image.open(root / "design/brand/offset-final/exports/camber-mono-80b8ff-512.png").convert("RGBA")

large = Image.new("RGB", (328, 628))
pixels = large.load()
for y in range(628):
    for x in range(328):
        sheen = max(0, 1 - ((x - 40) ** 2 / 150000 + (y - 60) ** 2 / 160000))
        pixels[x, y] = (int(12 + 7 * sheen), int(16 + 9 * sheen), int(23 + 13 * sheen))
draw = ImageDraw.Draw(large)
draw.rounded_rectangle((28, 42, 300, 315), radius=40, fill="#111923", outline="#2c3a4d", width=2)
large.paste(mark.resize((220, 220), Image.Resampling.LANCZOS), (54, 68), mark.resize((220, 220), Image.Resampling.LANCZOS))
draw.text((30, 357), "Camber", font=bold(38), fill="#f0f4fa")
draw.text((30, 419), "Your phone.", font=regular(26), fill="#c5d0dd")
draw.text((30, 455), "A better camera.", font=regular(26), fill="#c5d0dd")
draw.rounded_rectangle((30, 535, 296, 586), radius=16, fill="#192b40", outline="#314c6a")
draw.text((48, 548), "LOCAL. PRIVATE. YOURS.", font=bold(16), fill="#91c1ff")
large.save(output / "wizard-large.bmp")

small = Image.new("RGB", (116, 116), "#111923")
icon = mark.resize((88, 88), Image.Resampling.LANCZOS)
small.paste(icon, (14, 14), icon)
small.save(output / "wizard-small.bmp")
print("Generated two native wizard assets from the existing Camber mark.")
