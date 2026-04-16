#include "Engine/Framework/EnginePch.h"
#include "Engine/Animation/AnimInstance.h"

#include "Engine/Animation/AnimationRuntime.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Components/SkeletalMeshComponent.h"

namespace
{
const char* ToDebugString(const LocomotionAnimState state)
{
    switch (state)
    {
    case LocomotionAnimState::Idle: return "Idle";
    case LocomotionAnimState::Move: return "Move";
    case LocomotionAnimState::Jump: return "Jump";
    case LocomotionAnimState::Falling: return "Falling";
    case LocomotionAnimState::Land: return "Land";
    default: return "Unknown";
    }
}

float ComputeTransitionBlendAlpha(const float elapsedTime, const float duration)
{
    if (duration <= 1e-6f)
        return 1.0f;

    return FMath::clamp(elapsedTime / duration, 0.0f, 1.0f);
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
}

void AnimInstance::Initialize(SkeletalMeshComponent* owningComponent)
{
    m_OwningComponent = owningComponent;
    m_LocomotionState = {};
    OnInitialize();
}

const AnimationAsset* AnimInstance::ResolveAnimationAsset(
    const AnimationEvaluationContext& context,
    const AssetPtr<AnimationAsset>& asset) const
{
    if (!context.AssetManager || (uint64)asset.GetHandle() == 0)
        return nullptr;

    AnimationAsset* animation =
        dynamic_cast<AnimationAsset*>(context.AssetManager->Load(asset.GetHandle()));
    if (!animation)
        return nullptr;

    if (context.Skeleton && (uint64)animation->m_SkeletonID != 0 &&
        (uint64)animation->m_SkeletonID != (uint64)context.Skeleton->ID)
    {
        return nullptr;
    }

    return animation;
}

bool AnimInstance::EvaluateClip(
    const AnimationEvaluationContext& context,
    const AssetPtr<AnimationAsset>& asset,
    const float playbackTime,
    const bool bLooping,
    TArray<Mat4>& outLocalPose) const
{
    const AnimationAsset* animation = ResolveAnimationAsset(context, asset);
    if (!context.Skeleton || !context.LocalBindPose || !animation)
        return false;

    const float sampleTime = AnimationRuntime::NormalizePlaybackTime(
        playbackTime,
        animation->m_DurationSeconds,
        bLooping);

    return AnimationRuntime::EvaluateLocalPose(
        context.Skeleton,
        animation,
        sampleTime,
        *context.LocalBindPose,
        outLocalPose,
        m_bRootMotionEnabled);
}

void LocomotionAnimInstance::TransitionTo(const LocomotionAnimState newState)
{
    if (m_State == newState)
        return;

    m_PreviousAnimation = SelectAnimationForState(m_State);
    m_PreviousState = m_State;
    m_PreviousStatePlaybackTime = m_StatePlaybackTime;
    m_TransitionElapsedTime = 0.0f;
    m_bTransitionActive = m_TransitionDuration > 1e-6f;

    m_State = newState;
    m_StateElapsedTime = 0.0f;
    m_StatePlaybackTime = 0.0f;
}

bool LocomotionAnimInstance::ShouldUseMoveState() const
{
    const bool bHasMovementInput = m_LocomotionState.bHasMovementInput;
    const float speed = m_LocomotionState.HorizontalSpeed;

    if (m_State == LocomotionAnimState::Move)
        return bHasMovementInput || speed >= m_MoveExitSpeed;

    return bHasMovementInput || speed >= m_MoveEnterSpeed;
}

bool LocomotionAnimInstance::IsStateLooping(const LocomotionAnimState state)
{
    return state == LocomotionAnimState::Idle ||
           state == LocomotionAnimState::Move ||
           state == LocomotionAnimState::Falling;
}
DEFINE_LOG_CATEGORY(AnimInstanceLog)
void LocomotionAnimInstance::Update(const float dt, const AnimationLocomotionState& locomotionState)
{
    m_LocomotionState = locomotionState;
    const float clampedDeltaTime = glm::max(0.0f, dt);
    m_StateElapsedTime += clampedDeltaTime;
    m_StatePlaybackTime += clampedDeltaTime;
    if (m_bTransitionActive)
    {
        m_PreviousStatePlaybackTime += clampedDeltaTime;
        m_TransitionElapsedTime += clampedDeltaTime;
        if (m_TransitionElapsedTime >= m_TransitionDuration)
            m_bTransitionActive = false;
    }
    
    //RB_LOG(AnimInstanceLog,debug,"State {}",(uint8)m_State);

    if (m_LocomotionState.bLanded)
    {
        TransitionTo(LocomotionAnimState::Land);
        return;
    }

    if (!m_LocomotionState.bIsGrounded)
    {
        // A real jump-start event must be able to retrigger Jump, including
        // from Falling for double-jumps. The velocity-based heuristic is only
        // for the first air-entry and must not flap back from Falling.
        if (m_LocomotionState.bJumpStarted)
        {
            if (m_State == LocomotionAnimState::Jump)
            {
                m_StateElapsedTime = 0.0f;
                m_StatePlaybackTime = 0.0f;
            }
            else
            {
                TransitionTo(LocomotionAnimState::Jump);
            }

            return;
        }

        const bool bShouldStartJumpState =
            m_State != LocomotionAnimState::Jump &&
            m_State != LocomotionAnimState::Falling &&
            m_LocomotionState.VerticalSpeed > m_JumpToFallingVerticalSpeed;

        if (bShouldStartJumpState)
        {
            TransitionTo(LocomotionAnimState::Jump);
            return;
        }

        if (m_State == LocomotionAnimState::Jump &&
            /*m_StateElapsedTime < m_JumpMinDuration &&*/
            m_LocomotionState.VerticalSpeed >= m_JumpToFallingVerticalSpeed)
        {
            return;
            
        }

        if (m_State != LocomotionAnimState::Falling &&
            (m_State == LocomotionAnimState::Land ||
             m_State == LocomotionAnimState::Jump ||
             m_LocomotionState.VerticalSpeed <= m_JumpToFallingVerticalSpeed))
        {
            TransitionTo(LocomotionAnimState::Falling);
        }

        return;
    }

    if (m_State == LocomotionAnimState::Land)
    {
        if (m_StateElapsedTime < m_LandMinDuration)
            return;

        if (ShouldUseMoveState())
            TransitionTo(LocomotionAnimState::Move);
        else
            TransitionTo(LocomotionAnimState::Idle);
        return;
    }

    if (ShouldUseMoveState())
    {
        TransitionTo(LocomotionAnimState::Move);
        return;
    }

    TransitionTo(LocomotionAnimState::Idle);
}

AssetPtr<AnimationAsset> LocomotionAnimInstance::SelectAnimationForState(const LocomotionAnimState state) const
{
    const SkeletalMeshComponent* component = GetOwningComponent();
    if (!component)
        return {};

    switch (state)
    {
    case LocomotionAnimState::Idle:
        if ((uint64)component->IdleAnimation.GetHandle() != 0)
            return component->IdleAnimation;
        break;
    case LocomotionAnimState::Move:
        if ((uint64)component->MoveAnimation.GetHandle() != 0)
            return component->MoveAnimation;
        break;
    case LocomotionAnimState::Jump:
        if (m_LocomotionState.JumpCount >= 2 &&
            (uint64)component->DoubleJumpAnimation.GetHandle() != 0)
        {
            return component->DoubleJumpAnimation;
        }
        if ((uint64)component->JumpAnimation.GetHandle() != 0)
            return component->JumpAnimation;
        break;
    case LocomotionAnimState::Falling:
        if ((uint64)component->FallingAnimation.GetHandle() != 0)
            return component->FallingAnimation;
        break;
    case LocomotionAnimState::Land:
        if ((uint64)component->LandAnimation.GetHandle() != 0)
            return component->LandAnimation;
        break;
    }

    return component->Animation;
}

float LocomotionAnimInstance::GetCurrentStateDuration(const AnimationEvaluationContext& context) const
{
    const AnimationAsset* animation = ResolveAnimationAsset(context, SelectAnimationForState());
    return animation ? animation->m_DurationSeconds : 0.0f;
}

bool LocomotionAnimInstance::Evaluate(const AnimationEvaluationContext& context, TArray<Mat4>& outLocalPose)
{
    const AssetPtr<AnimationAsset> currentAnimation = SelectAnimationForState(m_State);
    if ((uint64)currentAnimation.GetHandle() == 0)
        return false;

    const bool bCurrentLooping = IsStateLooping(m_State);

    const float duration = GetCurrentStateDuration(context);
    if (!bCurrentLooping && duration > 0.0f)
        m_StatePlaybackTime = glm::min(m_StatePlaybackTime, duration);

    TArray<Mat4> currentPose;
    if (!EvaluateClip(
        context,
        currentAnimation,
        m_StatePlaybackTime,
        bCurrentLooping,
        currentPose))
    {
        return false;
    }

    if (!m_bTransitionActive)
    {
        outLocalPose = std::move(currentPose);
        return true;
    }

    const AssetPtr<AnimationAsset> previousAnimation = m_PreviousAnimation;
    if ((uint64)previousAnimation.GetHandle() == 0)
    {
        outLocalPose = std::move(currentPose);
        return true;
    }

    const bool bPreviousLooping = IsStateLooping(m_PreviousState);
    const AnimationAsset* previousResolved = ResolveAnimationAsset(context, previousAnimation);
    if (!previousResolved)
    {
        outLocalPose = std::move(currentPose);
        return true;
    }

    float previousSampleTime = m_PreviousStatePlaybackTime;
    if (!bPreviousLooping && previousResolved->m_DurationSeconds > 0.0f)
        previousSampleTime = glm::min(previousSampleTime, previousResolved->m_DurationSeconds);

    TArray<Mat4> previousPose;
    if (!EvaluateClip(
            context,
            previousAnimation,
            previousSampleTime,
            bPreviousLooping,
            previousPose))
    {
        outLocalPose = std::move(currentPose);
        return true;
    }

    const float alpha = ComputeTransitionBlendAlpha(m_TransitionElapsedTime, m_TransitionDuration);
    BlendLocalPoses(previousPose, currentPose, alpha, outLocalPose);
    return true;
}

const char* LocomotionAnimInstance::GetDebugStateName() const
{
    return ToDebugString(m_State);
}
