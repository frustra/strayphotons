#pragma once

#include "graphics/GraphicsManager.hh"
#include "Common.hh"
#include "ecs/Ecs.hh"
#include "game/GameLogic.hh"
#include "game/InputManager.hh"
#include "game/GuiManager.hh"

namespace sp
{
	class Game
	{
	public:
		Game();
		~Game();

		void Start();
		bool Frame();
		bool ShouldStop();

		// Order is important.
		GuiManager gui;
		GraphicsManager graphics;
		InputManager input;
		EntityManager entityManager;
		GameLogic logic;

	private:
		double lastFrameTime;
	};
}
