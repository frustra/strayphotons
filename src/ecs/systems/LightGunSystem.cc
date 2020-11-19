#include "ecs/systems/LightGunSystem.hh"

#include "core/Logging.hh"
#include "ecs/components/Controller.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightGun.hh"
#include "ecs/components/Transform.hh"
#include "ecs/components/View.hh"
#include "physx/PhysxUtils.hh"

#include <PxPhysicsAPI.h>
#include <game/GameLogic.hh>
#include <game/input/InputManager.hh>

namespace ecs {
	LightGunSystem::LightGunSystem(
		ecs::EntityManager *entities, sp::InputManager *input, sp::PhysxManager *physics, sp::GameLogic *logic)
		: entities(entities), input(input), physics(physics), logic(logic) {}

	LightGunSystem::~LightGunSystem() {}

	bool LightGunSystem::Frame(float dtSinceLastFrame) {
		Entity player = logic->GetPlayer();
		physx::PxRigidActor *playerActor = nullptr;

		if (player.Valid() && player.Has<HumanController>()) {
			auto *controller = player.Get<HumanController>()->pxController;
			if (controller) {
				playerActor = controller->getActor();
				if (playerActor) { this->physics->DisableCollisions(playerActor); }
			}
		}

		if (input != nullptr) {
			for (auto ent : entities->EntitiesWith<LightGun, Transform>()) {
				auto gun = ent.Get<LightGun>();

				if (input->IsPressed(sp::INPUT_ACTION_PRIMARY_TRIGGER)) {
					ShootLight(ent);
				} else if (input->IsPressed(sp::INPUT_ACTION_SECONDARY_TRIGGER)) {
					SuckLight(ent);
				}
			}
		}

		if (playerActor) { this->physics->EnableCollisions(playerActor); }

		return true;
	}

	Entity LightGunSystem::EntityRaycast(Entity &origin) {
		auto transform = origin.Get<Transform>();
		physx::PxRaycastBuffer hitBuff;
		bool hit = physics->RaycastQuery(origin,
			GlmVec3ToPxVec3(transform->GetGlobalPosition(*origin.GetManager())),
			GlmVec3ToPxVec3(transform->GetGlobalForward(*origin.GetManager())),
			1000.0f,
			hitBuff);

		if (!hit) return Entity();

		const physx::PxRaycastHit &touch = hitBuff.getAnyHit(0);
		return Entity(entities, physics->GetEntityId(*touch.actor));
	}

	void LightGunSystem::SuckLight(Entity &gun) {
		if (!gun.Has<LightGun>() || !gun.Has<Transform>()) {
			throw runtime_error("invalid entity for LightGunSystem::SuckLight");
		}

		auto lightGun = gun.Get<LightGun>();
		auto transform = gun.Get<Transform>();
		if (lightGun->hasLight) return;

		Entity hitEntity = EntityRaycast(gun);
		if (!hitEntity.Valid() || !hitEntity.Has<Light>()) { return; }

		auto light = hitEntity.Get<Light>();
		if (!light->on) return;

		light->on = false;
		lightGun->hasLight = true;
		if (!gun.Has<Light>()) {
			auto gunLight = gun.Assign<Light>();
			gunLight->intensity = 0.1;
			gunLight->spotAngle = glm::radians(10.0f);
			gunLight->tint = glm::vec3(200, 128, 128);
		}
		if (!gun.Has<View>()) {
			auto gunView = gun.Assign<View>();
			gunView->extents = glm::vec2(2048.f, 2048.f);
			gunView->clip = glm::vec2(0.1f, 70.0f);
		}

		auto gunLight = gun.Get<Light>();
		gunLight->on = true;
	}

	void LightGunSystem::ShootLight(Entity &gun) {
		if (!gun.Has<LightGun>() || !gun.Has<Transform>()) {
			throw runtime_error("invalid entity for LightGunSystem::ShootLight");
		}

		auto lightGun = gun.Get<LightGun>();
		auto transform = gun.Get<Transform>();
		if (!lightGun->hasLight) return;

		Entity hitEntity = EntityRaycast(gun);
		if (!hitEntity.Valid() || !hitEntity.Has<Light>()) { return; }

		auto light = hitEntity.Get<Light>();
		if (light->on) return;

		light->on = true;
		lightGun->hasLight = false;
		if (!gun.Has<Light>()) { return; }

		auto gunLight = gun.Get<Light>();
		gunLight->on = false;
	}
} // namespace ecs
