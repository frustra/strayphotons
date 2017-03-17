#pragma once

#include <Ecs.hh>

namespace ecs
{
	class SlideDoor
	{
	public:
		bool IsOpen();
		void Close();
		void Open();
		void ValidateDoor() const;
		void ApplyParams();

		Entity left;
		Entity right;

		float width = 1.0f;
		float openTime = 0.5f;
	};
}