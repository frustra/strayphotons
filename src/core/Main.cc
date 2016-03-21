#include <iostream>
using namespace std;

#include "core/Game.hh"
#include "core/Logging.hh"

#include <cstdio>

#ifdef _WIN32
#include <direct.h>
#define os_getcwd _getcwd
#else
#include <unistd.h>
#define os_getcwd getcwd
#endif

int main()
{
	char cwd[FILENAME_MAX];
	os_getcwd(cwd, FILENAME_MAX);
	Logf("Starting in directory: %s", cwd);

	try
	{
		sp::Game game;
		game.Start();
		return 0;
	}
	catch (const char *err)
	{
		Errorf(err);
	}
	return -1;
}

