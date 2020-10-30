#pragma once

#include "Timer.hh"
#include "ecs_util.h"

#include <atomic>
#include <bitset>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#define ENTITY_COUNT 1000000
#define THREAD_COUNT 0
#define SPINLOCK_RETRY_YIELD 10

static_assert(ATOMIC_INT_LOCK_FREE == 2, "std::atomic_int is not lock-free");

namespace benchmark {

	template<typename, typename...>
	class ComponentSetReadLock {};

	template<template<typename...> typename ECSType, typename... AllComponentTypes, typename... ReadComponentTypes>
	class ComponentSetReadLock<ECSType<AllComponentTypes...>, ReadComponentTypes...> {
	public:
		inline ComponentSetReadLock(ECSType<AllComponentTypes...> &ecs) : ecs(ecs) {
			ecs.validIndex.RLock();
			LockInOrder<AllComponentTypes...>();
		}

		inline ~ComponentSetReadLock() {
			UnlockInOrder<AllComponentTypes...>();
			ecs.validIndex.RUnlock();
		}

		template<typename T>
		inline constexpr const std::vector<size_t> &ValidIndexes() const {
			return ecs.template Storage<T>().readValidIndexes;
		}

		template<typename T>
		inline bool Has(size_t entityId) const {
			auto &validBitset = ecs.validIndex.readComponents[entityId];
			return ecs.template BitsetHas<T>(validBitset);
		}

		template<typename T, typename T2, typename... Tn>
		inline bool Has(size_t entityId) const {
			auto &validBitset = ecs.validIndex.readComponents[entityId];
			return ecs.template BitsetHas<T>(validBitset) && ecs.template BitsetHas<T2, Tn...>(validBitset);
		}

		template<typename T>
		inline const T &Get(size_t entityId) const {
			return ecs.template Storage<T>().readComponents[entityId];
		}

	private:
		// Call lock operations on ReadComponentTypes in the same order they are defined in AllComponentTypes
		// This is accomplished by filtering AllComponentTypes by ReadComponentTypes
		template<typename U>
		inline void LockInOrder() {
			if (is_type_in_set<U, ReadComponentTypes...>::value) { ecs.template Storage<U>().RLock(); }
		}

		template<typename U, typename U2, typename... Un>
		inline void LockInOrder() {
			LockInOrder<U>();
			LockInOrder<U2, Un...>();
		}

		template<typename U>
		inline void UnlockInOrder() {
			if (is_type_in_set<U, ReadComponentTypes...>::value) { ecs.template Storage<U>().RUnlock(); }
		}

		template<typename U, typename U2, typename... Un>
		inline void UnlockInOrder() {
			UnlockInOrder<U2, Un...>();
			UnlockInOrder<U>();
		}

		ECSType<AllComponentTypes...> &ecs;
	};

	template<typename, bool, typename...>
	class ComponentSetWriteTransaction {};

	template<template<typename...> typename ECSType, typename... AllComponentTypes, bool AllowAddRemove,
		typename... WriteComponentTypes>
	class ComponentSetWriteTransaction<ECSType<AllComponentTypes...>, AllowAddRemove, WriteComponentTypes...> {
	public:
		inline ComponentSetWriteTransaction(ECSType<AllComponentTypes...> &ecs) : ecs(ecs) {
			if (AllowAddRemove) {
				ecs.validIndex.StartWrite();
			} else {
				ecs.validIndex.RLock();
			}
			LockInOrder<AllComponentTypes...>();
		}

		inline ~ComponentSetWriteTransaction() {
			UnlockInOrder<AllComponentTypes...>();
			if (AllowAddRemove) {
				ecs.validIndex.CommitWrite();
			} else {
				ecs.validIndex.RUnlock();
			}
		}

		template<typename T>
		inline constexpr const std::vector<size_t> &ValidIndexes() const {
			return ecs.template Storage<T>().writeValidIndexes;
		}

		inline size_t AddEntity() {
			if (AllowAddRemove) {
				AddEntityToComponents<AllComponentTypes...>();
				size_t id = ecs.validIndex.writeComponents.size();
				ecs.validIndex.writeComponents.emplace_back();
				ecs.validIndex.writeValidDirty = true;
				return id;
			} else {
				throw std::runtime_error("Can't Add entity without setting AllowAddRemove to true");
			}
		}

