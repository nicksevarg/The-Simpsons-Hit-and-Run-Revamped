// Permanent Android-only interface for the per-level exterior depth-write fix.
#ifndef SHAR_TRANSPARENCY_DEPTH_WRITE_HPP
#define SHAR_TRANSPARENCY_DEPTH_WRITE_HPP

#ifdef RAD_ANDROID
void SetSHARExteriorTransparencyDepthWriteFixActive(bool enabled);
bool IsSHARExteriorTransparencyDepthWriteFixActive();
#endif

#endif // SHAR_TRANSPARENCY_DEPTH_WRITE_HPP
