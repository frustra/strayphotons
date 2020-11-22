#pragma once

#include <Tecs.hh>
#include <ecs/Ecs.hh>
#include <ecs/NamedEntity.hh>
#include <ecs/components/Animation.hh>
#include <ecs/components/Barrier.hh>
#include <ecs/components/Controller.hh>
#include <ecs/components/Interact.hh>
#include <ecs/components/Light.hh>
#include <ecs/components/LightGun.hh>
#include <ecs/components/LightSensor.hh>
#include <ecs/components/Mirror.hh>
#include <ecs/components/Owner.hh>
#include <ecs/components/Physics.hh>
#include <ecs/components/Renderable.hh>
#include <ecs/components/SignalReceiver.hh>
#include <ecs/components/SlideDoor.hh>
#include <ecs/components/Transform.hh>
#include <ecs/components/TriggerArea.hh>
#include <ecs/components/Triggerable.hh>
#include <ecs/components/View.hh>
#include <ecs/components/VoxelInfo.hh>
#include <ecs/components/XRView.hh>

namespace ecs {
	template<typename T>
	T &Handle<T>::operator*() {
		auto lock = ecs.StartTransaction<Tecs::Write<T>>();
		return e.Get<T>(lock);
	}

	template<typename T>
	T *Handle<T>::operator->() {
		auto lock = ecs.StartTransaction<Tecs::Write<T>>();
		return &e.Get<T>(lock);
	}

	template<typename T, typename... Args>
	Handle<T> Entity::Assign(Args... args) {
		auto lock = em->tecs.StartTransaction<Tecs::AddRemove>();
		e.Set<T>(lock, args...);
		return Handle<T>(em->tecs, e);
	}

	template<typename T>
	bool Entity::Has() {
		auto lock = em->tecs.StartTransaction<Tecs::Read<T>>();
		return e.Has<T>(lock);
	}

	template<typename T>
	Handle<T> Entity::Get() {
		return Handle<T>(em->tecs, e);
	}

	template<typename Event>
	Subscription Entity::Subscribe(std::function<void(Entity, const Event &)> callback) {
		if (em == nullptr) { throw runtime_error("Cannot subscribe to events on NULL entity"); }
		return em->Subscribe(callback, this->e.id);
	}

	template<typename Event>
	void Entity::Emit(const Event &event) {
		em->Emit(this->e.id, event);
	}

	template<typename T>
	void EntityManager::DestroyAllWith(const T &value) {
		std::vector<Tecs::Entity> removeList;
		{
			auto lock = tecs.StartTransaction<Tecs::Read<T>>();
			for (auto e : lock.template EntitiesWith<T>()) {
				if (e.template Get<T>(lock) == value) { removeList.push_back(e); }
			}
		}
		for (auto e : removeList) {
			Emit(e.id, EntityDestruction());
		}
		{
			auto lock = tecs.StartTransaction<Tecs::AddRemove>();
			for (auto e : removeList) {
				e.Destroy(lock);
			}
		}
	}

	template<typename T, typename... Tn>
	std::vector<Entity> EntityManager::EntitiesWith() {
		auto lock = tecs.StartTransaction<Tecs::Read<T, Tn...>>();

		auto &entities = lock.template EntitiesWith<T>();
		std::vector<Entity> result;
		for (auto e : entities) {
			if (!e.template Has<T, Tn...>(lock)) continue;
			result.emplace_back(this, e);
		}
		return result;
	}

	template<typename T>
	Entity EntityManager::EntityWith(const T &value) {
		auto lock = tecs.template StartTransaction<Tecs::Read<T>>();

		for (auto e : lock.template EntitiesWith<T>()) {
			if (e.template Get<T>(lock) == value) return Entity(this, e);
		}
		return Entity();
	}

	template<typename Event>
	Subscription EntityManager::Subscribe(std::function<void(const Event &e)> callback) {
		// TODO-cs: this shares a lot of code in common with
		// Subscribe(function<void(Entity, const Event &)>), find a way
		// to eliminate the duplicate code.
		std::type_index eventType = typeid(Event);

		uint32 nonEntityEventIndex;

		try {
			nonEntityEventIndex = eventTypeToNonEntityEventIndex.at(eventType);
		}
		// Non-Entity Event never seen before, add it to the collection
		catch (const std::out_of_range &e) {
			registerNonEntityEventType<Event>();
			nonEntityEventIndex = eventTypeToNonEntityEventIndex.at(eventType);
		}

		std::lock_guard<std::recursive_mutex> lock(signalLock);
		auto &signal = nonEntityEventSignals.at(nonEntityEventIndex);
		auto connection = signal.insert(signal.end(), *reinterpret_cast<GenericCallback *>(&callback));

		return Subscription(this, &signal, connection);
	}

