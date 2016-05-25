#ifndef ECS_RENDERABLE_H
#define ECS_RENDERABLE_H

#include "assets/Model.hh"

namespace ECS
{
	struct Renderable
	{
		Renderable() {}
		Renderable(shared_ptr<sp::Model> model) : model(model) {}
		shared_ptr<sp::Model> model;
	};
}

#endif
