//=== Copyright Frustra Software, all rights reserved ===//

#include <iostream>
using namespace std;

#include "StrayPhotons.hh"

int main()
{
	try
	{
		StrayPhotons game;
		game.Start();
		return 0;
	}
	catch (const char *err)
	{
		std::cerr << err << std::endl;
	}
	return -1;
}