	template<typename Event>
	Subscription EntityManager::Subscribe(std::function<void(Entity, const Event &)> callback) {
		std::type_index eventType = typeid(Event);

		uint32 eventIndex;

		try {
			eventIndex = eventTypeToEventIndex.at(eventType);
		}
		// Event never seen before, add it to the collection
		catch (const std::out_of_range &e) {
			registerEventType<Event>();
			eventIndex = eventTypeToEventIndex.at(eventType);
		}

		std::lock_guard<std::recursive_mutex> lock(signalLock);
		auto &signal = eventSignals.at(eventIndex);
		auto connection = signal.insert(signal.end(), *reinterpret_cast<GenericEntityCallback *>(&callback));

		return Subscription(this, &signal, connection);
	}

	template<typename Event>
	Subscription EntityManager::Subscribe(std::function<void(Entity, const Event &e)> callback, Entity::Id entity) {
		auto &signal = entityEventSignals[entity][typeid(Event)];
		auto connection = signal.insert(signal.end(), *reinterpret_cast<GenericEntityCallback *>(&callback));
		return Subscription(this, &signal, connection);
	}

	template<typename Event>
	void EntityManager::Emit(Entity::Id e, const Event &event) {
		std::type_index eventType = typeid(Event);
		Entity entity(this, e);

		std::lock_guard<std::recursive_mutex> lock(signalLock);

		// signal the generic Event subscribers
		if (eventTypeToEventIndex.count(eventType) > 0) {
			auto eventIndex = eventTypeToEventIndex.at(eventType);
			auto &signal = eventSignals.at(eventIndex);
			auto connection = signal.begin();
			while (connection != signal.end()) {
				auto callback = (*reinterpret_cast<std::function<void(Entity, const Event &)> *>(&(*connection)));
				connection++;
				callback(entity, event);
			}
		}

		// now signal the entity-specific Event subscribers
		if (entityEventSignals.count(e) > 0) {
			if (entityEventSignals[e].count(eventType) > 0) {
				auto &signal = entityEventSignals[e][typeid(Event)];
				auto connection = signal.begin();
				while (connection != signal.end()) {
					auto callback = (*reinterpret_cast<std::function<void(Entity, const Event &)> *>(&(*connection)));
					connection++;
					callback(entity, event);
				}
			}
		}
	}

	template<typename Event>
	void EntityManager::Emit(const Event &event) {
		std::type_index eventType = typeid(Event);

		std::lock_guard<std::recursive_mutex> lock(signalLock);

		if (eventTypeToNonEntityEventIndex.count(eventType) > 0) {
			auto eventIndex = eventTypeToNonEntityEventIndex.at(eventType);
			auto &signal = nonEntityEventSignals.at(eventIndex);
			auto connection = signal.begin();
			while (connection != signal.end()) {
				auto callback = (*reinterpret_cast<std::function<void(const Event &)> *>(&(*connection)));
				connection++;
				callback(event);
			}
		}
	}

	template<typename Event>
	void EntityManager::registerEventType() {
		std::type_index eventType = typeid(Event);

		if (eventTypeToEventIndex.count(eventType) != 0) {
			std::stringstream ss;
			ss << "event type " << string(eventType.name()) << " is already registered";
			throw std::runtime_error(ss.str());
		}

		uint32 eventIndex = eventSignals.size();
		eventTypeToEventIndex[eventType] = eventIndex;
		eventSignals.push_back({});
	}

	template<typename Event>
	void EntityManager::registerNonEntityEventType() {
		std::type_index eventType = typeid(Event);

		if (eventTypeToNonEntityEventIndex.count(eventType) != 0) {
			std::stringstream ss;
			ss << "event type " << string(eventType.name()) << " is already registered";
			throw std::runtime_error(ss.str());
		}

		uint32 nonEntityEventIndex = nonEntityEventSignals.size();
		eventTypeToNonEntityEventIndex[eventType] = nonEntityEventIndex;
		nonEntityEventSignals.push_back({});
	}
} // namespace ecs
