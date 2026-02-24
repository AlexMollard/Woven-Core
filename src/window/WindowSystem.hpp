#pragma once

#include "pch.hpp"

class WindowSystem
{
public:
	WindowSystem();
	~WindowSystem();

	bool Initialize();
	void Shutdown();

	SDL_Window* GetWindow() const
	{
		return m_Window;
	}

private:
	SDL_Window* m_Window = nullptr;
};
