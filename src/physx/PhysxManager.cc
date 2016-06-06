#include "physx/PhysxManager.hh"
#include "core/Logging.hh"

namespace sp
{
	PhysxManager::PhysxManager ()
	{
		Logf("PhysX starting up");
		pxFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, defaultAllocatorCallback, defaultErrorCallback);
		pxSDK = PxCreatePhysics(PX_PHYSICS_VERSION, *pxFoundation, physx::PxTolerancesScale() );
		if(pxSDK == NULL) {
			Errorf("Error creating PhysX device.");
		}
	}
}