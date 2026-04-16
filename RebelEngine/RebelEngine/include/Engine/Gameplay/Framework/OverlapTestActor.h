#pragma once

#include "Engine/Scene/Actor.h"

struct BoxComponent;

class OverlapTestActor : public Actor
{
    REFLECTABLE_CLASS(OverlapTestActor, Actor)
public:
    OverlapTestActor();
    ~OverlapTestActor() override = default;

    void BeginPlay() override;
    void OnActorBeginOverlap(Actor* otherActor, PrimitiveComponent* myComponent, PrimitiveComponent* otherComponent) override;
    void OnActorOverlapStay(Actor* otherActor, PrimitiveComponent* myComponent, PrimitiveComponent* otherComponent) override;
    void OnActorEndOverlap(Actor* otherActor, PrimitiveComponent* myComponent, PrimitiveComponent* otherComponent) override;

    BoxComponent* GetTriggerComponent() const { return m_Trigger; }

private:
    BoxComponent* m_Trigger = nullptr;
    Bool m_bLogOverlapStay = false;
    uint32 m_OverlapBeginCount = 0;
    uint32 m_OverlapEndCount = 0;
};

REFLECT_CLASS(OverlapTestActor, Actor)
    REFLECT_PROPERTY(OverlapTestActor, m_bLogOverlapStay,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor | Rebel::Core::Reflection::EPropertyFlags::Editable);
    REFLECT_PROPERTY(OverlapTestActor, m_OverlapBeginCount,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
    REFLECT_PROPERTY(OverlapTestActor, m_OverlapEndCount,
        Rebel::Core::Reflection::EPropertyFlags::VisibleInEditor);
END_REFLECT_CLASS(OverlapTestActor)

