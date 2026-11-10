"""Encode the native renderer's generated test frames; never opens a camera."""
import argparse
from pathlib import Path
import cv2
from PIL import Image, ImageDraw

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("folder", type=Path, help="Folder from Camber.ReactionTests --preview")
args = parser.parse_args()
for name in ("lasers", "rain"):
    files = sorted(args.folder.glob(f"{name}-[0-9][0-9][0-9].png"))
    if len(files) != 120:
        raise SystemExit(f"Expected 120 generated frames for {name}, got {len(files)}")
    first = cv2.imread(str(files[0]))
    h, w = first.shape[:2]
    target = args.folder.parent / f"{name}-preview.mp4"
    writer = cv2.VideoWriter(str(target), cv2.VideoWriter_fourcc(*"mp4v"), 30, (w, h))
    if not writer.isOpened():
        raise SystemExit("The local MP4 encoder is unavailable")
    try:
        for file in files:
            frame = cv2.imread(str(file))
            if frame is None or frame.shape[:2] != (h, w):
                raise SystemExit(f"Invalid generated frame: {file.name}")
            writer.write(frame)
    finally:
        writer.release()
    reader = cv2.VideoCapture(str(target))
    decoded = 0
    while reader.read()[0]:
        decoded += 1
    reader.release()
    if decoded != 120:
        raise SystemExit(f"Encoded preview lost frames: {decoded}/120")
    # A browser-friendly animation plus a contact sheet for inspection.
    frames = []
    for file in files:
        with Image.open(file) as source:
            frames.append(source.resize((640, 360), Image.Resampling.LANCZOS).convert("RGB"))
    frames[0].save(args.folder.parent / f"{name}-preview.gif", save_all=True,
        append_images=frames[1:], duration=[30, 30, 40] * 40, loop=0, optimize=False)
    sheet = Image.new("RGB", (960, 580), "#101419")
    draw = ImageDraw.Draw(sheet)
    for index, number in enumerate((10, 35, 66, 105)):
        x, y = index % 2 * 480, index // 2 * 290
        with Image.open(files[number]) as source:
            sheet.paste(source.resize((480, 270), Image.Resampling.LANCZOS), (x, y + 20))
        draw.text((x + 8, y + 4), f"{name.title()} / {number / 30:.2f}s / generated scene", fill="#c3ccd7")
    sheet.save(args.folder.parent / f"{name}-contact.png")
    print(f"PASS {name}: 120 decoded MP4 frames, four-second GIF, contact sheet")
