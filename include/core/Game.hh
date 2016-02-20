//=== Copyright Frustra Software, all rights reserved ===//

#ifndef SP_GAME_H
#define SP_GAME_H

#include <string>

namespace sp
{
	class Game
	{
	public:
		Game();
		virtual ~Game();

		virtual void Start();
		virtual bool Frame() = 0;
		virtual bool ShouldStop() = 0;
	};
}

#endif

