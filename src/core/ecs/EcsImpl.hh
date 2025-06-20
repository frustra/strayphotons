/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "Ecs.hh"
#include "common/Common.hh"
#include "common/DispatchQueue.hh"
#include "common/Logging.hh"
#include "ecs/DynamicLibrary.hh"
#include "ecs/EntityReferenceManager.hh"
#include "ecs/EventQueue.hh"

// Components
#include "ecs/components/ActiveScene.hh"
#include "ecs/components/Animation.hh"
#include "ecs/components/Controller.hh"
#include "ecs/components/Events.hh"
#include "ecs/components/Focus.hh"
#include "ecs/components/Gui.hh"
#include "ecs/components/LaserEmitter.hh"
#include "ecs/components/LaserLine.hh"
#include "ecs/components/LaserSensor.hh"
#include "ecs/components/Light.hh"
#include "ecs/components/LightSensor.hh"
#include "ecs/components/Name.hh"
#include "ecs/components/OpticalElement.hh"
#include "ecs/components/Physics.hh"
#include "ecs/components/PhysicsJoints.hh"
#include "ecs/components/PhysicsQuery.hh"
#include "ecs/components/Renderable.hh"
#include "ecs/components/SceneConnection.hh"
#include "ecs/components/SceneInfo.hh"
#include "ecs/components/SceneProperties.hh"
#include "ecs/components/Screen.hh"
#include "ecs/components/Scripts.hh"
#include "ecs/components/Signals.hh"
#include "ecs/components/Sound.hh"
#include "ecs/components/Transform.h"
#include "ecs/components/Triggers.hh"
#include "ecs/components/View.hh"
#include "ecs/components/VoxelArea.hh"
#include "ecs/components/XrView.hh"

namespace ecs {
    struct EventQueuePool : public sp::NonMoveable {
        sp::LogOnExit logOnExit = "EventQueuePool shut down ==============================================";
        std::mutex mutex;
        std::deque<EventQueue> pool;
        std::priority_queue<size_t, std::vector<size_t>, std::greater<size_t>> freeList;
    };

    struct ECSContext : public sp::NonMoveable {
        // Order of these is important! Items are destroyed in bottom-up order.
        sp::LogOnExit logOnExit = "ECS shut down =========================================================";
        EventQueuePool eventQueues;
        ECS staging;
        ECS live;
        EntityReferenceManager refManager;
        sp::DispatchQueue transactionQueue = sp::DispatchQueue("ECSTransactionQueue");
    };
} // namespace ecs
