#pragma once

#include <memory>
#include <utility>

#include "Engine/Animation/AnimInstance.h"
#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Animation/SkeletalMeshAsset.h"
#include "Engine/Assets/AssetPtr.h"
#include "Engine/Components/SceneComponent.h"
#include "Engine/Rendering/Mesh.h"

struct SkeletalMeshComponent : SceneComponent
{
    AssetPtr<SkeletalMeshAsset> Mesh{};
    AssetPtr<AnimationAsset> Animation{};
    AssetPtr<AnimationAsset> IdleAnimation{};
    AssetPtr<AnimationAsset> MoveAnimation{};
    AssetPtr<AnimationAsset> JumpAnimation{};
    AssetPtr<AnimationAsset> DoubleJumpAnimation{};
    AssetPtr<AnimationAsset> FallingAnimation{};
    AssetPtr<AnimationAsset> LandAnimation{};
    MaterialHandle Material{};

    Bool bIsVisible = true;
    Bool bCastShadows = true;
    Bool bDrawSkeleton = false;
    Bool bPlayAnimation = true;
    Bool bLoopAnimation = true;
    Bool bEnableRootMotion = false;
    Float PlaybackSpeed = 1.0f;
    Float PlaybackTime = 0.0f;
    AnimationLocomotionState LocomotionState{};

    TArray<Mat4> LocalPose;
    TArray<Mat4> GlobalPose;
    TArray<Mat4> FinalPalette;

    TArray<Vector3> RuntimeBoneLocalTranslations;
    TArray<Vector3> RuntimeBoneGlobalTranslations;
    TArray<Vector3> RuntimeBoneLocalScales;
    TArray<Vector3> RuntimeBoneGlobalScales;
    TArray<Quaternion> RuntimeBoneLocalRotations;
    TArray<Quaternion> RuntimeBoneGlobalRotations;
    std::unique_ptr<AnimInstance> AnimScriptInstance{};
    AssetPtr<AnimationAsset> OverrideAnimation{};
    Bool bOverrideAnimationActive = false;
    Bool bOverrideAnimationLooping = false;
    Float OverridePlaybackSpeed = 1.0f;
    Float OverridePlaybackTime = 0.0f;
    Bool bOverrideBlendActive = false;
    Float OverrideBlendDuration = 0.0f;
    Float OverrideBlendElapsed = 0.0f;
    TArray<Mat4> OverrideBlendSourcePose;
    Bool bOverrideLockRootBoneTranslation = false;
    Bool bOverrideBlendOutActive = false;
    Float OverrideBlendOutDuration = 0.08f;
    Float OverrideBlendOutElapsed = 0.0f;
    TArray<Mat4> OverrideBlendOutSourcePose;

    SkeletalMeshComponent() = default;

    SkeletalMeshComponent(AssetHandle meshAsset,
                          MaterialHandle mat = MaterialHandle(),
                          Bool visible = true,
                          Bool castShadows = true)
        : Mesh(meshAsset)
        , Material(mat)
        , bIsVisible(visible)
        , bCastShadows(castShadows)
    {}

    Bool IsValid() const
    {
        return (uint64)Mesh.GetHandle() != 0;
    }

    template<typename TAnimInstance, typename... Args>
    TAnimInstance& CreateAnimInstance(Args&&... args)
    {
        auto instance = std::make_unique<TAnimInstance>(std::forward<Args>(args)...);
        TAnimInstance* rawInstance = instance.get();
        SetAnimInstance(std::move(instance));
        return *rawInstance;
    }

    void SetAnimInstance(std::unique_ptr<AnimInstance> animInstance)
    {
        AnimScriptInstance = std::move(animInstance);
        if (AnimScriptInstance)
            AnimScriptInstance->Initialize(this);
    }

    AnimInstance* GetAnimInstance() const { return AnimScriptInstance.get(); }
    
    void PlayAnimation(
        const AssetPtr<AnimationAsset>& animation,
        Bool bLooping = false,
        Float playbackSpeed = 1.0f,
        Float blendDuration = 0.0f)
    {
        OverrideAnimation = animation;
        bOverrideAnimationActive = (uint64)OverrideAnimation.GetHandle() != 0;
        bOverrideAnimationLooping = bLooping;
        OverridePlaybackSpeed = glm::max(0.0f, playbackSpeed);
        OverridePlaybackTime = 0.0f;
        OverrideBlendDuration = glm::max(0.0f, blendDuration);
        OverrideBlendElapsed = 0.0f;
        bOverrideBlendActive = bOverrideAnimationActive && OverrideBlendDuration > 1.0e-6f;
        OverrideBlendSourcePose = LocalPose;
        bOverrideBlendOutActive = false;
        OverrideBlendOutElapsed = 0.0f;
        OverrideBlendOutSourcePose.Clear();
    }

    void PlayAnimation(
        const AssetHandle animationHandle,
        const Bool bLooping = false,
        const Float playbackSpeed = 1.0f,
        const Float blendDuration = 0.0f)
    {
        PlayAnimation(AssetPtr<AnimationAsset>(animationHandle), bLooping, playbackSpeed, blendDuration);
    }

    void StopAnimation()
    {
        const float blendOutDuration = glm::max(0.0f, OverrideBlendOutDuration);
        const bool bCanBlendOut = blendOutDuration > 1.0e-6f && !LocalPose.IsEmpty();

        bOverrideAnimationActive = false;
        OverrideAnimation = {};
        OverridePlaybackTime = 0.0f;
        bOverrideBlendActive = false;
        OverrideBlendDuration = 0.0f;
        OverrideBlendElapsed = 0.0f;
        OverrideBlendSourcePose.Clear();
        bOverrideLockRootBoneTranslation = false;

        bOverrideBlendOutActive = bCanBlendOut;
        OverrideBlendOutElapsed = 0.0f;
        if (bCanBlendOut)
            OverrideBlendOutSourcePose = LocalPose;
        else
            OverrideBlendOutSourcePose.Clear();
    }

    explicit operator Bool() const { return IsValid(); }

    REFLECTABLE_CLASS(SkeletalMeshComponent, SceneComponent)
};

REFLECT_CLASS(SkeletalMeshComponent, SceneComponent)
{
    REFLECT_PROPERTY(SkeletalMeshComponent, Mesh,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, Animation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, IdleAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, MoveAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, JumpAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, DoubleJumpAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, FallingAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, LandAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, Material,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bIsVisible,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bCastShadows,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bDrawSkeleton,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bPlayAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bLoopAnimation,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, bEnableRootMotion,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, PlaybackSpeed,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);

    REFLECT_PROPERTY(SkeletalMeshComponent, PlaybackTime,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable | EPropertyFlags::Transient);

    REFLECT_PROPERTY(SkeletalMeshComponent, OverrideBlendOutDuration,
        EPropertyFlags::VisibleInEditor | EPropertyFlags::Editable);
}
END_REFLECT_CLASS(SkeletalMeshComponent)
REFLECT_ECS_COMPONENT(SkeletalMeshComponent)
