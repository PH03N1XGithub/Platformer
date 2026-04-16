#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/LevelStarterActor.h"

#include "Engine/Gameplay/Framework/Pawn.h"
#include "Engine/Gameplay/Framework/PlayerController.h"
#include "Engine/Gameplay/Framework/MovementComponent.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/World.h"

DEFINE_LOG_CATEGORY(LevelStarterActorLog)

namespace
{
LevelStarterActor* g_ActiveLevelStarter = nullptr;
bool g_RuntimeTimerVisible = false;
float g_RuntimeElapsedSeconds = 0.0f;
}

LevelStarterActor::LevelStarterActor()
{
    SetCanEverTick(true);
    SetTickGroup(ActorTickGroup::PostPhysics);
}

void LevelStarterActor::BeginPlay()
{
    Actor::BeginPlay();

    m_ElapsedSeconds = 0.0f;
    m_CachedPawn = nullptr;

    if (m_bEnableLevelTimer)
    {
        g_ActiveLevelStarter = this;
        g_RuntimeTimerVisible = true;
        g_RuntimeElapsedSeconds = 0.0f;
    }
}

void LevelStarterActor::EndPlay()
{
    if (g_ActiveLevelStarter == this)
    {
        g_ActiveLevelStarter = nullptr;
        g_RuntimeTimerVisible = false;
        g_RuntimeElapsedSeconds = 0.0f;
    }

    Actor::EndPlay();
}

void LevelStarterActor::Tick(float dt)
{
    Actor::Tick(dt);

    if (m_bEnableLevelTimer && dt > 0.0f)
    {
        m_ElapsedSeconds += dt;
        if (g_ActiveLevelStarter == this)
            g_RuntimeElapsedSeconds = m_ElapsedSeconds;
    }

    Pawn* pawn = ResolvePlayerPawn();
    if (!pawn)
        return;

    ResetPawnIfFallen(*pawn);
}

bool LevelStarterActor::IsRuntimeTimerVisible()
{
    return g_RuntimeTimerVisible;
}

float LevelStarterActor::GetRuntimeElapsedSeconds()
{
    return g_RuntimeElapsedSeconds;
}

Pawn* LevelStarterActor::ResolvePlayerPawn()
{
    if (m_CachedPawn && !m_CachedPawn->IsPendingDestroy())
        return m_CachedPawn;

    World* world = GetWorld();
    Scene* scene = world ? world->GetScene() : nullptr;
    if (!scene)
        return nullptr;

    for (const auto& actorPtr : scene->GetActors())
    {
        Actor* actor = actorPtr.Get();
        if (!actor || actor->IsPendingDestroy())
            continue;

        PlayerController* playerController = dynamic_cast<PlayerController*>(actor);
        if (!playerController)
            continue;

        Pawn* possessedPawn = playerController->GetPawn();
        if (!possessedPawn || possessedPawn->IsPendingDestroy())
            continue;

        m_CachedPawn = possessedPawn;
        return m_CachedPawn;
    }

    return nullptr;
}

void LevelStarterActor::ResetPawnIfFallen(Pawn& pawn)
{
    const Vector3 location = pawn.GetActorLocation();
    if (location.z >= m_FallResetZ)
        return;

    pawn.SetActorLocation(m_ResetLocation);

    if (m_bResetVelocityOnTeleport)
    {
        if (MovementComponent* movement = pawn.GetMovementComponent())
            movement->SetVelocity(Vector3(0.0f));
    }

    RB_LOG(LevelStarterActorLog, info, "Reset pawn '{}' to start location ({}, {}, {})",
        pawn.GetName().c_str(), m_ResetLocation.x, m_ResetLocation.y, m_ResetLocation.z)
}
