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

		Entity left;
		Entity right;
	};
}