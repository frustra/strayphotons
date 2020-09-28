#pragma once

#include <Ecs.hh>
#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <thread>

#define ENTITY_COUNT 10000000
#define THREAD_COUNT 8

namespace benchmark {
	struct Transform {
		float pos[3] = {0};
		uint64_t generation = 0;

		Transform() {}
		Transform(float x, float y, float z) {
			pos[0] = x;
			pos[1] = y;
			pos[2] = z;
		}
		Transform(float x, float y, float z, uint64_t generation) : generation(generation) {
			pos[0] = x;
			pos[1] = y;
			pos[2] = z;
		}
	};

	struct Script {
		std::vector<uint8_t> data;
	};

	struct Renderable {
		std::string name;
	};

	typedef std::tuple<Transform, Renderable, Script> ComponentsTuple;

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
		Entity() = default;

		// Move constructor
		Entity(Entity &&ent) {
			ent.Lock();
			this->id = ent.id;
			this->components = ent.components;
			this->valid = ent.valid;
			ent.Unlock();
		};

		template<typename T>
		inline constexpr bool Valid() {
			return valid[index_of_component<0, T>()];
		}

		template<typename T>
		inline constexpr T &Get() {
			return std::get<T>(components);
		}

		template<typename T>
		inline void Set(T &value) {
			std::get<T>(components) = value;
			valid[index_of_component<0, T>()] = true;
		}

		template<typename T>
		inline void Unset(T &value) {
			valid[index_of_component<0, T>()] = false;
		}

		inline void Lock() {
			while (locked.test_and_set(std::memory_order_acquire)) {
				std::this_thread::yield();
			}
		}

		inline void Unlock() {
			locked.clear(std::memory_order_release);
		}

	private:
		std::atomic_flag locked = ATOMIC_FLAG_INIT;

		ecs::eid_t id;
		std::array<bool, std::tuple_size<ComponentsTuple>::value> valid;
		ComponentsTuple components;
	};
} // namespace benchmark
