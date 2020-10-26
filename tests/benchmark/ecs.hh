#pragma once

#include "Timer.hh"

#include <Ecs.hh>
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <thread>

#define ENTITY_COUNT 1000000
#define THREAD_COUNT 0
#define SPINLOCK_RETRY_YIELD 10

static_assert(ATOMIC_INT_LOCK_FREE == 2, "std::atomic_int is not lock-free");

namespace benchmark {
	struct Transform {
		double pos[3] = {0};
		uint64_t generation = 0;

		Transform() {}
		Transform(double x, double y, double z) {
			pos[0] = x;
			pos[1] = y;
			pos[2] = z;
		}
		Transform(double x, double y, double z, uint64_t generation) : generation(generation) {
			pos[0] = x;
			pos[1] = y;
			pos[2] = z;
		}
	};

	struct Script {
		std::vector<uint8_t> data;

		Script() {}
		Script(uint8_t *data, size_t size) : data(data, data + size) {}
		Script(std::initializer_list<uint8_t> init) : data(init) {}
	};

	struct Renderable {
		std::string name;

		Renderable() {}
		Renderable(std::string name) : name(name) {}
	};

	typedef std::tuple<Transform *, Renderable *, Script *> ComponentsTuple;

	template<size_t I, typename T>
	constexpr size_t index_of_component() {
		static_assert(I < std::tuple_size<ComponentsTuple>::value, "Component does not exist");

		if constexpr (std::is_same<T, typename std::tuple_element<I, ComponentsTuple>::type>::value) {
			return I;
		} else {
			return index_of_component<I + 1, T>();
		}
	}

	class Entity {
	public:
		Entity() {}
		Entity(ecs::eid_t id) : id(id), valid({0}) {}

		inline Entity &operator=(const Entity &other) {
			id = other.id;
			valid = other.valid;
			return *this;
		}

		inline const ecs::eid_t Id() const {
			return id;
		}

		template<typename T>
		inline constexpr bool Has() const {
			return valid[index_of_component<0, T *>()];
		}

		template<typename T>
		inline constexpr T &Get() {
			return *std::get<T *>(components);
		}

		template<typename T>
		inline T *&GetPtr() {
			return std::get<T *>(components);
		}

		template<typename T>
		inline void SetPtr(T *ptr) {
			std::get<T *>(components) = ptr;
		}

		template<typename T>
		inline void Set(T &value) {
			*std::get<T *>(components) = value;
			valid[index_of_component<0, T *>()] = true;
		}

		template<typename T, typename... Args>
		inline void Set(Args &&... args) {
			*std::get<T *>(components) = std::move(T(std::forward<Args>(args)...));
			valid[index_of_component<0, T *>()] = true;
		}

		template<typename T>
		inline void Unset(T &value) {
			valid[index_of_component<0, T *>()] = false;
		}

	private:
		ecs::eid_t id;
		std::array<bool, std::tuple_size<ComponentsTuple>::value> valid;
		ComponentsTuple components;
	};

	template<typename T>
	class ComponentIndex {
	public:
		void Init(std::deque<Entity> &readEntities, std::deque<Entity> &writeEntities) {
			ecs::Assert(readEntities.size() == writeEntities.size(), "read and write entity lists should be same size");

			readComponents.resize(readEntities.size());
			writeComponents.resize(writeEntities.size());

			auto rComp = readComponents.begin();
			auto wComp = writeComponents.begin();
			auto rEnt = readEntities.begin();
			auto wEnt = writeEntities.begin();
			auto rCompEnd = readComponents.end();
			auto wCompEnd = writeComponents.end();
			auto rEntEnd = readEntities.end();
			auto wEntEnd = writeEntities.end();
			while (rComp != rCompEnd && wComp != wCompEnd && rEnt != rEntEnd && wEnt != wEntEnd) {
				rEnt->SetPtr<T>(&(*rComp));
				wEnt->SetPtr<T>(&(*wComp));
				rComp++;
				wComp++;
				rEnt++;
				wEnt++;
			}
		}

		void UpdateIndex(std::deque<Entity> &writeEntities) {
			validIndexes.clear();

			size_t i = 0;
			for (auto &writeEnt : writeEntities) {
				if (writeEnt.Has<T>()) validIndexes.emplace_back(std::make_tuple(i, writeEnt.GetPtr<T>()));
				i++;
			}
			std::cout << typeid(T).name() << " Valid Indexes: " << validIndexes.size() << std::endl;
		}

		// inline T &AddEntity(Entity &ent) {
		// 	T &comp = writeComponents.emplace_back();
		// 	ent.SetPtr(&comp);
		// }

		inline std::deque<T> &RLock() {
			int retry = 0;
			while (true) {
				uint32_t current = readers;
				if (writer != WRITER_COMMITING && current != READER_LOCKED) {
					uint32_t next = current + 1;
					if (readers.compare_exchange_weak(current, next)) {
						// Lock aquired
						return readComponents;
					}
				}

				if (retry++ > SPINLOCK_RETRY_YIELD) {
					retry = 0;
					std::this_thread::yield();
				}
			}
		}

		inline void RUnlock() {
			readers--;
		}

		inline std::vector<std::tuple<size_t, T *>> &StartWrite() {
			int retry = 0;
			while (true) {
				uint32_t current = writer;
				if (current == WRITER_FREE) {
					if (writer.compare_exchange_weak(current, WRITER_STARTED)) {
						// Lock aquired
						return validIndexes;
					}
				}

				if (retry++ > SPINLOCK_RETRY_YIELD) {
					retry = 0;
					std::this_thread::yield();
				}
			}
		}

		// Only use inside StartWrite/CommitWrite block.
		inline std::vector<std::tuple<size_t, T *>> &ValidIndexes() {
			return validIndexes;
		}

		inline void CommitWrite() {
			int retry = 0;
			while (true) {
				uint32_t current = readers;
				if (current == READER_FREE) {
					if (readers.compare_exchange_weak(current, READER_LOCKED)) {
						// Lock aquired
						commitEntities();
						break;
					}
				}

				if (retry++ > SPINLOCK_RETRY_YIELD) {
					retry = 0;
					std::this_thread::yield();
				}
			}

			// Unlock read and write copies
			readers = READER_FREE;
			writer = WRITER_FREE;
		}

	private:
		static const uint32_t WRITER_FREE = 0;
		static const uint32_t WRITER_STARTED = 1;
		static const uint32_t WRITER_COMMITING = 2;
		static const uint32_t READER_FREE = 0;
		static const uint32_t READER_LOCKED = UINT32_MAX;

		MultiTimer timer = MultiTimer(std::string("ComponentIndex Commit ") + typeid(T).name());
		inline void commitEntities() {
			Timer t(timer);

			// Based on benchmarking, it is faster to bulk copy if >= 1/2 of the components are valid.
			if (validIndexes.size() >= writeComponents.size() / 2) {
				readComponents = writeComponents;
			} else {
				for (auto &valid : validIndexes) {
					readComponents[std::get<size_t>(valid)] = *std::get<T *>(valid);
				}
			}
		}

		std::atomic_uint32_t readers = 0;
		std::atomic_uint32_t writer = 0;

		std::deque<T> readComponents;
		std::deque<T> writeComponents;
		std::vector<std::tuple<size_t, T *>> validIndexes;
	};
} // namespace benchmark
