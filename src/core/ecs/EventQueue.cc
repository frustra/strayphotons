/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "EventQueue.hh"

#include "assets/JsonHelpers.hh"
#include "ecs/Ecs.hh"
#include "ecs/EcsImpl.hh"
#include "strayphotons/Logging.hh"

#include <cstdint>
#include <glm/gtx/string_cast.hpp>
#include <memory>
#include <memory_resource>
#include <picojson.h>
#include <shared_mutex>
#include <sstream>
#include <string_view>

namespace ecs {
    const EventData &EventData::operator=(bool newBool) {
        type = EventDataType::Bool;
        b = newBool;
        return *this;
    }

    const EventData &EventData::operator=(int newInt) {
        type = EventDataType::Int;
        i = newInt;
        return *this;
    }

    const EventData &EventData::operator=(unsigned int newUint) {
        type = EventDataType::Uint;
        ui = newUint;
        return *this;
    }

    const EventData &EventData::operator=(float newFloat) {
        type = EventDataType::Float;
        f = newFloat;
        return *this;
    }

    const EventData &EventData::operator=(double newDouble) {
        type = EventDataType::Double;
        d = newDouble;
        return *this;
    }

    const EventData &EventData::operator=(glm::vec2 newVec2) {
        type = EventDataType::Vec2;
        vec2 = newVec2;
        return *this;
    }

    const EventData &EventData::operator=(glm::vec3 newVec3) {
        type = EventDataType::Vec3;
        vec3 = newVec3;
        return *this;
    }

    const EventData &EventData::operator=(glm::vec4 newVec4) {
        type = EventDataType::Vec4;
        vec4 = newVec4;
        return *this;
    }

    const EventData &EventData::operator=(Transform newTransform) {
        type = EventDataType::Transform;
        transform = newTransform;
        return *this;
    }

    const EventData &EventData::operator=(NamedEntity newNamedEntity) {
        type = EventDataType::NamedEntity;
        namedEntity = newNamedEntity;
        return *this;
    }

    const EventData &EventData::operator=(Entity newEntity) {
        type = EventDataType::Entity;
        ent = newEntity;
        return *this;
    }

    const EventData &EventData::operator=(EventString newString) {
        type = EventDataType::String;
        str = newString;
        return *this;
    }

    const EventData &EventData::operator=(EventBytes newBytes) {
        type = EventDataType::Bytes;
        bytes = newBytes;
        return *this;
    }

    bool EventData::operator==(const EventData &other) const {
        if (type != other.type) return false;
        return EventData::Visit(*this, [&other](auto &data) {
            using T = std::remove_cvref_t<decltype(data)>;
            return data == EventData::Get<T>(other);
        });
    }

    template<>
    bool StructMetadata::Load<EventData>(EventData &data, const picojson::value &src) {
        if (src.is<bool>()) {
            data = src.get<bool>();
        } else if (src.is<double>()) {
            data = src.get<double>();
        } else if (src.is<std::string>()) {
            auto &str = src.get<std::string>();
            if (str.starts_with("entity:")) {
                data = NamedEntity(Name(str.substr(7), EntityScope()));
            } else {
                data = str;
            }
        } else if (src.is<picojson::array>()) {
            auto &arr = src.get<picojson::array>();
            if (arr.size() == 2) {
                glm::vec2 vec(0);
                if (!sp::json::Load(vec, src)) {
                    Errorf("Unsupported EventData value: %s", src.to_str());
                    return false;
                }
                data = vec;
            } else if (arr.size() == 3) {
                glm::vec3 vec(0);
                if (!sp::json::Load(vec, src)) {
                    Errorf("Unsupported EventData value: %s", src.to_str());
                    return false;
                }
                data = vec;
            } else if (arr.size() == 4) {
                glm::vec4 vec(0);
                if (!sp::json::Load(vec, src)) {
                    Errorf("Unsupported EventData value: %s", src.to_str());
                    return false;
                }
                data = vec;
            } else {
                Errorf("Unsupported EventData array size: %u", arr.size());
                return false;
            }
        } else {
            Errorf("Unsupported EventData value: %s", src.to_str());
            return false;
        }
        return true;
    }

    template<>
    void StructMetadata::Save<EventData>(const EntityScope &scope,
        picojson::value &dst,
        const EventData &src,
        const EventData *def) {
        if (def && src == *def) return;

        EventData::Visit(src, [&](auto &data) {
            sp::json::Save(scope, dst, data);
        });
    }

