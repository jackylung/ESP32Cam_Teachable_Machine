import os
import sys
from pathlib import Path
from PIL import Image


def rotate_images(folder_path, angle=90):
    root = Path(folder_path)
    if not root.is_dir():
        print(f"Error: '{folder_path}' is not a valid directory.")
        sys.exit(1)

    extensions = ('.jpg', '.jpeg', '.png', '.bmp', '.tiff', '.webp')
    rotated_count = 0

    for filepath in root.rglob('*'):
        if filepath.suffix.lower() in extensions:
            try:
                with Image.open(filepath) as img:
                    rotated = img.rotate(angle, expand=True)
                    rotated.save(filepath)
                    print(f"Rotated: {filepath.relative_to(root)}")
                    rotated_count += 1
            except Exception as e:
                print(f"Failed to rotate {filepath.name}: {e}")

    print(f"\nDone. {rotated_count} image(s) rotated by {angle}°.")


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python rotate_images.py <folder_path> [angle]")
        print("  angle: rotation angle in degrees (default: 90)")
        sys.exit(1)

    folder = sys.argv[1]
    angle = int(sys.argv[2]) if len(sys.argv) > 2 else 90
    rotate_images(folder, angle)
