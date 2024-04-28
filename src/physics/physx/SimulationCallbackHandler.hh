/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <PxSimulationEventCallback.h>

namespace sp {
    using namespace physx;

    class SimulationCallbackHandler : public PxSimulationEventCallback {
    public:
        SimulationCallbackHandler() {}

        void onConstraintBreak(PxConstraintInfo *constraints, PxU32 count) override;
        void onWake(PxActor **actors, PxU32 count) override;
        void onSleep(PxActor **actors, PxU32 count) override;
        void onContact(const PxContactPairHeader &pairHeader, const PxContactPair *pairs, PxU32 nbPairs) override;
        void onTrigger(PxTriggerPair *pairs, PxU32 count) override;
        void onAdvance(const PxRigidBody *const *bodyBuffer, const PxTransform *poseBuffer, const PxU32 count) override;

        static PxFilterFlags SimulationFilterShader(PxFilterObjectAttributes attributes0,
            PxFilterData filterData0,
            PxFilterObjectAttributes attributes1,
            PxFilterData filterData1,
            PxPairFlags &pairFlags,
            const void *constantBlock,
            PxU32 constantBlockSize);
    };
} // namespace sp
