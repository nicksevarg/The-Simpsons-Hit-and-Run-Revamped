#pragma once

// Per-draw identity for narrowly scoped GLES outline paths. This state is
// independent of diagnostic logging.
inline unsigned long long& SHARCurrentDrawableUIDStorage()
{
    static unsigned long long uid = 0;
    return uid;
}
inline void SetSHARCurrentDrawableUID(unsigned long long uid)
{
    SHARCurrentDrawableUIDStorage() = uid;
}
inline void ClearSHARCurrentDrawableUID()
{
    SHARCurrentDrawableUIDStorage() = 0;
}
inline unsigned long long GetSHARCurrentDrawableUID()
{
    return SHARCurrentDrawableUIDStorage();
}