    template<>
    void StructMetadata::SetScope<EventData>(EventData &dst, const EntityScope &scope) {
        if (dst.type == EventDataType::NamedEntity) {
            dst.namedEntity.SetScope(scope);
        }
    }

    uint64_t Event::Send(const DynamicLock<SendEventsLock> &lock, Entity target, const Event &event) {
        return EventBindings::SendEvent(lock, target, event);
    }

    uint64_t Event::SendNamed(const DynamicLock<SendEventsLock> &lock, const NamedEntity &target, const Event &event) {
        return EventBindings::SendEvent(lock, target, event);
    }

    uint64_t Event::SendRef(const DynamicLock<SendEventsLock> &lock, const EntityRef &target, const Event &event) {
        return EventBindings::SendEvent(lock, target, event);
    }

    std::string Event::ToString() const {
        std::stringstream ss;
        ss << std::string_view(this->name) << ": " << this->data;
        return ss.str();
    }

    std::ostream &operator<<(std::ostream &out, const EventData &v) {
        switch (v.type) {
        case EventDataType::Bool:
            out << "bool(" << v.b << ")";
            break;
        case EventDataType::Int:
            out << "int(" << v.i << ")";
            break;
        case EventDataType::Uint:
            out << "uint(" << v.ui << ")";
            break;
        case EventDataType::Float:
            out << "float(" << v.f << ")";
            break;
        case EventDataType::Double:
            out << "double(" << v.d << ")";
            break;
        case EventDataType::Vec2:
            out << glm::to_string(v.vec2);
            break;
        case EventDataType::Vec3:
            out << glm::to_string(v.vec3);
            break;
        case EventDataType::Vec4:
            out << glm::to_string(v.vec4);
            break;
        case EventDataType::Transform:
            out << glm::to_string(v.transform.offset) << " scale " << glm::to_string(v.transform.scale);
            break;
        case EventDataType::NamedEntity:
            out << v.namedEntity.Name().String();
            break;
        case EventDataType::Entity:
            out << std::to_string(v.ent);
            break;
        case EventDataType::String:
            out << "\"" << v.str << "\"";
            break;
        default:
            Abortf("Unexpected EventData type: %s", v.type);
        }
        return out;
    }

    EventQueue::EventQueue(uint32_t maxQueueSize) : capacity(maxQueueSize) {}

    EventQueue::~EventQueue() {
        DebugAssertf(handle.expired(), "EventQueue destroyed while referenced");
        AsyncEvent *entry = first;
        while (entry) {
            AsyncEvent *tmp = entry;
            entry = entry->next;
            GetECSContext().eventPool.deallocate(tmp, sizeof(AsyncEvent));
        }
    }

    thread_local sp::HeapVector<EventQueue::AsyncEvent *> pendingEvents;

    bool EventQueue::Add(Lock<>, const Event &event) {
        std::lock_guard l(mutex);
        if (count >= capacity) return false;
        void *memory = GetECSContext().eventPool.allocate(sizeof(AsyncEvent), alignof(AsyncEvent));
        if (last) {
            last = last->next = new (memory) AsyncEvent{event};
        } else {
            first = last = new (memory) AsyncEvent{event};
        }
        count++;
        pendingEvents.emplace_back(last);
        return true;
    }

    bool EventQueue::Poll(Event &eventOut) {
        std::lock_guard l(mutex);
        if (!first || first->commitId == 0) {
            eventOut = Event();
            return false;
        }

        DebugAssertf(first->commitId != 0, "EventQueue::Poll read a non-ready event");
        eventOut = *first;

        AsyncEvent *tmp = first;
        first = first->next;
        if (!first) last = nullptr;

        GetECSContext().eventPool.deallocate(tmp, sizeof(AsyncEvent));

        count--;
        return true;
    }

    bool EventQueue::Empty() const {
        std::shared_lock l(mutex);
        return !first || first->commitId == 0;
    }

    uint32_t EventQueue::Size() const {
        std::shared_lock l(mutex);
        return count;
    }

    uint32_t EventQueue::Capacity() const {
        std::shared_lock l(mutex);
        return capacity;
    }

    void EventQueue::CommitPendingEvents(uint64_t commitId) {
        for (auto &event : pendingEvents) {
            event->commitId = commitId;
        }
        pendingEvents.clear();
    }
} // namespace ecs
