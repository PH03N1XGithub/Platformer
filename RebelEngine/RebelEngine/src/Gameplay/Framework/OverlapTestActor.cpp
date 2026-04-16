#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/OverlapTestActor.h"

#include "Engine/Components/BoxComponent.h"

DEFINE_LOG_CATEGORY(OverlapTestActorLog)

OverlapTestActor::OverlapTestActor()
{
    m_Trigger = &CreateDefaultSubobject<BoxComponent>();
    SetRootComponent(m_Trigger);

    if (m_Trigger)
    {
        m_Trigger->HalfExtent = Vector3(1.0f, 1.0f, 1.0f);
        m_Trigger->BodyType = ERBBodyType::Static;
        m_Trigger->ObjectChannel = CollisionChannel::Trigger;
        m_Trigger->bIsTrigger = true;
        m_Trigger->bGenerateOverlapEvents = true;
    }

    SetCanEverTick(false);
}

void OverlapTestActor::BeginPlay()
{
    Actor::BeginPlay();

    m_OverlapBeginCount = 0;
    m_OverlapEndCount = 0;

    RB_LOG(OverlapTestActorLog, info, "OverlapTestActor '{}' ready. Trigger={}", GetName().c_str(), (void*)m_Trigger)
}

void OverlapTestActor::OnActorBeginOverlap(Actor* otherActor, PrimitiveComponent* myComponent, PrimitiveComponent* otherComponent)
{
    ++m_OverlapBeginCount;
    RB_LOG(
        OverlapTestActorLog,
        info,
        "BeginOverlap '{}' with '{}' (myComp={}, otherComp={}) count={}",
        GetName().c_str(),
        otherActor ? otherActor->GetName().c_str() : "None",
        (void*)myComponent,
        (void*)otherComponent,
        m_OverlapBeginCount)
}

void OverlapTestActor::OnActorOverlapStay(Actor* otherActor, PrimitiveComponent* myComponent, PrimitiveComponent* otherComponent)
{
    if (!m_bLogOverlapStay)
        return;

    RB_LOG(
        OverlapTestActorLog,
        trace,
        "OverlapStay '{}' with '{}' (myComp={}, otherComp={})",
        GetName().c_str(),
        otherActor ? otherActor->GetName().c_str() : "None",
        (void*)myComponent,
        (void*)otherComponent)
}

void OverlapTestActor::OnActorEndOverlap(Actor* otherActor, PrimitiveComponent* myComponent, PrimitiveComponent* otherComponent)
{
    ++m_OverlapEndCount;
    RB_LOG(
        OverlapTestActorLog,
        info,
        "EndOverlap '{}' with '{}' (myComp={}, otherComp={}) count={}",
        GetName().c_str(),
        otherActor ? otherActor->GetName().c_str() : "None",
        (void*)myComponent,
        (void*)otherComponent,
        m_OverlapEndCount)
}

