#include "StrayPhotons.hh"

StrayPhotons::StrayPhotons()
{
	graphics.CreateContext();
}

bool StrayPhotons::Frame()
{
	if (!graphics.Frame()) return false;

	return true;
}

bool StrayPhotons::ShouldStop()
{
	return !graphics.HasActiveContext();
}

