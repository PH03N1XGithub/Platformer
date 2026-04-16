#pragma once

#include "Engine/Scene/Actor.h"

class Pawn;

class LevelStarterActor : public Actor
{
    REFLECTABLE_CLASS(LevelStarterActor, Actor)
public:
    LevelStarterActor();
    ~LevelStarterActor() override = default;

    void BeginPlay() override;
    void EndPlay() override;
    void Tick(float dt) override;

    static bool IsRuntimeTimerVisible();
    static float GetRuntimeElapsedSeconds();

private:
    Pawn* ResolvePlayerPawn();
    void ResetPawnIfFallen(Pawn& pawn);

private:
    float m_FallResetZ = -20.0f;
    Vector3 m_ResetLocation = Vector3(0.0f, 0.0f, 0.0f);
    bool m_bEnableLevelTimer = true;
    bool m_bResetVelocityOnTeleport = true;
    float m_ElapsedSeconds = 0.0f;
    Pawn* m_CachedPawn = nullptr;
};

REFLECT_CLASS(LevelStarterActor, Actor)
    REFLECT_PROPERTY(LevelStarterActor, m_FallResetZ,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(LevelStarterActor, m_bEnableLevelTimer,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(LevelStarterActor, m_bResetVelocityOnTeleport,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(LevelStarterActor, m_ElapsedSeconds,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
END_REFLECT_CLASS(LevelStarterActor)