		template<typename T>
		inline bool Has(size_t entityId) const {
			auto &validBitset = ecs.validIndex.writeComponents[entityId];
			return ecs.template BitsetHas<T>(validBitset);
		}

		template<typename T, typename T2, typename... Tn>
		inline bool Has(size_t entityId) const {
			auto &validBitset = ecs.validIndex.writeComponents[entityId];
			return ecs.template BitsetHas<T>(validBitset) && ecs.template BitsetHas<T2, Tn...>(validBitset);
		}

		template<typename T>
		inline const T &Get(size_t entityId) const {
			return ecs.template Storage<T>().writeComponents[entityId];
		}

		template<typename T>
		inline T &Get(size_t entityId) {
			return ecs.template Storage<T>().writeComponents[entityId];
		}

		template<typename T>
		inline void Set(size_t entityId, T &value) {
			ecs.template Storage<T>().writeComponents[entityId] = value;
			auto &validBitset = ecs.validIndex.writeComponents[entityId];
			if (!validBitset[ecs.template FindIndex<0, T>()]) {
				if (!AllowAddRemove)
					throw std::runtime_error("Can't Add new component without setting AllowAddRemove to true");
				validBitset[ecs.template FindIndex<0, T>()] = true;
				ecs.template Storage<T>().writeValidIndexes.emplace_back(entityId);
				ecs.template Storage<T>().writeValidDirty = true;
			}
		}

		template<typename T, typename... Args>
		inline void Set(size_t entityId, Args... args) {
			ecs.template Storage<T>().writeComponents[entityId] = std::move(T(args...));
			auto &validBitset = ecs.validIndex.writeComponents[entityId];
			if (!validBitset[ecs.template FindIndex<0, T>()]) {
				if (!AllowAddRemove)
					throw std::runtime_error("Can't Add new component without setting AllowAddRemove to true");
				validBitset[ecs.template FindIndex<0, T>()] = true;
				ecs.template Storage<T>().writeValidIndexes.emplace_back(entityId);
				ecs.template Storage<T>().writeValidDirty = true;
			}
		}

		// Only allow this if AllowAddRemove is true
		// template<typename T>
		// inline void Unset(size_t entityId) {
		// 	auto &validBitset = ecs.validIndex.writeComponents[entityId];
		// 	if (validBitset[ecs.template FindIndex<0, T>()]) {
		// 		validBitset[ecs.template FindIndex<0, T>()] = false;
		// 		// TODO: Find index in ecs.template Storage<T>().writeValidIndexes and remove it.
		// 		ecs.template Storage<T>().writeValidDirty = true;
		// 	}
		// }

	private:
		// Call lock operations on WriteComponentTypes in the same order they are defined in AllComponentTypes
		// This is accomplished by filtering AllComponentTypes by WriteComponentTypes
		template<typename U>
		inline void LockInOrder() {
			if (is_type_in_set<U, WriteComponentTypes...>::value) { ecs.template Storage<U>().StartWrite(); }
		}

		template<typename U, typename U2, typename... Un>
		inline void LockInOrder() {
			LockInOrder<U>();
			LockInOrder<U2, Un...>();
		}

		template<typename U>
		inline void UnlockInOrder() {
			if (is_type_in_set<U, WriteComponentTypes...>::value) { ecs.template Storage<U>().CommitWrite(); }
		}

		template<typename U, typename U2, typename... Un>
		inline void UnlockInOrder() {
			UnlockInOrder<U2, Un...>();
			UnlockInOrder<U>();
		}

		template<typename U>
		inline void AddEntityToComponents() {
			ecs.template Storage<U>().writeComponents.emplace_back();
			ecs.template Storage<U>().writeValidDirty = true;
		}

		template<typename U, typename U2, typename... Un>
		inline void AddEntityToComponents() {
			AddEntityToComponents<U>();
			AddEntityToComponents<U2, Un...>();
		}

		ECSType<AllComponentTypes...> &ecs;
	};

	template<typename T>
	class ComponentIndex {
	public:
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

		std::atomic_uint32_t readers = 0;
		std::atomic_uint32_t writer = 0;

