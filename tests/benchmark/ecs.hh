#pragma once

#include "Timer.hh"

#include <Ecs.hh>
#include <atomic>
#include <bitset>
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
	typedef std::bitset<std::tuple_size<ComponentsTuple>::value> ValidComponents;

	template<size_t I, typename T>
	constexpr size_t index_of_component() {
		static_assert(I < std::tuple_size<ComponentsTuple>::value, "Component does not exist");

		if constexpr (std::is_same<T, typename std::tuple_element<I, ComponentsTuple>::type>::value) {
			return I;
		} else {
			return index_of_component<I + 1, T>();
		}
	}

	template<typename T>
	inline constexpr bool component_valid(const ValidComponents &components) {
		return components[index_of_component<0, T *>()];
	}

	template<typename T, typename T2, typename... Tn>
	inline constexpr bool component_valid(const ValidComponents &components) {
		return component_valid<T>(components) && component_valid<T2, Tn...>(components);
	}

	template<typename T>
	inline constexpr void set_component_valid(ValidComponents &components, bool value) {
		components[index_of_component<0, T *>()] = value;
	}

	class Entity {
	public:
		Entity() {}
		Entity(ecs::eid_t id) : id(id), valid(0) {}

		inline Entity &operator=(const Entity &other) {
			id = other.id;
			valid = other.valid;
			return *this;
		}

		inline const ecs::eid_t Id() const {
			return id;
		}

		// TODO: Use ComponentIndex lookup instead of local storage
		/*template<typename... Tn>
		inline constexpr bool Has() const {
			return component_valid<Tn...>(valid);
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
		}*/

	private:
		ecs::eid_t id;
		ValidComponents valid;
		// ComponentsTuple components;
	};

	template<typename T>
	class ComponentIndex {
	public:
		void Init(std::deque<Entity> &readEntities, std::deque<Entity> &writeEntities) {
			ecs::Assert(readEntities.size() == writeEntities.size(), "read and write entity lists should be same size");

			readComponents.resize(readEntities.size());
			writeComponents.resize(writeEntities.size());
		}

		inline void RLock() {
			int retry = 0;
			while (true) {
				uint32_t current = readers;
				if (writer != WRITER_COMMITING && current != READER_LOCKED) {
					uint32_t next = current + 1;
					if (readers.compare_exchange_weak(current, next)) {
						// Lock aquired
						return;
					}
				}

				if (retry++ > SPINLOCK_RETRY_YIELD) {
					retry = 0;
					std::this_thread::yield();
				}
			}
		}

		inline std::deque<T> &ReadComponents() {
			return readComponents;
		}

		inline std::vector<size_t> &ReadValidIndexes() {
			return readValidIndexes;
		}

		inline void RUnlock() {
			readers--;
		}

		inline void StartWrite() {
			int retry = 0;
			while (true) {
				uint32_t current = writer;
				if (current == WRITER_FREE) {
					if (writer.compare_exchange_weak(current, WRITER_STARTED)) {
						// Lock aquired
						return;
					}
				}

				if (retry++ > SPINLOCK_RETRY_YIELD) {
					retry = 0;
					std::this_thread::yield();
				}
			}
		}

		inline std::deque<T> &WriteComponents() {
			return writeComponents;
		}

		inline std::vector<size_t> &WriteValidIndexes() {
			// TODO: Set writeValidDirty if indexes changed
			return writeValidIndexes;
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
			if (writeValidIndexes.size() >= writeComponents.size() / 2) {
				readComponents = writeComponents;
			} else {
				for (auto &valid : writeValidIndexes) {
					readComponents[valid] = writeComponents[valid];
				}
			}
			if (writeValidDirty) {
				readValidIndexes = writeValidIndexes;
				writeValidDirty = false;
			}
		}

		std::atomic_uint32_t readers = 0;
		std::atomic_uint32_t writer = 0;

		std::deque<T> readComponents;
		std::deque<T> writeComponents;
		std::vector<size_t> readValidIndexes;
		std::vector<size_t> writeValidIndexes;
		bool writeValidDirty = false;
	};
} // namespace benchmark
