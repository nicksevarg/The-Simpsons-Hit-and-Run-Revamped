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

inline bool IsSHARTomaccoPlantDrawableUID(unsigned long long uid)
{
    switch (uid)
    {
        case 0x59990BF1ADFF8064ULL:
        case 0xE4F59E255A9E247EULL:
        case 0xD381CAC9A3AF9521ULL:
        case 0x356F2748A9996933ULL:
            return true;
        default:
            return false;
    }
}
