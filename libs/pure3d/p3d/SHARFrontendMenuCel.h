#ifndef P3D_SHAR_FRONTEND_MENU_CEL_H
#define P3D_SHAR_FRONTEND_MENU_CEL_H

class tDrawable;

#ifdef RAD_ANDROID
// UID-based selection is required because Android Release builds retain Pure3D
// entity UIDs but do not retain the original text names.
bool IsSHARFrontendMenuOutlineDrawable(tDrawable* drawable);
bool IsSHARFrontendMenuFoliageDrawable(tDrawable* drawable);
#endif

#endif // P3D_SHAR_FRONTEND_MENU_CEL_H
