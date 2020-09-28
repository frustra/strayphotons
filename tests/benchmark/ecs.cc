#include "ecs.hh"

#include "Timer.hh"

#include <cstring>
#include <mutex>
#include <robin_hood.h>
#include <thread>

// static std::shared_mutex transformLock;
static std::mutex workingLock;

using namespace benchmark;

void workerThreadTransformSingleThread(std::vector<Entity *> &transformEntities) {
	for (auto ent : transformEntities) {
		if (ent->Valid<Transform>()) {
			auto &transform = ent->Get<Transform>();
			transform.pos[0]++;
			transform.pos[1]++;
			transform.pos[2]++;
		}
	}
}

void workerThreadTransformGlobalLock(size_t thread_num, std::vector<Entity *> &transformEntities) {
	// Timer t("Thread " + std::to_string(thread_num));

	std::lock_guard lock(workingLock);

	size_t count = (transformEntities.size() / THREAD_COUNT) + 1;
	size_t i = count * thread_num;
	size_t end = std::min(i + count, transformEntities.size());
	for (; i < end; i++) {
		auto ent = transformEntities[i];
		if (ent->Valid<Transform>()) {
			auto &transform = ent->Get<Transform>();
			transform.pos[0]++;
			transform.pos[1]++;
			transform.pos[2]++;
		}
	}
}

void workerThreadTransformLocalLock(size_t thread_num, std::vector<Entity *> &transformEntities) {
	// Timer t("Thread " + std::to_string(thread_num));

	size_t count = (transformEntities.size() / THREAD_COUNT) + 1;
	size_t i = count * thread_num;
	size_t end = std::min(i + count, transformEntities.size());
	for (; i < end; i++) {
		auto ent = transformEntities[i];
		ent->Lock();
		if (ent->Valid<Transform>()) {
			auto &transform = ent->Get<Transform>();
			transform.pos[0]++;
			transform.pos[1]++;
			transform.pos[2]++;
		}
		ent->Unlock();
	}
}

void workerThreadTransformNoLock(size_t thread_num, std::vector<Entity *> &transformEntities) {
	// Timer t("Thread " + std::to_string(thread_num));

	size_t count = (transformEntities.size() / THREAD_COUNT) + 1;
	size_t i = count * thread_num;
	size_t end = std::min(i + count, transformEntities.size());
	for (; i < end; i++) {
		auto ent = transformEntities[i];
		if (ent->Valid<Transform>()) {
			auto &transform = ent->Get<Transform>();
			transform.pos[0]++;
			transform.pos[1]++;
			transform.pos[2]++;
		}
	}
}

#define TRANSFORM_DIVISOR 10
#define RENDERABLE_DIVISOR 4
#define SCRIPT_DIVISOR 3
#define EXPECTED_COUNT 4

int main(int argc, char **argv) {
	std::vector<Transform> transforms;
	std::vector<Renderable> renderables;
	std::vector<Script> scripts;
	std::vector<Entity> entities;
	std::vector<Entity *> transformEntities;

	{
		Timer t("Alloc entities");
		transforms.resize(ENTITY_COUNT / TRANSFORM_DIVISOR);
		renderables.resize(ENTITY_COUNT / RENDERABLE_DIVISOR);
		scripts.resize(ENTITY_COUNT / SCRIPT_DIVISOR);
		entities.resize(ENTITY_COUNT);
		transformEntities.resize(ENTITY_COUNT / TRANSFORM_DIVISOR);
	}

	{
		Timer t("Populate entities");
		for (size_t i = 0; i < ENTITY_COUNT; i++) {
			if (i % TRANSFORM_DIVISOR == 0) {
				entities[i].Set(transforms[i / TRANSFORM_DIVISOR]);
				transformEntities[i / TRANSFORM_DIVISOR] = &entities[i];
			}
			if (i % RENDERABLE_DIVISOR == 0) { entities[i].Set(renderables[i / RENDERABLE_DIVISOR]); }
			if (i % SCRIPT_DIVISOR == 0) { entities[i].Set(scripts[i / SCRIPT_DIVISOR]); }
		}
	}

	{
		Timer t("Advance entities (single thread)");
		std::thread threads[1];
		threads[0] = std::thread(workerThreadTransformSingleThread, std::ref(transformEntities));
		threads[0].join();
	}

	{
		Timer t("Advance entities (global lock)");
		std::thread threads[THREAD_COUNT];
		for (size_t i = 0; i < THREAD_COUNT; i++) {
			threads[i] = std::thread(workerThreadTransformGlobalLock, i, std::ref(transformEntities));
		}
		for (size_t i = 0; i < THREAD_COUNT; i++) {
			threads[i].join();
		}
	}

	{
		Timer t("Advance entities (local lock)");
		std::thread threads[THREAD_COUNT];
		for (size_t i = 0; i < THREAD_COUNT; i++) {
			threads[i] = std::thread(workerThreadTransformLocalLock, i, std::ref(transformEntities));
		}
		for (size_t i = 0; i < THREAD_COUNT; i++) {
			threads[i].join();
		}
	}

	{
		Timer t("Advance entities (no lock)");
		std::thread threads[THREAD_COUNT];
		for (size_t i = 0; i < THREAD_COUNT; i++) {
			threads[i] = std::thread(workerThreadTransformNoLock, i, std::ref(transformEntities));
		}
		for (size_t i = 0; i < THREAD_COUNT; i++) {
			threads[i].join();
		}
	}

	int validCount = 0;
	{
		Timer t("Validate entities");
		int invalid = 0;
		for (auto ent : transformEntities) {
			if (ent->Valid<Transform>()) {
				auto transform = ent->Get<Transform>();
				if (transform.pos[0] != EXPECTED_COUNT || transform.pos[1] != EXPECTED_COUNT ||
					transform.pos[2] != EXPECTED_COUNT) {
					if (invalid == 0) {
						std::cerr << "Component is not in correct place! " << transform.pos[0] << ", "
								  << transform.pos[1] << ", " << transform.pos[2] << std::endl;
					}
					invalid++;
				}
				validCount++;
			}
		}
		if (invalid != 0) { std::cerr << "Error: " << std::to_string(invalid) << " invalid components" << std::endl; }
	}
	std::cout << std::to_string(validCount) << " valid components" << std::endl;

	{
		Timer t("Remove entities");
		transforms.clear();
		renderables.clear();
		scripts.clear();
		entities.clear();
		transformEntities.clear();
	}

	return 0;
}
