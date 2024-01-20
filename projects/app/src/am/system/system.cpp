#include "system.h"

#include "datamgr.h"
#include "am/asset/factory.h"
#include "am/asset/ui/assetwindowfactory.h"
#include "am/xml/doc.h"
#include "rage/grcore/fvf.h"
#include "exception/handler.h"

#ifdef AM_INTEGRATED
#include "am/integration/memory/hook.h"
#endif

void rageam::System::LoadDataFromXML()
{
	file::WPath configPath = DataManager::GetAppData() / DATA_FILE_NAME;
	if (IsFileExists(configPath))
	{
		try
		{
			XmlDoc xDoc;
			xDoc.LoadFromFile(configPath);
			XmlHandle xRoot = xDoc.Root();

			XmlHandle xWindow = xRoot.GetChild("Window");
			xWindow.GetChild("Width").GetValue(Data.Window.Width);
			xWindow.GetChild("Height").GetValue(Data.Window.Height);
			xWindow.GetChild("X").GetValue(Data.Window.X);
			xWindow.GetChild("Y").GetValue(Data.Window.Y);
			xWindow.GetChild("Maximized").GetValue(Data.Window.Maximized);

			XmlHandle xUi = xRoot.GetChild("UI");
			xUi.GetChild("FontSize").GetValue(Data.UI.FontSize);
		}
		catch (const XmlException& ex)
		{
			ex.Print();
		}
		HasData = true;
	}
	else
	{
		HasData = false;
	}
}

void rageam::System::SaveDataToXML() const
{
	file::WPath configPath = DataManager::GetAppData() / DATA_FILE_NAME;
	try
	{
		XmlDoc xDoc("SystemData");
		XmlHandle xRoot = xDoc.Root();

		XmlHandle xWindow = xRoot.AddChild("Window");
		xWindow.AddChild("Width").SetValue(Data.Window.Width);
		xWindow.AddChild("Height").SetValue(Data.Window.Height);
		xWindow.AddChild("X").SetValue(Data.Window.X);
		xWindow.AddChild("Y").SetValue(Data.Window.Y);
		xWindow.AddChild("Maximized").SetValue(Data.Window.Maximized);

		XmlHandle xUi = xRoot.AddChild("UI");
		xUi.AddChild("FontSize").SetValue(Data.UI.FontSize);

		xDoc.SaveToFile(configPath);
	}
	catch (const XmlException& ex)
	{
		ex.Print();
	}
}

void rageam::System::Destroy()
{
	if (!m_Initialized)
		return;

	// Order is opposite to initialization
	rage::grcVertexDeclaration::CleanUpCache();

	// Even though render is created before UI (in order to allocate GPU objects),
	// we must destroy it now because it is currently drawing UI
	m_Render = nullptr;
	m_ImGlue = nullptr;
	m_PlatformWindow = nullptr;

	// Integration is called by ImGlue, must be destroyed after
	// Ideally we can destroy them in the right order if we add rendering function
	AM_INTEGRATED_ONLY(m_Integration = nullptr);

	m_ImageCache = nullptr;

	asset::TxdAsset::ShutdownClass();
	asset::AssetFactory::Shutdown();
	ui::AssetWindowFactory::Shutdown();
	BackgroundWorker::Shutdown();
	ExceptionHandler::Shutdown();

	SaveDataToXML();

	AM_INTEGRATED_ONLY(Hook::Shutdown());

	rage::SystemHeap::Shutdown();

	m_Initialized = false;
}

void rageam::System::Init(bool withUI)
{
	Timer timer = Timer::StartNew();

	// Core
	AM_INTEGRATED_ONLY(Hook::Init());
	m_TexturePresetManager = std::make_unique<asset::TexturePresetStore>(); // Depends on LoadData
	LoadDataFromXML();
	ExceptionHandler::Init();
	asset::AssetFactory::Init();
	m_ImageCache = std::make_unique<graphics::ImageCache>();

	// Not a render thread in integrated mode, because called from Init launcher function
	AM_STANDALONE_ONLY((void)SetThreadDescription(GetCurrentThread(), L"[RAGEAM] Render Thread"));
	// Window (UI) + Render
	if (withUI)
		m_PlatformWindow = std::make_unique<graphics::Window>();
	m_Render = std::make_unique<graphics::Render>();
	if (withUI)
		m_ImGlue = std::make_unique<ui::ImGlue>();

	// Integration must be initialized after rendering/ui, it depends on it
	AM_INTEGRATED_ONLY(m_Integration = std::make_unique<integration::GameIntegration>());

	m_Initialized = true;

	timer.Stop();
	AM_TRACEF("[RAGEAM] Startup time: %llu ms", timer.GetElapsedMilliseconds());

	// Now both window and render are created/initialized, we can enter update loop
	if (withUI)
	{
		m_Render->EnterRenderLoop();
	}
}
