#include "Viewer.h"
#include "Assert.h"
#include "Vulkan/Context.h"
#include "Vulkan/Device.h"

#include <SDL3/SDL.h>


namespace mv
{
	Viewer::Viewer()
	{
		FATAL_CHECK(SDL_Init(SDL_INIT_VIDEO), "Failed to initialize SDL");

		m_window = SDL_CreateWindow("MeshViewer", 640, 480, SDL_WINDOW_VULKAN);
		FATAL_CHECK(m_window, "Failed to initialize window");

		m_vkcontext = std::make_unique<vk::Context>(m_window);
		m_vkdevice = std::make_unique<vk::Device>(*m_vkcontext);

		quit = false;
	}

	Viewer::~Viewer()
	{
		SDL_DestroyWindow(m_window);
	}

	void Viewer::run()
	{
		while (!quit)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
				{
					quit = true;
				}
			}
		}
	}


} // namespace mv