		MultiTimer timer = MultiTimer(std::string("ComponentIndex Commit ") + typeid(T).name());
		// MultiTimer timerDirty = MultiTimer(std::string("ComponentIndex CommitDirty ") + typeid(T).name());
		inline void commitEntities() {
			// TODO: Possibly make this a compile-time branch
			// writeValidDirty means there was a change to the list of valid components (i.e new entities were added)
			if (writeValidDirty) {
				// Timer t(timerDirty);
				readComponents = writeComponents;
				readValidIndexes = writeValidIndexes;
				writeValidDirty = false;
			} else {
				Timer t(timer);
				// Based on benchmarks, it is faster to bulk copy if more than roughly 1/6 of the components are valid.
				if (writeValidIndexes.size() > writeComponents.size() / 6) {
					readComponents = writeComponents;
				} else {
					for (auto &valid : writeValidIndexes) {
						readComponents[valid] = writeComponents[valid];
					}
				}
			}
		}

		std::vector<T> readComponents;
		std::vector<T> writeComponents;
		std::vector<size_t> readValidIndexes;
		std::vector<size_t> writeValidIndexes;
		bool writeValidDirty = false;

		template<typename, typename...>
		friend class ComponentSetReadLock;
		template<typename, bool, typename...>
		friend class ComponentSetWriteTransaction;
	};

	// Template magic to create
	//     std::tuple<ComponentIndex<T1>, ComponentIndex<T2>, ...>
	// from
	//     ComponentIndexTuple<T1, T2, ...>::type
	template<typename... Tn>
	struct ComponentIndexTuple;

	template<typename Tnew>
	struct ComponentIndexTuple<Tnew> {
		using type = std::tuple<ComponentIndex<Tnew>>;
	};

	template<typename... Texisting, typename Tnew>
	struct ComponentIndexTuple<std::tuple<Texisting...>, Tnew> {
		using type = std::tuple<Texisting..., ComponentIndex<Tnew>>;
	};

	template<typename Tnew, typename... Tremaining>
	struct ComponentIndexTuple<Tnew, Tremaining...> {
		using type = typename ComponentIndexTuple<std::tuple<ComponentIndex<Tnew>>, Tremaining...>::type;
	};

	template<typename... Texisting, typename Tnew, typename... Tremaining>
	struct ComponentIndexTuple<std::tuple<Texisting...>, Tnew, Tremaining...> {
		using type = typename ComponentIndexTuple<std::tuple<Texisting..., ComponentIndex<Tnew>>, Tremaining...>::type;
	};

	// ECS contains all ECS data. Component types must be known at compile-time and are passed in as
	// template arguments.
	template<typename... Tn>
	class ECS {
	public:
		template<typename... Un>
		inline ComponentSetReadLock<ECS<Tn...>, Un...> ReadEntitiesWith() {
			return ComponentSetReadLock<ECS<Tn...>, Un...>(*this);
		}

		template<bool AllowAddRemove, typename... Un>
		inline ComponentSetWriteTransaction<ECS<Tn...>, AllowAddRemove, Un...> ModifyEntitiesWith() {
			return ComponentSetWriteTransaction<ECS<Tn...>, AllowAddRemove, Un...>(*this);
		}

	private:
		using ValidComponentSet = std::bitset<sizeof...(Tn)>;
		using IndexStorage = typename ComponentIndexTuple<Tn...>::type;

		template<size_t I, typename U>
		inline static constexpr size_t FindIndex() {
			static_assert(I < sizeof...(Tn), "Component does not exist");

			if constexpr (std::is_same<U, typename std::tuple_element<I, std::tuple<Tn...>>::type>::value) {
				return I;
			} else {
				return FindIndex<I + 1, U>();
			}
		}

		template<typename U>
		inline static constexpr bool BitsetHas(ValidComponentSet &validBitset) {
			return validBitset[FindIndex<0, U>()];
		}

		template<typename U, typename U2, typename... Un>
		inline static constexpr bool BitsetHas(ValidComponentSet &validBitset) {
			return BitsetHas<U>(validBitset) && BitsetHas<U2, Un...>(validBitset);
		}

		template<typename T>
		inline constexpr ComponentIndex<T> &Storage() {
			return std::get<ComponentIndex<T>>(indexes);
		}

		ComponentIndex<ValidComponentSet> validIndex;
		IndexStorage indexes;

		template<typename, typename...>
		friend class ComponentSetReadLock;
		template<typename, bool, typename...>
		friend class ComponentSetWriteTransaction;
	};
} // namespace benchmark
