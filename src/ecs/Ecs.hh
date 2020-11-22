#pragma once

#include <Tecs.hh>
#include <cstring>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <robin_hood.h>
#include <sstream>
#include <typeindex>

namespace picojson {
	class value;
}

namespace ecs {
	typedef std::string Name;

	class Animation;
	struct Barrier;
	struct Owner;
	enum class OwnerType;
	class HumanController;
	struct InteractController;
	struct Light;
	class LightGun;
	class LightSensor;
	struct Mirror;
	struct Physics;
	struct Renderable;
	class SignalReceiver;
	class SlideDoor;
	class Transform;
	struct TriggerArea;
	struct Triggerable;
	class View;
	struct VoxelArea;
	struct VoxelInfo;
	struct XRView;

	using ECS = Tecs::ECS<Name, Barrier, Owner, Animation, HumanController, InteractController, Light, LightGun,
		LightSensor, Mirror, Physics, Renderable, SignalReceiver, SlideDoor, Transform, TriggerArea, Triggerable, View,
		VoxelArea, VoxelInfo, XRView>;

	class EntityManager;
	class Subscription;

	template<typename T>
	class Handle {
	public:
		Handle(ECS &ecs, Tecs::Entity &e) : ecs(ecs), e(e) {}

		T &operator*();
		T *operator->();

	private:
		ECS &ecs;
		Tecs::Entity e;
	};

	class Entity {
	public:
		Entity() : e(), em(nullptr) {}
		Entity(EntityManager *em, const Tecs::Entity &e) : e(e.id), em(em) {}

		using Id = decltype(Tecs::Entity::id);

		Id GetId() const {
			return e.id;
		}

		EntityManager *GetManager() {
			return em;
		}

		inline bool Valid() const {
			return !!e && em != nullptr;
		}

		inline operator bool() const {
			return !!e;
		}

		inline bool operator==(const Entity &other) const {
			return e == other.e && em == other.em;
		}

		inline bool operator!=(const Entity &other) const {
			return e != other.e || em != other.em;
		}

		template<typename T, typename... Args>
		Handle<T> Assign(Args... args);

		template<typename T>
		bool Has();

		template<typename T>
		Handle<T> Get();

		void Destroy();

		template<typename Event>
		Subscription Subscribe(std::function<void(Entity, const Event &)> callback);

		template<typename Event>
		void Emit(const Event &event);

		inline std::string ToString() const {
			if (this->Valid()) {
				return "Entity(" + std::to_string(e.id) + ")";
			} else {
				return "Entity(NULL)";
			}
		}

		friend std::ostream &operator<<(std::ostream &os, const Entity e) {
			if (e.Valid()) {
				os << "Entity(" << e.e.id << ")";
			} else {
				os << "Entity(NULL)";
			}
			return os;
		}

	private:
		Tecs::Entity e;
		EntityManager *em = nullptr;
	};

	typedef std::function<void(Entity, void *)> GenericEntityCallback;
	typedef std::function<void(void *)> GenericCallback;

	class EntityDestruction {};

	class Subscription {
	public:
		Subscription();
		Subscription(EntityManager *manaager, std::list<GenericEntityCallback> *list,
			std::list<GenericEntityCallback>::iterator &c);
		Subscription(
			EntityManager *manaager, std::list<GenericCallback> *list, std::list<GenericCallback>::iterator &c);
		Subscription(const Subscription &other) = default;

		bool IsActive() const;

		void Unsubscribe();

	private:
		EntityManager *manager = nullptr;
		std::list<GenericEntityCallback> *entityConnectionList = nullptr;
		std::list<GenericEntityCallback>::iterator entityConnection;
		std::list<GenericCallback> *connectionList = nullptr;
		std::list<GenericCallback>::iterator connection;
	};

	class EntityManager {
	public:
		EntityManager();

		Entity NewEntity();
		void DestroyAll();
		template<typename T>
		void DestroyAllWith(const T &value);
		template<typename T, typename... Tn>
		std::vector<Entity> EntitiesWith();
		template<typename T>
		Entity EntityWith(const T &value);

		template<typename Event>
		Subscription Subscribe(std::function<void(const Event &e)> callback);
		template<typename Event>
		Subscription Subscribe(std::function<void(Entity, const Event &)> callback);
		template<typename Event>
		Subscription Subscribe(std::function<void(Entity, const Event &e)> callback, Entity::Id entity);
		template<typename Event>
		void Emit(Entity::Id e, const Event &event);
		template<typename Event>
		void Emit(const Event &event);

	private:
		template<typename Event>
		void registerEventType();
		template<typename Event>
		void registerNonEntityEventType();

		ECS tecs;
		std::recursive_mutex signalLock;

		typedef std::list<GenericEntityCallback> GenericSignal;
		std::vector<GenericSignal> eventSignals;

		robin_hood::unordered_flat_map<std::type_index, uint32_t> eventTypeToEventIndex;
		robin_hood::unordered_flat_map<std::type_index, uint32_t> eventTypeToNonEntityEventIndex;

		typedef std::list<GenericCallback> NonEntitySignal;
		std::vector<NonEntitySignal> nonEntityEventSignals;

		typedef robin_hood::unordered_flat_map<std::type_index, GenericSignal> SignalMap;
		robin_hood::unordered_flat_map<Entity::Id, SignalMap> entityEventSignals;

		friend class Entity;
		friend class Subscription;
	};
}; // namespace ecs
