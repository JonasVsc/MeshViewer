#pragma once

namespace mv
{
	class Renderer
	{
	public:

		Renderer();

		Renderer(const Renderer&) = delete;
		Renderer& operator=(const Renderer&) = delete;

		Renderer(Renderer&&) = delete;
		Renderer& operator=(Renderer&&) = delete;

		~Renderer();

	private:


	}; // class Renderer

} // namespace mv