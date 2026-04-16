#pragma once

#include "Engine/Animation/AnimationAsset.h"
#include "Engine/Assets/AssetPtr.h"

class AssetManager;
struct SkeletonAsset;
struct SkeletalMeshComponent;

enum class AnimationMovementMode : uint8
{
    None = 0,
    Walking,
    Falling,
    Flying,
    Custom
};

struct AnimationLocomotionState
{
    Vector3 WorldVelocity = Vector3(0.0f);
    Vector3 WorldAcceleration = Vector3(0.0f);
    Vector3 WorldMoveInput = Vector3(0.0f);
    Vector2 LocalMoveDirection = Vector2(0.0f);

    float HorizontalSpeed = 0.0f;
    float VerticalSpeed = 0.0f;
    float GroundDistance = 0.0f;
    float TimeInAir = 0.0f;
    float ActorYawDegrees = 0.0f;
    float ControllerYawDegrees = 0.0f;
    int JumpCount = 0;

    bool bIsGrounded = false;
    bool bHasMovementInput = false;
    bool bJumpStarted = false;
    bool bLanded = false;
    bool bHasControllerYaw = false;

    AnimationMovementMode MovementMode = AnimationMovementMode::None;
};

struct AnimationEvaluationContext
{
    AssetManager* AssetManager = nullptr;
    const SkeletonAsset* Skeleton = nullptr;
    const TArray<Mat4>* LocalBindPose = nullptr;
};

class AnimInstance
{
public:
    virtual ~AnimInstance() = default;

    void Initialize(SkeletalMeshComponent* owningComponent);

    SkeletalMeshComponent* GetOwningComponent() const { return m_OwningComponent; }
    const AnimationLocomotionState& GetLocomotionState() const { return m_LocomotionState; }
    void SetRootMotionEnabled(bool enabled) { m_bRootMotionEnabled = enabled; }
    bool IsRootMotionEnabled() const { return m_bRootMotionEnabled; }

    virtual void Update(float dt, const AnimationLocomotionState& locomotionState) = 0;
    virtual bool Evaluate(const AnimationEvaluationContext& context, TArray<Mat4>& outLocalPose) = 0;

    virtual const char* GetDebugStateName() const = 0;
    virtual float GetDebugPlaybackTime() const = 0;

protected:
    virtual void OnInitialize() {}

    const AnimationAsset* ResolveAnimationAsset(const AnimationEvaluationContext& context, const AssetPtr<AnimationAsset>& asset) const;
    bool EvaluateClip(
        const AnimationEvaluationContext& context,
        const AssetPtr<AnimationAsset>& asset,
        float playbackTime,
        bool bLooping,
        TArray<Mat4>& outLocalPose) const;

protected:
    SkeletalMeshComponent* m_OwningComponent = nullptr;
    AnimationLocomotionState m_LocomotionState{};
    bool m_bRootMotionEnabled = true;
};

enum class LocomotionAnimState : uint8
{
    Idle = 0,
    Move,
    Jump,
    Falling,
    Land
};

class LocomotionAnimInstance final : public AnimInstance
{
public:
    void Update(float dt, const AnimationLocomotionState& locomotionState) override;
    bool Evaluate(const AnimationEvaluationContext& context, TArray<Mat4>& outLocalPose) override;

    const char* GetDebugStateName() const override;
    float GetDebugPlaybackTime() const override { return m_StatePlaybackTime; }

private:
    void TransitionTo(LocomotionAnimState newState);
    AssetPtr<AnimationAsset> SelectAnimationForState(LocomotionAnimState state) const;
    AssetPtr<AnimationAsset> SelectAnimationForState() const { return SelectAnimationForState(m_State); }
    float GetCurrentStateDuration(const AnimationEvaluationContext& context) const;
    bool ShouldUseMoveState() const;
    static bool IsStateLooping(LocomotionAnimState state);

private:
    LocomotionAnimState m_State = LocomotionAnimState::Idle;
    LocomotionAnimState m_PreviousState = LocomotionAnimState::Idle;
    AssetPtr<AnimationAsset> m_PreviousAnimation{};
    float m_StateElapsedTime = 0.0f;
    float m_StatePlaybackTime = 0.0f;
    float m_PreviousStatePlaybackTime = 0.0f;
    float m_TransitionElapsedTime = 0.0f;
    float m_TransitionDuration = 0.15f;
    bool m_bTransitionActive = false;
    float m_MoveEnterSpeed = 0.2f;
    float m_MoveExitSpeed = 0.1f;
    float m_JumpToFallingVerticalSpeed = 0.0f;
    float m_JumpMinDuration = 0.08f;
    float m_LandMinDuration = 0.12f;
};
