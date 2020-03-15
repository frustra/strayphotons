#include "ecs/components/Barrier.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Physics.hh"
#include "assets/AssetManager.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

#include "Ecs.hh"
#include <picojson/picojson.h>
#include <assets/AssetHelpers.hh>

namespace ecs
{
	template<>
	bool Component<Barrier>::LoadEntity(Entity &dst, picojson::value &src)
	{
		auto barrier = dst.Assign<Barrier>();

		for (auto param : src.get<picojson::object>())
		{
			if (param.first == "isOpen")
			{
				barrier->isOpen = param.second.get<bool>();
			}
		}

		if (sp::ParametersExist(src, {"translate", "scale"}))
		{
			glm::vec3 translate;
			glm::vec3 scale;

			for (auto param : src.get<picojson::object>())
			{
				if (param.first == "translate")
				{
					translate = sp::MakeVec3(param.second);
				}
				else if (param.first == "scale")
				{
					scale = sp::MakeVec3(param.second);
				}
			}

			Errorf("Deserialization of barrier prefab component not currently supported.");
			return false;
			// ecs::Barrier::Create(translate, scale, px, *em);
		}

		if (barrier->isOpen)
		{
			Errorf("Deserialization of open barrier component not currently supported.");
			return false;
			// if (!dst.Has<Physics>() || !dst.Has<Renderable>())
			// {
			// 	throw std::runtime_error(
			// 		"barrier component must come after Physics and Renderable"
			// 	);
			// }
			// Barrier::Open(dst, px);
		}
		return true;
	}

	Entity Barrier::Create(
		const glm::vec3 &pos,
		const glm::vec3 &dimensions,
		sp::PhysxManager &px,
		ecs::EntityManager &em)
	{
		Entity barrier = em.NewEntity();
		auto model = sp::GAssets.LoadModel("box");
		barrier.Assign<Renderable>(model);
		auto transform = barrier.Assign<Transform>();
		transform->Scale(dimensions);

		// align bottom of barrier with given y pos
		glm::vec3 adjustedPos(pos);
		adjustedPos.y += dimensions.y / 2.f;
		transform->Translate(adjustedPos);

		sp::PhysxActorDesc desc;
		desc.transform = physx::PxTransform(GlmVec3ToPxVec3(adjustedPos));
		desc.scale = physx::PxMeshScale(GlmVec3ToPxVec3(dimensions),
										physx::PxQuat(physx::PxIdentity));
		desc.dynamic = true;
		desc.kinematic = true;

		auto actor = px.CreateActor(model, desc, barrier);
		barrier.Assign<Physics>(actor, model, desc);
		barrier.Assign<Barrier>();

		return barrier;
	}

	void Barrier::Close(Entity e, sp::PhysxManager &px)
	{
		auto barrier = e.Get<Barrier>();

		px.EnableCollisions(e.Get<Physics>()->actor);

		auto renderable = e.Get<Renderable>();
		renderable->hidden = false;
		barrier->isOpen = false;
	}

	void Barrier::Open(Entity e, sp::PhysxManager &px)
	{
		auto barrier = e.Get<Barrier>();

		px.DisableCollisions(e.Get<Physics>()->actor);

		auto renderable = e.Get<Renderable>();
		renderable->hidden = true;
		barrier->isOpen = true;
	}
}