/*===========================================================================
    poseanimation.cpp
    05-Jan-00 Created by Neall

    Copyright (c)2000 Radical Entertainment, Inc.
    All rights reserved.
===========================================================================*/

#include <p3d/anim/poseanimation.hpp>
#include <p3d/anim/channel.hpp>
#include <p3d/anim/pose.hpp>
#include <p3d/anim/skeleton.hpp>
#include <p3d/utility.hpp>
#include <radmath/radmath.hpp>

#include <string.h>

using namespace RadicalMathLibrary;

namespace
{

// This special-case is limited to the gas04 joint in PTRN_animsigns.
static bool IsL1Gas04SignJoint(const tAnimation* animation, const tPose::Joint* joint)
{
    return animation != NULL &&
           joint != NULL &&
           animation->GetUID() == tEntity::MakeUID("PTRN_animsigns") &&
           joint->uid == tEntity::MakeUID("gas04");
}

// Permanently apply gas04's tested relative spin in the reverse direction,
// while preserving the authored heading. The caller separately applies gas04's
// translation channel so its motion counteracts the offset joint pivot.
static void ApplyGas04PermanentRotation(const tRotationChannel* rotation,
                                        float frame,
                                        const Matrix& restPose,
                                        Matrix* objectMatrix)
{
    if (rotation == NULL || objectMatrix == NULL)
    {
        return;
    }

    Quaternion qReference, qCurrent, qInverseReference, qDelta, qRest, qOutput;
    rotation->GetQuaternion(0.0f, &qReference);
    rotation->GetQuaternion(frame, &qCurrent);
    qInverseReference = qReference;
    qInverseReference.Inverse();
    qDelta.Mult(qCurrent, qInverseReference);
    qDelta.Normalize();
    qDelta.Inverse();

    qRest.BuildFromMatrix(restPose);
    qRest.Normalize();
    qOutput.Mult(qDelta, qRest);
    qOutput.Normalize();
    qOutput.ConvertToMatrix(objectMatrix);
}
} // namespace

//*********************************************************
// Class tPoseAnimationController
//*********************************************************
tPoseAnimationController::tPoseAnimationController() : 
    tBlendFrameController(),
    pose(NULL)
{
}

//---------------------------------------------------------
tPoseAnimationController::tPoseAnimationController(tPoseAnimationController* c) :
    tBlendFrameController(c)
{
    pose = new tPose(c->GetPose()->GetSkeleton());
    pose->AddRef();
}

//---------------------------------------------------------
tPoseAnimationController::~tPoseAnimationController()
{
    tRefCounted::Release(pose);
}

//---------------------------------------------------------
tFrameController* tPoseAnimationController::Clone(void)
{
    return new tPoseAnimationController(this);
}

//---------------------------------------------------------
void tPoseAnimationController::SetPose(tPose* p)
{
    P3DASSERT(p);
    tRefCounted::Assign(pose, p);
}

//---------------------------------------------------------
bool tPoseAnimationController::ValidateAnimation(tAnimation* anim)
{
    return (anim->GetAnimationType()==Pure3DAnimationTypes::POSE_TRANSFORM_PTRN);
}

