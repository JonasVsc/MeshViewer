#pragma once
#include <memory>

struct SDL_Window;

namespace mv
{
	namespace vk 
	{ 
		class Context; 
		class Device;
	}

	class Viewer
	{
	public:

		Viewer();

		Viewer(const Viewer&) = delete;
		Viewer& operator=(const Viewer&) = delete;

		Viewer(Viewer&&) = delete;
		Viewer& operator=(Viewer&&) = delete;

		~Viewer();

		void run();

	private:

		
		bool quit{ false };
		SDL_Window* m_window{ nullptr };

		std::unique_ptr<vk::Context> m_vkcontext;
		std::unique_ptr<vk::Device> m_vkdevice;

	}; // class Viewer

} // namespace mv