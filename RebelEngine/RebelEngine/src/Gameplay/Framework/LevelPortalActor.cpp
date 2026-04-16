#include "Engine/Framework/EnginePch.h"
#include "Engine/Gameplay/Framework/LevelPortalActor.h"

#include "Engine/Components/BoxComponent.h"
#include "Engine/Framework/BaseEngine.h"
#include "Engine/Gameplay/Framework/Pawn.h"
#include "Engine/Rendering/RenderModule.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/World.h"
#include <filesystem>

DEFINE_LOG_CATEGORY(LevelPortalActorLog)

namespace
{
void ApplyLightingFromScene(Scene& scene)
{
    if (!GEngine)
        return;

    RenderModule* renderer = GEngine->GetModuleManager().GetModule<RenderModule>();
    if (!renderer)
        return;

    const Scene::LightingData& lighting = scene.GetLightingData();

    RenderModule::DirectionalLightSettings dir = renderer->GetDirectionalLight();
    dir.Direction = lighting.DirectionalLight.Direction;
    dir.Color = lighting.DirectionalLight.Color;
    dir.Intensity = lighting.DirectionalLight.Intensity;
    dir.SpecularIntensity = lighting.DirectionalLight.SpecularIntensity;
    dir.SpecularPower = lighting.DirectionalLight.SpecularPower;
    renderer->SetDirectionalLight(dir);

    RenderModule::SkyAmbientSettings sky = renderer->GetSkyAmbient();
    sky.SkyColor = lighting.SkyAmbient.SkyColor;
    sky.GroundColor = lighting.SkyAmbient.GroundColor;
    sky.Intensity = lighting.SkyAmbient.Intensity;
    renderer->SetSkyAmbient(sky);

    RenderModule::EnvironmentLightingSettings env = renderer->GetEnvironmentLighting();
    env.UseEnvironmentMap = lighting.Environment.UseEnvironmentMap;
    env.DiffuseIBLIntensity = lighting.Environment.DiffuseIBLIntensity;
    env.SpecularIBLIntensity = lighting.Environment.SpecularIBLIntensity;
    renderer->SetEnvironmentLighting(env);
}

bool TryLoadSceneWithFallbacks(Scene& scene, const String& targetPath)
{
    if (scene.Deserialize(targetPath))
        return true;

    const std::filesystem::path rawPath(targetPath.c_str());
    const std::filesystem::path editorPrefixed = std::filesystem::path("Editor") / rawPath;
    if (scene.Deserialize(editorPrefixed.string().c_str()))
        return true;

    if (rawPath.has_parent_path())
    {
        const std::filesystem::path justFile = rawPath.filename();
        if (scene.Deserialize(justFile.string().c_str()))
            return true;
    }

    return false;
}
}

LevelPortalActor::LevelPortalActor()
{
    m_Trigger = &CreateDefaultSubobject<BoxComponent>();
    SetRootComponent(m_Trigger);

    if (m_Trigger)
    {
        m_Trigger->HalfExtent = Vector3(1.0f, 1.0f, 2.0f);
        m_Trigger->BodyType = ERBBodyType::Static;
        m_Trigger->ObjectChannel = CollisionChannel::Trigger;
        m_Trigger->bIsTrigger = true;
        m_Trigger->bGenerateOverlapEvents = true;
    }

    SetCanEverTick(false);
}

void LevelPortalActor::BeginPlay()
{
    Actor::BeginPlay();

    m_bTransitionQueued = false;
    m_TransitionTimer.Invalidate();
}

void LevelPortalActor::OnActorBeginOverlap(Actor* otherActor, PrimitiveComponent* myComponent, PrimitiveComponent* otherComponent)
{
    (void)myComponent;
    (void)otherComponent;

    if (m_bTransitionQueued || m_TargetScenePath.length() == 0)
        return;

    if (m_bRequirePawnOverlap)
    {
        Pawn* pawn = dynamic_cast<Pawn*>(otherActor);
        if (!pawn)
            return;
    }

    QueueLevelTransition();
}

void LevelPortalActor::QueueLevelTransition()
{
    World* world = GetWorld();
    if (!world)
        return;

    Scene* scene = world->GetScene();
    if (!scene)
        return;

    const String targetPath = m_TargetScenePath;
    m_bTransitionQueued = true;

    const float delay = std::max(0.001f, m_TransitionDelaySeconds);
    world->SetTimer(
        m_TransitionTimer,
        [world, targetPath]()
        {
            if (!world)
                return;

            Scene* activeScene = world->GetScene();
            if (!activeScene)
                return;

            if (!TryLoadSceneWithFallbacks(*activeScene, targetPath))
            {
                RB_LOG(LevelPortalActorLog, error, "Failed to load scene '{}'", targetPath.c_str())
                return;
            }

            world->SetScene(activeScene);
            ApplyLightingFromScene(*activeScene);
            world->BeginPlay();

            RB_LOG(LevelPortalActorLog, info, "Level transition -> '{}'", targetPath.c_str())
        },
        delay,
        false);
}
