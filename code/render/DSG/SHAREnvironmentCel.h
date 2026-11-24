#ifndef SHAR_ENVIRONMENT_CEL_H
#define SHAR_ENVIRONMENT_CEL_H

#include <p3d/utility.hpp>
#include <p3d/view.hpp>
#include <p3d/camera.hpp>

bool IsCelShadingEnabled();
bool IsSHARCelShadingLite();
void SetSHAREnvironmentOutlineWidth(float width);

void SetSHAREnvironmentCelCameraPosition(const rmt::Vector& position);
void ClearSHAREnvironmentCelCameraPosition();
bool GetSHAREnvironmentCelCameraPosition(rmt::Vector* position);

// Initial Lite environment policy, using world-space distance only:
//   0-50 units:    1.33-pixel environment outline
//   50-100 units:  1.0-pixel environment outline
//   100-150 units: 0.75-pixel environment outline
//   beyond 150:    no environment outline
//
// Add the object's bounding radius to each distance threshold so large
// structures remain outlined while their nearest surface is within range.
// Foliage-tagged objects are skipped in Lite, but retain Full-mode handling.
static inline bool SHARShouldDrawEnvironmentCelOutline(
    bool foliage,
    const rmt::Vector& worldCentre,
    float boundingRadius,
    float* outlineWidth,
    bool allowFoliageInLite = false)
{
    if (outlineWidth != NULL)
        *outlineWidth = 1.33f;

    if (!IsCelShadingEnabled())
        return false;

    if (!IsSHARCelShadingLite())
        return true;

    if (foliage && !allowFoliageInLite)
        return false;

    rmt::Vector cameraPosition;
    if (!GetSHAREnvironmentCelCameraPosition(&cameraPosition))
    {
        // Preserve outlines if a camera cannot be resolved for this draw.
        return true;
    }

    const float dx = worldCentre.x - cameraPosition.x;
    const float dy = worldCentre.y - cameraPosition.y;
    const float dz = worldCentre.z - cameraPosition.z;
    const float distanceSquared = dx * dx + dy * dy + dz * dz;

    const float safeRadius =
        (boundingRadius > 0.0f && boundingRadius < 10000.0f)
            ? boundingRadius : 0.0f;

    const float nearLimit = safeRadius + 50.0f;
    if (distanceSquared <= nearLimit * nearLimit)
        return true;

    const float mediumLimit = safeRadius + 100.0f;
    if (distanceSquared <= mediumLimit * mediumLimit)
    {
        if (outlineWidth != NULL)
            *outlineWidth = 1.0f;
        return true;
    }

    const float farLimit = safeRadius + 150.0f;
    if (distanceSquared <= farLimit * farLimit)
    {
        if (outlineWidth != NULL)
            *outlineWidth = 0.75f;
        return true;
    }

    return false;
}

#endif
