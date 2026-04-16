#include "Engine/Framework/EnginePch.h"
#include "Engine/Animation/AnimationModule.h"

#include <unordered_set>

#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Animation/AnimInstance.h"
#include "Engine/Animation/AnimationDebug.h"
#include "Engine/Animation/AnimationRuntime.h"
#include "Engine/Animation/SkeletalMeshAsset.h"
#include "Engine/Animation/SkeletonAsset.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Assets/AssetManagerModule.h"
#include "Engine/Components/Components.h"
#include "Engine/Gameplay/Framework/Character.h"
#include "Engine/Gameplay/Framework/CharacterMovementComponent.h"
#include "Engine/Scene/Scene.h"

namespace
{
void ClearAnimationRuntimeData(SkeletalMeshComponent* skComp)
{
    if (!skComp)
        return;

    skComp->LocalPose.Clear();
    skComp->GlobalPose.Clear();
    skComp->FinalPalette.Clear();

    skComp->RuntimeBoneLocalTranslations.Clear();
    skComp->RuntimeBoneGlobalTranslations.Clear();
    skComp->RuntimeBoneLocalScales.Clear();
    skComp->RuntimeBoneGlobalScales.Clear();
    skComp->RuntimeBoneLocalRotations.Clear();
    skComp->RuntimeBoneGlobalRotations.Clear();
}

void UpdateRuntimeBoneTransforms(
    SkeletalMeshComponent* skComp,
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& localPose,
    const TArray<Mat4>& globalPose)
{
    if (!skComp || !skeleton)
        return;

    const int32 boneCount = static_cast<int32>(skeleton->m_InvBind.Num());
    if (boneCount <= 0 || localPose.Num() != boneCount || globalPose.Num() != boneCount)
    {
        skComp->RuntimeBoneLocalTranslations.Clear();
        skComp->RuntimeBoneGlobalTranslations.Clear();
        skComp->RuntimeBoneLocalScales.Clear();
        skComp->RuntimeBoneGlobalScales.Clear();
        skComp->RuntimeBoneLocalRotations.Clear();
        skComp->RuntimeBoneGlobalRotations.Clear();
        return;
    }

    skComp->RuntimeBoneLocalTranslations.Resize(boneCount);
    skComp->RuntimeBoneGlobalTranslations.Resize(boneCount);
    skComp->RuntimeBoneLocalScales.Resize(boneCount);
    skComp->RuntimeBoneGlobalScales.Resize(boneCount);
    skComp->RuntimeBoneLocalRotations.Resize(boneCount);
    skComp->RuntimeBoneGlobalRotations.Resize(boneCount);

    for (int32 i = 0; i < boneCount; ++i)
    {
        Vector3 localT(0.0f);
        Vector3 localS(1.0f);
        Quaternion localR(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(localPose[i], localT, localR, localS);

        Vector3 globalT(0.0f);
        Vector3 globalS(1.0f);
        Quaternion globalR(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(globalPose[i], globalT, globalR, globalS);

        skComp->RuntimeBoneLocalTranslations[i] = localT;
        skComp->RuntimeBoneGlobalTranslations[i] = globalT;
        skComp->RuntimeBoneLocalScales[i] = localS;
        skComp->RuntimeBoneGlobalScales[i] = globalS;
        skComp->RuntimeBoneLocalRotations[i] = localR;
        skComp->RuntimeBoneGlobalRotations[i] = globalR;
    }
}

void ValidateBindPoseMatricesOnce(const SkeletonAsset* skeleton, const TArray<Mat4>& globalBindPose)
{
#if ANIMATION_DEBUG
    if (!skeleton)
        return;

    static std::unordered_set<uint64> validatedSkeletons;
    const uint64 skeletonID = (uint64)skeleton->ID;
    if (validatedSkeletons.find(skeletonID) != validatedSkeletons.end())
        return;

    int32 worstBoneIndex = -1;
    const float worstError =
        AnimationRuntime::ValidateBindPoseIdentity(skeleton, globalBindPose, &worstBoneIndex);

    if (worstError > 1e-3f)
    {
        String boneName = "Unknown";
        if (worstBoneIndex >= 0 && worstBoneIndex < skeleton->m_BoneNames.Num())
            boneName = skeleton->m_BoneNames[worstBoneIndex];

        ANIMATION_DEBUG_LOG("[Skinning][BindPose] WARNING skeleton="
                            << (uint64)skeleton->ID
                            << " worstError=" << worstError
                            << " boneIndex=" << worstBoneIndex
                            << " boneName=" << boneName.c_str());
    }
    else
    {
        ANIMATION_DEBUG_LOG("[Skinning][BindPose] OK skeleton="
                            << (uint64)skeleton->ID
                            << " maxError=" << worstError);
    }

    ANIMATION_DEBUG_FLUSH();
    validatedSkeletons.insert(skeletonID);
#else
    (void)skeleton;
    (void)globalBindPose;
#endif
}

void BlendLocalPoses(
    const TArray<Mat4>& fromPose,
    const TArray<Mat4>& toPose,
    const float alpha,
    TArray<Mat4>& outPose)
{
    if (fromPose.Num() != toPose.Num())
    {
        outPose = toPose;
        return;
    }

    outPose.Resize(toPose.Num());
    const float blendAlpha = FMath::clamp(alpha, 0.0f, 1.0f);
    for (int32 i = 0; i < toPose.Num(); ++i)
    {
        Vector3 fromTranslation(0.0f);
        Vector3 fromScale(1.0f);
        Quaternion fromRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(fromPose[i], fromTranslation, fromRotation, fromScale);

        Vector3 toTranslation(0.0f);
        Vector3 toScale(1.0f);
        Quaternion toRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(toPose[i], toTranslation, toRotation, toScale);

        Quaternion adjustedTargetRotation = toRotation;
        if (FMath::dot(fromRotation, adjustedTargetRotation) < 0.0f)
            adjustedTargetRotation = -adjustedTargetRotation;

        const Vector3 blendedTranslation = FMath::mix(fromTranslation, toTranslation, blendAlpha);
        const Vector3 blendedScale = FMath::mix(fromScale, toScale, blendAlpha);
        const Quaternion blendedRotation =
            FMath::normalize(FMath::slerp(fromRotation, adjustedTargetRotation, blendAlpha));

        outPose[i] = AnimationRuntime::ComposeTRS(blendedTranslation, blendedRotation, blendedScale);
    }
}

void LockRootBoneTranslationToBindPose(
    const SkeletonAsset* skeleton,
    const TArray<Mat4>& localBindPose,
    TArray<Mat4>& inOutLocalPose)
{
    if (!skeleton || localBindPose.Num() != inOutLocalPose.Num())
        return;

    const int32 boneCount = static_cast<int32>(inOutLocalPose.Num());
    auto ToLowerAscii = [](const String& value) -> std::string
    {
        std::string out = value.c_str();
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return out;
    };

    auto IsLocomotionCarrierName = [&](const int32 boneIndex) -> bool
    {
        if (boneIndex < 0 || boneIndex >= skeleton->m_BoneNames.Num())
            return false;

        const std::string name = ToLowerAscii(skeleton->m_BoneNames[boneIndex]);
        return name.find("hips") != std::string::npos ||
               name.find("pelvis") != std::string::npos ||
               name == "root";
    };

    auto lockBoneTranslation = [&](const int32 boneIndex)
    {
        Vector3 bindTranslation(0.0f);
        Vector3 bindScale(1.0f);
        Quaternion bindRotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(localBindPose[boneIndex], bindTranslation, bindRotation, bindScale);

        Vector3 translation(0.0f);
        Vector3 scale(1.0f);
        Quaternion rotation(1.0f, 0.0f, 0.0f, 0.0f);
        AnimationRuntime::DecomposeTRS(inOutLocalPose[boneIndex], translation, rotation, scale);

        inOutLocalPose[boneIndex] = AnimationRuntime::ComposeTRS(bindTranslation, rotation, scale);
    };

    for (int32 i = 0; i < boneCount; ++i)
    {
        const int32 parent = skeleton->m_Parent[i];
        if (parent >= 0 && parent < boneCount)
            continue;

        lockBoneTranslation(i);

        int32 locomotionCarrier = -1;
        for (int32 childIndex = 0; childIndex < boneCount; ++childIndex)
        {
            if (skeleton->m_Parent[childIndex] != i)
                continue;

            if (!IsLocomotionCarrierName(childIndex))
                continue;

            locomotionCarrier = childIndex;
            break;
        }

        if (locomotionCarrier >= 0)
            lockBoneTranslation(locomotionCarrier);
    }
}

AnimationLocomotionState BuildLocomotionStateForComponent(const SkeletalMeshComponent* skComp)
{
    if (!skComp)
        return {};

    const Actor* owner = skComp->GetOwner();
    if (!owner)
        return skComp->LocomotionState;

    const Character* character = dynamic_cast<const Character*>(owner);
    if (!character)
        return skComp->LocomotionState;

    const CharacterMovementComponent* movement = character->GetCharacterMovementComponent();
    if (!movement)
        return skComp->LocomotionState;

    return movement->BuildAnimationLocomotionState();
}
} // namespace

AnimationModule::AnimationModule()
{
    m_TickType = TickType::PostSimulation;
}

AnimationModule::~AnimationModule()
{
}

void AnimationModule::OnEvent(const Event& e)
{
    IModule::OnEvent(e);
}

void AnimationModule::Init()
{
}

void AnimationModule::UpdateAnimations(float dt)
{
    if (!m_Scene || !m_AssetManager)
        return;

    AssetManager& assetManager = *m_AssetManager;
    auto& reg = m_Scene->GetRegistry();
    auto skelView = reg.view<SkeletalMeshComponent*>();

    for (auto e : skelView)
    {
        auto* skComp = skelView.get<SkeletalMeshComponent*>(e);
        if (!skComp)
            continue;

        if (!skComp->bIsVisible || !skComp->IsValid())
            continue;

        SkeletalMeshAsset* skAsset =
            dynamic_cast<SkeletalMeshAsset*>(assetManager.Load(skComp->Mesh.GetHandle()));
        if (!skAsset)
        {
            ClearAnimationRuntimeData(skComp);
            continue;
        }

        SkeletonAsset* skeleton =
            dynamic_cast<SkeletonAsset*>(assetManager.Load(skAsset->m_Skeleton.GetHandle()));
        if (!skeleton)
        {
            ClearAnimationRuntimeData(skComp);
            continue;
        }

        TArray<Mat4> localBindPose;
        TArray<Mat4> globalBindPose;
        if (!AnimationRuntime::BuildBindPoses(skeleton, localBindPose, globalBindPose))
        {
            ClearAnimationRuntimeData(skComp);
            continue;
        }

        ValidateBindPoseMatricesOnce(skeleton, globalBindPose);

        TArray<Mat4> localPose = localBindPose;
        TArray<Mat4> globalPose = globalBindPose;

        const bool bWantsOverrideAnimation =
            skComp->bOverrideAnimationActive &&
            (uint64)skComp->OverrideAnimation.GetHandle() != 0;

        bool bPoseEvaluated = false;
        if (AnimInstance* animInstance = skComp->GetAnimInstance(); animInstance && !bWantsOverrideAnimation)
        {
            animInstance->SetRootMotionEnabled(skComp->bEnableRootMotion);
            skComp->LocomotionState = BuildLocomotionStateForComponent(skComp);
            if (skComp->bPlayAnimation)
                animInstance->Update(dt * skComp->PlaybackSpeed, skComp->LocomotionState);

            AnimationEvaluationContext context{};
            context.AssetManager = &assetManager;
            context.Skeleton = skeleton;
            context.LocalBindPose = &localBindPose;

            bPoseEvaluated = animInstance->Evaluate(context, localPose);
            skComp->PlaybackTime = animInstance->GetDebugPlaybackTime();
        }

        if (!bPoseEvaluated)
        {
            AnimationAsset* overrideAnimation = nullptr;
            if (bWantsOverrideAnimation)
            {
                overrideAnimation = dynamic_cast<AnimationAsset*>(
                    assetManager.Load(skComp->OverrideAnimation.GetHandle()));

                if (overrideAnimation &&
                    (uint64)overrideAnimation->m_SkeletonID != 0 &&
                    (uint64)overrideAnimation->m_SkeletonID != (uint64)skeleton->ID)
                {
                    overrideAnimation = nullptr;
                }

                if (!overrideAnimation)
                    skComp->StopAnimation();
            }

            if (overrideAnimation)
            {
                if (skComp->bPlayAnimation)
                    skComp->OverridePlaybackTime += dt * skComp->OverridePlaybackSpeed;

                skComp->OverridePlaybackTime = AnimationRuntime::NormalizePlaybackTime(
                    skComp->OverridePlaybackTime,
                    overrideAnimation->m_DurationSeconds,
                    skComp->bOverrideAnimationLooping);

                TArray<Mat4> overridePose = localBindPose;
                bPoseEvaluated = AnimationRuntime::EvaluateLocalPose(
                    skeleton,
                    overrideAnimation,
                    skComp->OverridePlaybackTime,
                    localBindPose,
                    overridePose,
                    skComp->bEnableRootMotion);

                if (bPoseEvaluated && skComp->bOverrideBlendActive)
                {
                    if (skComp->bPlayAnimation)
                    {
                        skComp->OverrideBlendElapsed += dt;
                        if (skComp->OverrideBlendElapsed > skComp->OverrideBlendDuration)
                            skComp->OverrideBlendElapsed = skComp->OverrideBlendDuration;
                    }

                    const float blendAlpha = skComp->OverrideBlendDuration > 1.0e-6f
                        ? FMath::clamp(skComp->OverrideBlendElapsed / skComp->OverrideBlendDuration, 0.0f, 1.0f)
                        : 1.0f;

                    BlendLocalPoses(
                        skComp->OverrideBlendSourcePose,
                        overridePose,
                        blendAlpha,
                        localPose);

                    if (blendAlpha >= 1.0f - 1.0e-6f)
                    {
                        skComp->bOverrideBlendActive = false;
                        skComp->OverrideBlendDuration = 0.0f;
                        skComp->OverrideBlendElapsed = 0.0f;
                        skComp->OverrideBlendSourcePose.Clear();
                    }
                }
                else if (bPoseEvaluated)
                {
                    localPose = std::move(overridePose);
                }

                if (bPoseEvaluated && skComp->bOverrideLockRootBoneTranslation)
                    LockRootBoneTranslationToBindPose(skeleton, localBindPose, localPose);

                skComp->PlaybackTime = skComp->OverridePlaybackTime;

                if (!skComp->bOverrideAnimationLooping &&
                    overrideAnimation->m_DurationSeconds > 0.0f &&
                    skComp->OverridePlaybackTime >= overrideAnimation->m_DurationSeconds - 1.0e-4f)
                {
                    const float blendOutDuration = glm::max(0.0f, skComp->OverrideBlendOutDuration);
                    const bool bCanBlendOut = blendOutDuration > 1.0e-6f && !localPose.IsEmpty();

                    skComp->bOverrideAnimationActive = false;
                    skComp->OverrideAnimation = {};
                    skComp->OverridePlaybackTime = 0.0f;
                    skComp->bOverrideBlendActive = false;
                    skComp->OverrideBlendDuration = 0.0f;
                    skComp->OverrideBlendElapsed = 0.0f;
                    skComp->OverrideBlendSourcePose.Clear();
                    skComp->bOverrideLockRootBoneTranslation = false;

                    skComp->bOverrideBlendOutActive = bCanBlendOut;
                    skComp->OverrideBlendOutElapsed = 0.0f;
                    if (bCanBlendOut)
                        skComp->OverrideBlendOutSourcePose = localPose;
                    else
                        skComp->OverrideBlendOutSourcePose.Clear();
                }
            }

            if (!bPoseEvaluated)
            {
                AnimationAsset* animation = nullptr;
                if ((uint64)skComp->Animation.GetHandle() != 0)
                {
                    animation = dynamic_cast<AnimationAsset*>(assetManager.Load(skComp->Animation.GetHandle()));

                    if (animation && (uint64)animation->m_SkeletonID != 0 &&
                        (uint64)animation->m_SkeletonID != (uint64)skeleton->ID)
                    {
                        animation = nullptr;
                    }
                }

                if (animation)
                {
                    if (skComp->bPlayAnimation)
                        skComp->PlaybackTime += dt * skComp->PlaybackSpeed;

                    skComp->PlaybackTime = AnimationRuntime::NormalizePlaybackTime(
                        skComp->PlaybackTime,
                        animation->m_DurationSeconds,
                        skComp->bLoopAnimation);

                    bPoseEvaluated = AnimationRuntime::EvaluateLocalPose(
                        skeleton,
                        animation,
                        skComp->PlaybackTime,
                        localBindPose,
                        localPose,
                        skComp->bEnableRootMotion);

                    if (bPoseEvaluated && !skComp->bEnableRootMotion)
                        LockRootBoneTranslationToBindPose(skeleton, localBindPose, localPose);
                }
                else
                {
                    skComp->PlaybackTime = 0.0f;
                }
            }
        }

        if (!bPoseEvaluated)
            localPose = localBindPose;

        if (skComp->bOverrideBlendOutActive)
        {
            const bool bValidBlendOutSource =
                !skComp->OverrideBlendOutSourcePose.IsEmpty() &&
                skComp->OverrideBlendOutSourcePose.Num() == localPose.Num();
            if (!bValidBlendOutSource)
            {
                skComp->bOverrideBlendOutActive = false;
                skComp->OverrideBlendOutElapsed = 0.0f;
                skComp->OverrideBlendOutSourcePose.Clear();
            }
            else
            {
                if (skComp->bPlayAnimation)
                {
                    skComp->OverrideBlendOutElapsed += dt;
                    if (skComp->OverrideBlendOutElapsed > skComp->OverrideBlendOutDuration)
                        skComp->OverrideBlendOutElapsed = skComp->OverrideBlendOutDuration;
                }

                const float blendAlpha = skComp->OverrideBlendOutDuration > 1.0e-6f
                    ? FMath::clamp(
                        skComp->OverrideBlendOutElapsed / skComp->OverrideBlendOutDuration,
                        0.0f,
                        1.0f)
                    : 1.0f;

                TArray<Mat4> blendedPose;
                BlendLocalPoses(skComp->OverrideBlendOutSourcePose, localPose, blendAlpha, blendedPose);
                localPose = std::move(blendedPose);

                if (blendAlpha >= 1.0f - 1.0e-6f)
                {
                    skComp->bOverrideBlendOutActive = false;
                    skComp->OverrideBlendOutElapsed = 0.0f;
                    skComp->OverrideBlendOutSourcePose.Clear();
                }
            }
        }

        AnimationRuntime::BuildGlobalPose(skeleton, localPose, globalPose);

        TArray<Mat4> skinPalette;
        if (!AnimationRuntime::BuildSkinPalette(skeleton, globalPose, skinPalette))
        {
            ClearAnimationRuntimeData(skComp);
            continue;
        }

        skComp->LocalPose = std::move(localPose);
        skComp->GlobalPose = std::move(globalPose);
        skComp->FinalPalette = std::move(skinPalette);

        UpdateRuntimeBoneTransforms(skComp, skeleton, skComp->LocalPose, skComp->GlobalPose);
    }
}

void AnimationModule::Tick(float deltaTime)
{
    UpdateAnimations(deltaTime);
}

void AnimationModule::Shutdown()
{
}



