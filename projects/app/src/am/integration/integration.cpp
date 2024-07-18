
#ifdef AM_INTEGRATED

#include "integration.h"
#include "script/core.h"
#include "updatecomponent.h"
#include "gamedrawlists.h"
#include "memory/address.h"
#include "memory/hook.h"
#include "am/ui/imglue.h"
#include "am/ui/imgui_impl_dx11.h"
#include "am/system/system.h"
#include "am/asset/types/hotdrawable.h"
#include "ui/starbar.h"

#include <backends/imgui_impl_win32.h>
#include <imgui_internal.h>
#include <easy/profiler.h>

namespace
{
	std::atomic_bool s_InitializedFromGameThread;
	std::atomic_bool s_ShuttingDownRunGame;
	std::atomic_bool s_ShuttingDownSafeModeOperations;
	std::atomic_bool s_ShuttingDownEndFrame;
	gmAddress        s_EndFrame_Addr;
	gmAddress        s_GameUpdateAddr;
	gmAddress        s_SafeModeOperationsAddr;
}

void InitializeFromGameThread()
{
	if (s_InitializedFromGameThread)
		return;

	(void) SetThreadDescription(GetCurrentThread(), L"[RAGE] Game Thread");
	rageam::ThreadInfo::GetInstance()->SetIsMainThread();

	EASY_THREAD("Game Thread");

	// Must be called from a thread with game allocator in TLS
	rageam::integration::scrInit();
	s_InitializedFromGameThread = true;
}

// Called from CApp::RunGame() -> fwRenderThreadInterface::Synchronise()
// Render thread is guaranteed to be blocked here
void (*gImpl_PerformSafeModeOperations)(pVoid);
void aImpl_PerformSafeModeOperations(pVoid instance)
{
	InitializeFromGameThread();

	rageam::integration::GameIntegration::GetInstance()->Update();
	gImpl_PerformSafeModeOperations(instance);

	if (s_ShuttingDownSafeModeOperations)
	{
		Hook::Remove(s_SafeModeOperationsAddr);
		s_ShuttingDownSafeModeOperations = false;
		return;
	}
}

bool (*CApp_GameUpdate_gImpl)();
bool CApp_GameUpdate_aImpl()
{
	InitializeFromGameThread();

	EASY_BLOCK("Game Update", profiler::colors::Black);

	// We can't shut down until every component was released
	auto componentManager = rageam::integration::ComponentManager::GetInstance();
	bool canShutDown = !componentManager->HasAnythingToUpdate();
	if (s_ShuttingDownRunGame && canShutDown)
	{
		rageam::integration::scrShutdown();
		bool gameUpdateResult = CApp_GameUpdate_gImpl();
		Hook::Remove(s_GameUpdateAddr);
		// There's possible racing condition if library unloads before function returns,
		// but very unlikely to happen
		s_ShuttingDownRunGame = false;
		return gameUpdateResult;
	}

	auto integration = rageam::integration::GameIntegration::GetInstance();
	integration->EarlyUpdate();
	bool result = CApp_GameUpdate_gImpl();
	integration->LateUpdate();

	return result;
}

void (*gImpl_grcDevice_EndFrame)();
void aImpl_grcDevice_EndFrame()
{
	static bool s_Initialized = false;
	if (!s_Initialized)
	{
		EASY_THREAD("Render Thread");
		(void) SetThreadDescription(GetCurrentThread(), L"[RAGE] Render Thread");
		rageam::ThreadInfo::GetInstance()->SetIsRenderThread();
		s_Initialized = true;
	}

	auto integration = rageam::integration::GameIntegration::GetInstance();
	integration->GPUEndFrame();
	
	gImpl_grcDevice_EndFrame();

	if (s_ShuttingDownEndFrame)
	{
		// ImGui platform windows (that can be dragged outside main viewport)
		//  are managed from render thread (in GPUEndFrame)
		// They're already pending for closing because we call m_ImGlue->KillAllApps();
		//  in rageam::System::Destroy() before destroying game integration
		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			// We can't call UpdatePlatformWindows without 'faking' a frame
			// because there are frame counts asserts
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			ImGui::Render();

			ImGui::UpdatePlatformWindows();
		}

		// For windows that were created outside ui::App and weren't closed on KillAllApps
		for (int i = 1; i < GImGui->Viewports.Size; i++)
		{
			ImGuiViewportP* viewport = GImGui->Viewports[i];
			ConstString windowName = viewport->Window ? viewport->Window->Name : nullptr;

			AM_ASSERT(!viewport->PlatformWindowCreated,
				"ImGui -> Found still created platform window '%s', this will lead to game crash", windowName);

		}

		Hook::Remove(s_EndFrame_Addr);
		s_ShuttingDownEndFrame = false;
	}
}

void rageam::integration::GameIntegration::HookGameThread() const
{
	// CApp::RunGame, main game thread
#if APP_BUILD_2699_16_RELEASE_NO_OPT
	s_GameUpdateAddr =
		gmAddress::Scan("48 89 4C 24 08 48 81 EC 38 02 00 00 48 8D 05 ?? ?? ?? ?? 48");
#elif APP_BUILD_2699_16_RELEASE
	s_GameUpdateAddr =
		gmAddress::Scan("48 89 5C 24 08 57 48 81 EC 80 01 00 00 33");
#elif APP_BUILD_ID >= APP_BUILD_ID_3258_0
	s_GameUpdateAddr =
		gmAddress::Scan("48 83 EC 28 E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? B9");
#else
	s_GameUpdateAddr =
		gmAddress::Scan("48 83 EC 28 E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? B9");
#endif

	s_SafeModeOperationsAddr =
		gmAddress::Scan("B9 2D 92 F5 3C", "CGtaRenderThreadGameInterface::PerformSafeModeOperations")
#if APP_BUILD_2699_16_RELEASE_NO_OPT
		.GetAt(-0x65);
#else
		.GetAt(-0x31);
#endif

	Hook::Create(s_GameUpdateAddr, CApp_GameUpdate_aImpl, &CApp_GameUpdate_gImpl);
	Hook::Create(s_SafeModeOperationsAddr, aImpl_PerformSafeModeOperations, &gImpl_PerformSafeModeOperations);

	// System is created from DLL injector thread, we must initialize some things from game update thread
	while (!s_InitializedFromGameThread) {}
}

