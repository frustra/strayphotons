//=== Copyright Frustra Software, all rights reserved ===//

#include <iostream>
using namespace std;

#include "StrayPhotons.hh"
#include "core/Logging.hh"

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
		Errorf(err);
	}
	return -1;
}