//---------------------------------------------------------
void tPoseAnimationController::UpdateNoBlending()
{
    tAnimation* anim = playInfo[0].GetAnimation();

    float frame = playInfo[0].GetCurrentFrame();

    int nJoint = pose->GetNumJoint();
    for(int i=0;i < nJoint; i++)
    {
        tPose::Joint* joint = pose->GetJoint(i);
        P3DASSERT(joint);

        const tAnimationGroup* animGroup = anim->GetGroupByUID(joint->uid);

        const tVectorChannel* translation = NULL;
        const tRotationChannel* rotation = NULL;

        if(animGroup)
        {
            translation = animGroup->GetVectorChannel(Pure3DAnimationChannels::Pose::TRANSLATION_TRAN);
            rotation = animGroup->GetRotationChannel(Pure3DAnimationChannels::Pose::ROTATION_ROT);
        }

        tSkeleton::Joint* skeletonJoint = pose->GetSkeleton()->GetJoint(i);

        if (IsL1Gas04SignJoint(anim, joint))
        {
            // Permanent fix: keep gas04's own animated translation, but reverse
            // its relative spin to cancel the offset pivot motion. This is scoped
            // only to gas04 in PTRN_animsigns; gas03 remains on its normal path.
            joint->objectMatrix = skeletonJoint->restPose;
            ApplyGas04PermanentRotation(rotation, frame, skeletonJoint->restPose,
                                        &joint->objectMatrix);
            if (translation != NULL)
            {
                translation->GetValue(frame, &joint->objectMatrix.Row(3));
            }
            continue;
        }

        if(skeletonJoint->preserveBoneLengths)
        {
            // Stuff the bonelengths into the pose
            joint->objectMatrix.FillTranslate( skeletonJoint->restPose.Row(3) );
        }
        else if(translation)
        {
            // Animate the translation channel
            translation->GetValue(frame, &joint->objectMatrix.Row(3));
        }
        else
        {
            /************************  Start HC mod **************************/

            // Stuff the bone lengths into the pose.
            joint->objectMatrix.FillTranslate( skeletonJoint->restPose.Row(3) );


            // This new logic allows us to have translation animations on secondary bones.
            // Example run-through for character-specific animation:
            //                1.  core skeleton bones will be driven by the animation if applicable, except for shoulder joints (which have "preserveBoneLengths")
            //                2.  secondary bones will be driven by the animation if applicable.  If there is a translation animation, great!  use it!  If not, just use the skeleton values.
            // Example run-through for core skeleton animation (e.g. grapple receive):
            //                1.  core skeleton bones will be driven by the animation if applicable, except for shoulder joints (which have "preserveBoneLengths")
            //                2.  secondary bones don't have "preserveBoneLengths" and don't have animations channels, ==> use the skeleton's bone lengths.  Easy!


            // Requirements on art:    -  place "preserveBoneLengths" on bones in core skeleton that should never be overridden
            //                            by translation animations.  Example:  ShoulderLeft and ShoulderRight
            //                         -  secondary bones do NOT have "preserveBoneLengths".

            /************************   End  HC mod **************************/
        }
        
        if(rotation)
        {        
            // Animate the rotation channel
            rotation->GetMatrix(frame, &joint->objectMatrix);
        }
        else
        {
            joint->objectMatrix = skeletonJoint->restPose;
        }
    }
    pose->SetPoseReady(false);
}

