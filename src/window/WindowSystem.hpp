#pragma once

#include "pch.hpp"

class WindowSystem
{
public:
	WindowSystem();
	~WindowSystem();

	bool Initialize();
	void Shutdown();
	void ProcessEvent(const SDL_Event& event);

	SDL_Window* GetWindow() const
	{
		return m_Window;
	}

private:
	SDL_Window* m_Window = nullptr;
};
