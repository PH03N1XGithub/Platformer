#pragma once

#include "Engine/Scene/Actor.h"
#include "Engine/Scene/World.h"

struct BoxComponent;

class LevelPortalActor : public Actor
{
    REFLECTABLE_CLASS(LevelPortalActor, Actor)
public:
    LevelPortalActor();
    ~LevelPortalActor() override = default;

    void BeginPlay() override;
    void OnActorBeginOverlap(Actor* otherActor, PrimitiveComponent* myComponent, PrimitiveComponent* otherComponent) override;

    BoxComponent* GetTriggerComponent() const { return m_Trigger; }

private:
    void QueueLevelTransition();

private:
    BoxComponent* m_Trigger = nullptr;
    String m_TargetScenePath = "MainLevel.Ryml";
    Bool m_bRequirePawnOverlap = true;
    Bool m_bTransitionQueued = false;
    Float m_TransitionDelaySeconds = 0.05f;
    World::TimerHandle m_TransitionTimer;
};

REFLECT_CLASS(LevelPortalActor, Actor)
    REFLECT_PROPERTY(LevelPortalActor, m_TargetScenePath,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(LevelPortalActor, m_bRequirePawnOverlap,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(LevelPortalActor, m_TransitionDelaySeconds,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
END_REFLECT_CLASS(LevelPortalActor)

