//=== Copyright Frustra Software, all rights reserved ===//

#ifndef STRAY_PHOTONS_H
#define STRAY_PHOTONS_H

#include "core/Game.hh"
#include "graphics/GraphicsManager.hh"
#include "assets/AssetManager.hh"

#include <string>

class StrayPhotons : public sp::Game
{
public:
	StrayPhotons();

	bool Frame();
	bool ShouldStop();

	sp::AssetManager assets;
	sp::GraphicsManager graphics;
};

#endif