void rageam::integration::GameIntegration::HookRenderThread() const
{
	s_EndFrame_Addr = gmAddress::Scan(
#if APP_BUILD_2699_16_RELEASE_NO_OPT
		"48 89 4C 24 08 48 81 EC F8 00 00 00 E8 ?? ?? ?? ?? 8B 05", "rage::grcDevice::EndFrame");
#elif APP_BUILD_2699_16_RELEASE
		"40 55 53 56 57 41 54 41 56 41 57 48 8B EC 48 83 EC 70 48", "rage::grcDevice::EndFrame");
#else
		// Use offset pattern because mods may hook this function and first bytes will change
		"75 0C 44 38 35", "rage::grcDevice::EndFrame+0xC8").GetAt(-0xC8);
#endif;

	Hook::Create(s_EndFrame_Addr, aImpl_grcDevice_EndFrame, &gImpl_grcDevice_EndFrame);
}

void rageam::integration::GameIntegration::AntiDebugFixes() const
{
	// 1: Keyboard hook causes huge input delay when debugging game in visual studio
#ifndef APP_BUILD_2699_16_RELEASE_NO_OPT // In source build we simply disable this via param
	gmAddress mainPrologue = gmAddress::Scan("48 85 C0 74 0B 33 D2", "Main_Prologue+0xA").GetAt(-0xA);
	gmAddress addKeyboardHook = mainPrologue.GetAt(0x1A).GetCall(); // AddKeyboardHook()

	// Make sure that this hook won't be enabled ever again
	Hook::Nullsub(addKeyboardHook);
	
	// Unset existing hook if it was set
	HHOOK& hook = *addKeyboardHook.GetAt(0x2A).GetRef(3).To<HHOOK*>();
	if (hook != NULL)
	{
		AM_DEBUGF("GameIntegration::AntiDebugFixes() -> Removing keyboard hook.");
		UnhookWindowsHookEx(hook);
		hook = NULL;
	}
#endif
}

rageam::integration::GameIntegration::GameIntegration()
{
	AntiDebugFixes();

	// Register integration apps
	ui::ImGlue* ui = ui::GetUI();
	ui->AddApp(new StarBar());

	m_ComponentMgr = std::make_unique<ComponentManager>();
	m_GameDrawLists = std::make_unique<GameDrawLists>();
	m_OutlineRender = std::make_unique<graphics::OutlineRender>();
	m_GizmoManager = std::make_unique<gizmo::GizmoManager>();

	HookGameThread();
	HookRenderThread();
}

rageam::integration::GameIntegration::~GameIntegration()
{


	s_ShuttingDownEndFrame = true;
	while (s_ShuttingDownEndFrame) {}

	// Must be unhooked before CApp::RunGame() because depends on scrInit (CApp_GameUpdate_aImpl calls scrShutdown)
	s_ShuttingDownSafeModeOperations = true;
	while (s_ShuttingDownSafeModeOperations) {}

	s_ShuttingDownRunGame = true;
	while (s_ShuttingDownRunGame) {}

	m_ComponentMgr = nullptr;
	m_GameDrawLists = nullptr;
}

void rageam::integration::GameIntegration::GPUEndFrame()
{
	EASY_BLOCK("End Frame");

	// Window stuff must be updated from owner thread,
	// nothing bad will happen if we do it right here, before presenting image
	graphics::Window::GetInstance()->Update();

	m_OutlineRender->EndFrame();

	ui::ImGlue::GetInstance()->EndFrame();
}

void rageam::integration::GameIntegration::EarlyUpdate() const
{
	EASY_BLOCK("am Early Update", profiler::colors::color(55, 0, 180));
	m_ComponentMgr->EarlyUpdateAll();
}

void rageam::integration::GameIntegration::Update()
{
	EASY_BLOCK("am Update", profiler::colors::color(100, 0, 240));

	// Update called before building new game draw lists, release all old unused textures
	asset::HotDrawable::RemoveTexturesFromRenderThread(false);

	m_OutlineRender->NewFrame();

	System::GetInstance()->Update();

	// Render UI and update all components
	ui::ImGlue* ui = ui::ImGlue::GetInstance();
	ui->Lock();
	if (ui->BeginFrame())
	{
		auto gizmoManager = gizmo::GizmoManager::GetInstance();
		gizmoManager->BeginFrame();

		m_ComponentMgr->UpdateAll();
		ui->UpdateApps();
		m_GameDrawLists->EndFrame();

		gizmoManager->EndFrame();
		// ui->EndFrame will be called by render thread
	}
	ui->Unlock();
}

void rageam::integration::GameIntegration::LateUpdate() const
{
	EASY_BLOCK("am Update", profiler::colors::color(1, 135, 135));
	m_ComponentMgr->LateUpdateAll();
}

#endif
