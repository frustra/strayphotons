#pragma once

#include "assets/Model.hh"

namespace ecs
{
	struct Renderable
	{
		Renderable() {}
		Renderable(shared_ptr<sp::Model> model) : model(model) {}
		shared_ptr<sp::Model> model;
		bool hidden = false;
		float emissive = 0.0f;
		float voxelEmissive = 0.0f;
	};
}
