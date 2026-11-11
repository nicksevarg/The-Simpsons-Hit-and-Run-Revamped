//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        .h
//
// Description: Blahblahblah
//
// History:     16/04/2002 + Created -- NAME
//
//=============================================================================

#ifndef NISPLAYER_H
#define NISPLAYER_H

//========================================
// Nested Includes
//========================================

#include <presentation/simpleanimationplayer.h>

#include <p3d/scenegraph/scenegraph.hpp>

//========================================
// Forward References
//========================================

//=============================================================================
//
// Synopsis:    Blahblahblah
//
//=============================================================================

class NISPlayer : public SimpleAnimationPlayer
{
    public:
        NISPlayer();
        virtual ~NISPlayer();

        virtual void ClearData();

        bool GetBoundingBox( rmt::Box3D* box );
        void SetSHARCatapultOutlineGag(bool enabled) { mSHARCatapultOutlineGag = enabled; }
        tDrawable* GetDrawable() { return mpSceneGraph; }

    protected:
        virtual void DoLoaded();
        virtual void DoRender();

    private:

        //Prevent wasteful constructor creation.
        NISPlayer( const NISPlayer& nisPlayer );
        NISPlayer& operator=( const NISPlayer& nisPlayer );

        tDrawable* mpSceneGraph;
        bool mSHARCatapultOutlineGag;
};


#endif //NISPLAYER_H

