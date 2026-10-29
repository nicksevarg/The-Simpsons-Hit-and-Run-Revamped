# Adaptive Android Icon Layers

- `background.png`: full-canvas background; do not scale it.
- `foreground.png`: full-canvas source with transparency. The generator centers it at 77.5%.
- `monochrome.png`: themed-icon silhouette with transparency. The generator centers it at the same 77.5% scale.
- Keep source layers at 1024 × 1024 pixels. The release build validates dimensions and alpha before replacing source images from the shared Dropbox `Icons` folder.
- `tools/generate_adaptive_icon.py` remains responsible for the existing mild edge sharpening, adaptive-layer output, Play Store artwork, and square/circular previews.
