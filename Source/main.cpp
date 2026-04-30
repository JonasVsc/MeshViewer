#include "Viewer.h"
#include <iostream>

int main()
{
	try
	{
		mv::Viewer viewer;

		viewer.run();
	}
	catch (std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return 0;
}