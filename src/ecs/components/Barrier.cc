#include "ecs/components/Barrier.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/Physics.hh"
#include "assets/AssetManager.hh"
#include "physx/PhysxManager.hh"
#include "physx/PhysxUtils.hh"

namespace ecs
{
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

		sp::PhysxManager::ActorDesc desc;
		desc.transform = physx::PxTransform(GlmVec3ToPxVec3(adjustedPos));
		desc.scale = physx::PxMeshScale(GlmVec3ToPxVec3(dimensions),
										physx::PxQuat(physx::PxIdentity));
		desc.dynamic = true;
		desc.kinematic = true;

		auto actor = px.CreateActor(model, desc);
		barrier.Assign<Physics>(actor, model);
		barrier.Assign<Barrier>();

		return barrier;
	}

	void Barrier::Close(Entity e, sp::PhysxManager &px)
	{
		auto barrier = e.Get<Barrier>();

		physx::PxRigidDynamic *actor = e.Get<Physics>()->dynamic;
		px.EnableCollisions(actor);

		auto renderable = e.Get<Renderable>();
		renderable->hidden = false;
		barrier->isOpen = false;
	}

	void Barrier::Open(Entity e, sp::PhysxManager &px)
	{
		auto barrier = e.Get<Barrier>();

		physx::PxRigidDynamic *actor = e.Get<Physics>()->dynamic;
		px.DisableCollisions(actor);

		auto renderable = e.Get<Renderable>();
		renderable->hidden = true;
		barrier->isOpen = true;
	}
}