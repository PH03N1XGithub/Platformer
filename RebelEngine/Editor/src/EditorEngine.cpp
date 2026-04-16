#include "Engine/Framework/EnginePch.h"
#include "ImGUIModule.h"
#include "EditorEngine.h"
#include "imgui.h"
#include "Engine/Physics/PhysicsModule.h"
#include "Engine/Physics/PhysicsSystem.h"
#include "Engine/Rendering/RenderModule.h"
#include "ThirdParty/imgui_impl_opengl3.h"
#include "Engine/Scene/World.h"


DEFINE_LOG_CATEGORY(EditorLog);

namespace
{
void CaptureLightingIntoScene(BaseEngine& engine, Scene& scene)
{
	if (RenderModule* renderer = engine.GetModuleManager().GetModule<RenderModule>())
	{
		auto& lighting = scene.GetLightingData();

		const auto& dir = renderer->GetDirectionalLight();
		lighting.DirectionalLight.Direction = dir.Direction;
		lighting.DirectionalLight.Color = dir.Color;
		lighting.DirectionalLight.Intensity = dir.Intensity;
		lighting.DirectionalLight.SpecularIntensity = dir.SpecularIntensity;
		lighting.DirectionalLight.SpecularPower = dir.SpecularPower;

		const auto& sky = renderer->GetSkyAmbient();
		lighting.SkyAmbient.SkyColor = sky.SkyColor;
		lighting.SkyAmbient.GroundColor = sky.GroundColor;
		lighting.SkyAmbient.Intensity = sky.Intensity;

		const auto& env = renderer->GetEnvironmentLighting();
		lighting.Environment.UseEnvironmentMap = env.UseEnvironmentMap;
		lighting.Environment.DiffuseIBLIntensity = env.DiffuseIBLIntensity;
		lighting.Environment.SpecularIBLIntensity = env.SpecularIBLIntensity;
	}
}

void ApplyLightingFromScene(BaseEngine& engine, const Scene& scene)
{
	if (RenderModule* renderer = engine.GetModuleManager().GetModule<RenderModule>())
	{
		const auto& lighting = scene.GetLightingData();

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
}
}

EditorEngine::~EditorEngine()
{
}


bool EditorEngine::Initialize()
{
	Window::WindowProps props;
	props.Width = 1280;
	props.Height = 720;
	props.Title = "Rebel Editor";

	m_WindowSpecs = props;
	
	if (!BaseEngine::Initialize())
		return false;
	

	m_EditorCamera = new Camera(glm::vec3(0.0f, 5.0f, 3.0f));
	SetActiveCamera(m_EditorCamera);
	
	RB_LOG(EditorLog, info, "EditorEngine initialized successfully!");
	
	return true;
}

bool EditorEngine::SaveEditorScene(const String& path)
{
	if (IsPlaying())
	{
		RB_LOG(EditorLog, warn, "SaveEditorScene ignored while PIE is active.");
		return false;
	}

	Scene* scene = m_EditorScene ? m_EditorScene : GetActiveScene();
	if (!scene)
		return false;

	String targetPath = path;
	if (targetPath.length() == 0)
		targetPath = m_CurrentScenePath;

	if (targetPath.length() == 0)
		targetPath = "scene.Ryml";

	CaptureLightingIntoScene(*this, *scene);
	scene->Serialize(targetPath);
	m_CurrentScenePath = targetPath;
	return true;
}

bool EditorEngine::LoadEditorScene(const String& path)
{
	if (IsPlaying())
	{
		RB_LOG(EditorLog, warn, "LoadEditorScene ignored while PIE is active.");
		return false;
	}

	if (path.length() == 0)
		return false;

	Scene* scene = m_EditorScene ? m_EditorScene : GetActiveScene();
	if (!scene)
		return false;

	if (!scene->Deserialize(path))
		return false;

	ApplyLightingFromScene(*this, *scene);
	m_CurrentScenePath = path;
	SetActiveScene(scene);
	return true;
}


void EditorEngine::Shutdown()
{
	BaseEngine::Shutdown();
}

void EditorEngine::OnEngineEvent(const Event& e)
{
	if (e.Type == Event::Type::KeyPressed && e.Key == GLFW_KEY_F5)
	{
		if (IsPlaying())
			StopPlayInEditor();
		else
			StartPlayInEditor();
		return;
	}

	if (IsPlaying() && e.Type == Event::Type::KeyPressed && e.Key == GLFW_KEY_F8)
	{
		TogglePIECursorCaptured();
		return;
	}

	BaseEngine::OnEngineEvent(e);
}

CameraView EditorEngine::GetActiveCamera(const Float aspect) const
{
	if (IsPlaying() && !m_bPIECursorCaptured && m_EditorCamera)
		return m_EditorCamera->GetCameraView(aspect);

	return BaseEngine::GetActiveCamera(aspect);
}

bool EditorEngine::ShouldProcessGameplayInput() const
{
	return IsPlaying() && m_bPIECursorCaptured;
}

void EditorEngine::SetEditorCameraCaptured(bool captured)
{
	if (m_bExternalEditorCameraCaptured == captured)
		return;

	m_bExternalEditorCameraCaptured = captured;

	if (Window* window = GetWindow())
		window->SetCursorDisabled(captured || m_bPIECursorCaptured);

	InputModule::ResetFrameState();
	InputModule::ForgetMousePosition();
}

void EditorEngine::StartPlayInEditor()
{
	if (IsPlaying())
	{
		RB_LOG(EditorLog, warn, "StartPlayInEditor ignored: already in runtime mode.");
		return;
	}

	PhysicsModule* physicsModule = GetModuleManager().GetModule<PhysicsModule>();
	if (!physicsModule)
	{
		RB_LOG(EditorLog, error, "StartPlayInEditor failed: PhysicsModule not found.");
		return;
	}

	m_RuntimeScene = new Scene();

	CaptureLightingIntoScene(*this, *m_EditorScene);
    m_EditorScene->Serialize("TempPIE.Ryml");
    m_RuntimeScene->Deserialize("TempPIE.Ryml");
	ApplyLightingFromScene(*this, *m_RuntimeScene);

	m_Mode = EngineMode::Runtime;
	SetActiveScene(m_RuntimeScene);
	if (m_World)
		m_World->BeginPlay();

	SetPIECursorCaptured(true);

	physicsModule->Init();
	
}

void EditorEngine::StopPlayInEditor()
{
	if (!IsPlaying() || !m_RuntimeScene)
	{
		RB_LOG(EditorLog, warn, "StopPlayInEditor ignored: runtime scene is not active.");
		return;
	}

	PhysicsModule* physicsModule = GetModuleManager().GetModule<PhysicsModule>();
	if (!physicsModule)
	{
		RB_LOG(EditorLog, error, "StopPlayInEditor failed: PhysicsModule not found.");
		return;
	}

	delete m_RuntimeScene;
	m_RuntimeScene = nullptr;

	SetPIECursorCaptured(false);
	m_Mode = EngineMode::Editor;
	SetActiveScene(m_EditorScene);
	ApplyLightingFromScene(*this, *m_EditorScene);
	physicsModule->Shutdown();
}

void EditorEngine::SetPIECursorCaptured(bool captured)
{
	if (m_bPIECursorCaptured == captured)
		return;

	m_bPIECursorCaptured = captured;

	if (Window* window = GetWindow())
		window->SetCursorDisabled(captured || m_bExternalEditorCameraCaptured);

	InputModule::ResetFrameState();
	InputModule::ForgetMousePosition();
}

void EditorEngine::TogglePIECursorCaptured()
{
	if (!IsPlaying())
		return;

	SetPIECursorCaptured(!m_bPIECursorCaptured);
}

void EditorEngine::Tick(float deltaTime)
{
	if (IsPlaying())
	{
		if (!m_bPIECursorCaptured && m_EditorCamera)
			m_EditorCamera->Update(deltaTime, GetWindow());

		BaseEngine::Tick(deltaTime);
	}
	else
	{
		if (!m_bExternalEditorCameraCaptured)
			m_EditorCamera->Update(deltaTime, GetWindow());
		
		if (m_ActiveScene)
		{
			m_ActiveScene->UpdateTransforms();
			m_ActiveScene->FlushPendingActorDestroy();
		}
		
		m_ModuleManager.TickModulesByType(TickType::PreSimulation, deltaTime); // nothink is running 
		m_ModuleManager.TickModulesByType(TickType::Physics, deltaTime); // phsics module for collition debug only 
		if (World* world = GetWorld())
		{
			if (Scene* scene = GetActiveScene())
			{
				if (PhysicsSystem* physics = world->TryGetPhysics())
					physics->EditorDebugDraw(*scene);
			}
		}
		m_ModuleManager.TickModulesByType(TickType::PostSimulation, deltaTime); // Animation module for debug only, idle 0 playback time pose evaluation
		// All render pipeline 
		m_ModuleManager.TickModulesByType(TickType::Render, deltaTime);
		m_ModuleManager.TickModulesByType(TickType::PostRender, deltaTime);
	}
}

void EditorEngine::OnInit()
{
	BaseEngine::OnInit();
}

void EditorEngine::OnShutdown()
{
	BaseEngine::OnShutdown();
}


