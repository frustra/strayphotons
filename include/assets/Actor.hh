#ifndef SP_ACTOR_H
#define SP_ACTOR_H

#include "Common.hh"
#include <PxActor.h>
#include <glm/glm.hpp>

namespace sp
{
	class Asset;

	class Actor : public NonCopyable
	{
	public:
		Actor(const string &name, shared_ptr<Asset> asset);
		~Actor();
		glm::vec3 GetPosition () const;
		glm::quat GetRotation () const;
	private:
		physx::PxActor* pxActor;
		glm::vec3 position;
	};
}

#endif
