#define SDL_MAIN_USE_CALLBACKS 1
#include "pch.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "core/Application.hpp"

// SDL3 Callback: Init
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
	auto* app = new Application();
	*appstate = app;

	if (!app->Init())
	{
		SDL_Log("Failed to initialize application");
		delete app;
		return SDL_APP_FAILURE;
	}

	return SDL_APP_CONTINUE;
}

// SDL3 Callback: Frame
SDL_AppResult SDL_AppIterate(void* appstate)
{
	ZoneScoped; // Trace SDL callback
	auto* app = static_cast<Application*>(appstate);
	app->Update();

	return app->ShouldClose() ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

// SDL3 Callback: Events
SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
	ZoneScoped; // Trace event handling
	auto* app = static_cast<Application*>(appstate);

	if (event->type == SDL_EVENT_QUIT)
	{
		app->RequestClose();
		return SDL_APP_SUCCESS;
	}

	return SDL_APP_CONTINUE;
}

// SDL3 Callback: Cleanup
void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
	ZoneScoped; // Trace cleanup
	auto* app = static_cast<Application*>(appstate);
	app->Shutdown();
	delete app;
}
