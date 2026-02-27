#include "pch.hpp"

#include "core/Logger.hpp"
#include "WindowSystem.hpp"

#include <imgui.h>
#include <backends/imgui_impl_sdl3.h>

WindowSystem::WindowSystem()
{
}

WindowSystem::~WindowSystem()
{
}

bool WindowSystem::Initialize()
{
	ZoneScopedN("WindowSystem::Initialize");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
	{
		Logger::Error("Failed to initialize SDL: %s", SDL_GetError());
		return false;
	}

	m_Window = SDL_CreateWindow("Woven Core", 1920, 1080, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!m_Window)
	{
		Logger::Error("Failed to create window: %s", SDL_GetError());
		return false;
	}

	Logger::Info("SDL initialized (1920x1080, Vulkan)");
	return true;
}

void WindowSystem::Shutdown()
{
	ZoneScopedN("WindowSystem::Shutdown");

	if (m_Window)
	{
		SDL_DestroyWindow(m_Window);
		m_Window = nullptr;
	}

	SDL_Quit();
}

void WindowSystem::ProcessEvent(const SDL_Event& event)
{
	ZoneScopedN("WindowSystem::ProcessEvent");
	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGui_ImplSDL3_ProcessEvent(&event);
	}
}
