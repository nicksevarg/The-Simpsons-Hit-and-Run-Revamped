Original source images for the adaptive Android launcher icon.

- The final artwork is kept in the repository root on `master` as `final-icons-background.png`, `final-icons-foreground.png`, and `final-icons-monochrome.png`.
- The release workflow copies those three exact PNG assets into this directory and validates that each is square; foreground and monochrome must retain transparency.
- Square high-resolution source assets are resampled to the 1024 × 1024 Android icon canvas with Lanczos filtering. This preserves the full canvas with no crop.
- Foreground and monochrome artwork remain centered at the established 77.5% scale; the background is never reduced to that foreground scale.
- The generator retains its existing mild edge sharpening, adaptive-layer output, Play Store artwork, and square/circular previews.