//---------------------------------------------------------
void tPoseAnimationController::UpdateWithBlending()
{
    int i;
    int nJoint = pose->GetNumJoint();

    // translation
    for(i=0; i < nJoint; i++)
    {
        tPose::Joint* joint = pose->GetJoint(i);
        P3DASSERT(joint);

        // Simpsons Hack
        // Ask the skeleton if it needs to preserve it's bonelength
        // to allow for shared animations across different Hierarchies.
        tSkeleton::Joint* skeletonJoint = pose->GetSkeleton()->GetJoint(i);

        // For gas04 in PTRN_animsigns, always honour its translation channel;
        // the track is required to cancel the offset pivot while the sign spins.
        bool isL1Gas04 = false;
        for (int activeIndex = 0; activeIndex < nAnim; ++activeIndex)
        {
            if (IsL1Gas04SignJoint(playInfo[activeIndex].GetAnimation(), joint))
            {
                isL1Gas04 = true;
                break;
            }
        }

        if(skeletonJoint->preserveBoneLengths && !isL1Gas04)
        {
            // Stuff the bonelengths into the pose
            joint->objectMatrix.FillTranslate( skeletonJoint->restPose.Row(3) );
        }
        else
        {        
            Vector vBlend(0.0f, 0.0f, 0.0f);  // accumulation buffer
            
            // prime the accumulation buffer by evaluating the first animation in the buffer
            tAnimation *anim = playInfo[0].GetAnimation();
            const tAnimationGroup* animGroup = anim->GetGroupByUID(joint->uid);
            const tVectorChannel* translation = NULL;

            if(animGroup)
            {
                translation = animGroup->GetVectorChannel(Pure3DAnimationChannels::Pose::TRANSLATION_TRAN);
            }

            if(translation)
            {
                translation->GetValue(playInfo[0].GetCurrentFrame(), &vBlend);
            }
            else
            {
                /************************  Start HC mod **************************/
                // Mod by KMC.  See UpdateNoBlending() for comments on what I'm doing and why.

                // Stuff the bone length into the pose
                vBlend = skeletonJoint->restPose.Row( 3 );
                /************************   End  HC mod **************************/
            }

            // blend in remaining animations using the blend weight
            for(int j=1; j < nAnim; j++)
            {
                anim = playInfo[j].GetAnimation();
                animGroup = anim->GetGroupByUID(joint->uid);
                translation = NULL;
                if(animGroup)
                {
                    translation = animGroup->GetVectorChannel(Pure3DAnimationChannels::Pose::TRANSLATION_TRAN);
                }

                if(translation)
                {
                    Vector v;
                    vBlend.Scale(1.0f - playInfo[j].GetWeight());
                    translation->GetValue(playInfo[j].GetCurrentFrame(), &v);
                    vBlend.ScaleAdd(playInfo[j].GetWeight(), v);
                }
            }
             
            // stuff the resulting blended translation vector into the pose
            joint->objectMatrix.Row(3) = vBlend;
        }
    }

    // rotation
    for(i=0; i < nJoint; i++)
    {
        tPose::Joint* joint = pose->GetJoint(i);
        P3DASSERT(joint);

        const tAnimation* gas04Animation = NULL;
        int gas04AnimationIndex = -1;
        for (int activeIndex = 0; activeIndex < nAnim; ++activeIndex)
        {
            if (IsL1Gas04SignJoint(playInfo[activeIndex].GetAnimation(), joint))
            {
                gas04Animation = playInfo[activeIndex].GetAnimation();
                gas04AnimationIndex = activeIndex;
                break;
            }
        }
        if (gas04Animation != NULL)
        {
            tSkeleton::Joint* gas04SkeletonJoint = pose->GetSkeleton()->GetJoint(i);
            const tAnimationGroup* gas04Group = gas04Animation->GetGroupByUID(joint->uid);
            const tRotationChannel* gas04Rotation = gas04Group
                ? gas04Group->GetRotationChannel(Pure3DAnimationChannels::Pose::ROTATION_ROT)
                : NULL;

            // The translation pass above has already applied gas04's channel.
            // Replace only its rotation matrix; its animated translation remains.
            ApplyGas04PermanentRotation(gas04Rotation,
                                        playInfo[gas04AnimationIndex].GetCurrentFrame(),
                                        gas04SkeletonJoint->restPose,
                                        &joint->objectMatrix);
            continue;
        }

        Quaternion qBlend(0.0f, 0.0f, 0.0f, 1.0f);

        // Prime the accumulation buffer by evaluating the first animation in the buffer.  
        tAnimation *anim = playInfo[0].GetAnimation();      
        const tAnimationGroup* animGroup = anim->GetGroupByUID(joint->uid);
        const tRotationChannel* rotation = NULL;

        if(animGroup)
        {
            rotation = animGroup->GetRotationChannel(Pure3DAnimationChannels::Pose::ROTATION_ROT);
        }

        if(rotation)
        {
            rotation->GetQuaternion(playInfo[0].GetCurrentFrame(), &qBlend);
        }
        else
        {
            // Make sure to blend to the rest pose if there isn't any animation
            tSkeleton::Joint* skeletonJoint = pose->GetSkeleton()->GetJoint(i);
            qBlend.BuildFromMatrix(skeletonJoint->restPose);          
        }

        // blend in remaining animations using the blend weight
        for(int j=1; j < nAnim; j++)
        {
            anim = playInfo[j].GetAnimation();
            animGroup = anim->GetGroupByUID(joint->uid);
            rotation = NULL;
            if(animGroup)
            {
                rotation = animGroup->GetRotationChannel(Pure3DAnimationChannels::Pose::ROTATION_ROT);
            }

            if(rotation)
            {
                Quaternion q;
                rotation->GetQuaternion(playInfo[j].GetCurrentFrame(), &q);
                qBlend.Slerp(q, playInfo[j].GetWeight());
            }
        }
        // stuff the blended rotation quaternion into the pose
        qBlend.ConvertToMatrix(&(joint->objectMatrix));
    }

    pose->SetPoseReady(false);
}
