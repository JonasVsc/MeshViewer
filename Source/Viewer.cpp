#include "Viewer.h"
#include "Check.h"

#include "Vulkan/Context.h"
#include "Vulkan/Device.h"

#include "Renderer.h"

#include <SDL3/SDL.h>


namespace mv
{
	Viewer::Viewer()
	{
		FATAL_CHECK(SDL_Init(SDL_INIT_VIDEO), "Failed to initialize SDL");

		m_window = SDL_CreateWindow("MeshViewer", 640, 480, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
		FATAL_CHECK(m_window, "Failed to initialize window");

		m_vkcontext = std::make_unique<vk::Context>(m_window);

		m_vkdevice = std::make_unique<vk::Device>(*m_vkcontext);
		
		m_renderer = std::make_unique<Renderer>(*m_vkcontext, *m_vkdevice);

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
				if (event.type == SDL_EVENT_WINDOW_MINIMIZED)
				{
					int w = event.window.data1;
					int h = event.window.data2;
					
					(w == 0 || h == 0)
						? m_renderer->set_window_minimized(true) 
						: m_renderer->set_window_minimized(false);
				}
			}

			if (m_renderer->begin_frame())
			{
				m_renderer->begin_rendering();
				m_renderer->end_frame();
			}
		}
	}


} // namespace mv
