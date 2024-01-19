//
// File: window.h
//
// Copyright (C) 2023 ranstar74. All rights violated.
//
// Part of "Rage Am" Research Project.
//
#pragma once

#include "am/system/singleton.h"
#include "common/types.h"

#include <Windows.h>
#include <imgui.h>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace rageam::graphics
{
	LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	class Window : public Singleton<Window>
	{
		static constexpr ConstWString CLASS_NAME = L"amWindow";
#ifdef AM_STANDALONE
		static constexpr ConstWString WINDOW_NAME = L"rageAm";
#else
		static constexpr ConstWString WINDOW_NAME = L"Grand Theft Auto V | rageAm - RAGE Research Project";
#endif
		static constexpr int DEFAULT_WINDOW_WIDTH = 1200;
		static constexpr int DEFAULT_WINDOW_HEIGHT = 768;

		HMODULE m_Module = NULL;
		HWND	m_Handle = NULL;
		bool	m_MouseClipped = false;

		void Create(int width, int height, int x, int y);
		void Destroy();

	public:
		Window();
		~Window() override;

		void SetMouseVisible(bool visiblity) const;
		// Whether mouse cursor is locked within window bounds
		bool GetMouseClipped() const;
		void SetMouseClipped(bool clipped);

		void GetSize(int& outWidth, int& outHeight) const;

		// Relative to the window rect
		void GetMousePosition(int& outX, int& outY) const;
		void GetPosition(int& outX, int& outY) const;

		// In integration mode we hook WndProc, we can't do this in constructor because UI is not initialized yet
		// Must be called once before update loop
		void UpdateInit() const;
		bool Update() const;

		HWND GetHandle() const { return m_Handle; }
	};

	// Gets new size of window after resizing, if resize operation was done
	bool WindowGetNewSize(int& newWidth, int& newHeight);

	// Might be NULL if window was not created
	HWND WindowGetHWND();
}