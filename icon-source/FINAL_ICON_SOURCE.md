# Final launcher icon source

The release build takes its final icon artwork from the three user-uploaded PNGs at the repository root on `master`:

- `final-icons-background.png`
- `final-icons-foreground.png`
- `final-icons-monochrome.png`

The build validates the square PNGs and alpha channels, then syncs them into `icon-source/`. If source layers exceed 1024 × 1024, the generator uses Lanczos resampling to fit the full square canvas to 1024 × 1024 without cropping. The existing centered 77.5% foreground and monochrome scaling and mild edge sharpening are retained.
