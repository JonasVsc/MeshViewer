#include "Application.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>

int main()
{
    try
    {
        mv::Application app;
        app.run();
    }
    catch (const vk::SystemError& err)
    {
        std::cerr << "Vulkan error: " << err.what() << std::endl;
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}