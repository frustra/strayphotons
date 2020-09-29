#include "ecs.hh"

#include "Timer.hh"

#include <chrono>
#include <cstring>
#include <mutex>
#include <robin_hood.h>
#include <shared_mutex>
#include <thread>

using namespace benchmark;

std::atomic_bool running;
// static std::shared_mutex entityLock;
static std::deque<Entity> entities;
static std::shared_mutex transformLock;
static std::vector<Entity *> transformEntities;
static std::shared_mutex renderableLock;
static std::vector<Entity *> renderableEntities;
static std::shared_mutex scriptLock;
static std::vector<Entity *> scriptEntities;

void renderThread() {
	MultiTimer timer("RenderThread");
	while (running) {
		auto start = std::chrono::high_resolution_clock::now();
		size_t goodCount = 0;
		std::string maxName;
		double maxValue = 0;
		std::vector<std::string> bad;
		{
			Timer t(timer);
			std::shared_lock lock(transformLock);
			std::shared_lock lock2(renderableLock);
			for (auto ent : renderableEntities) {
				auto &transform = ent->Get<Transform>();
				auto &renderable = ent->Get<Renderable>();
				if (transform.pos[0] != transform.pos[1] || transform.pos[1] != transform.pos[2]) {
					bad.emplace_back(renderable.name);
				} else {
					goodCount++;
					if (transform.pos[0] > maxValue) {
						maxValue = transform.pos[0];
						maxName = renderable.name;
					}
				}
			}
		}
		// std::cout << "[RenderThread] Max value (" << maxName << ") at " << maxValue << ", Good: " << goodCount
		// 		  << ", Bad: " << bad.size() << "" << std::endl;
		std::this_thread::sleep_until(start + std::chrono::milliseconds(11));
	}
}

static std::atomic_size_t scriptWorkerQueue;

void scriptWorkerThread(bool master) {
	MultiTimer timer("ScriptWorkerThread", master);
	while (running) {
		auto start = std::chrono::high_resolution_clock::now();
		{
			Timer t(timer);
			std::shared_lock lock(scriptLock);
			while (running) {
				size_t scriptIndex = scriptWorkerQueue++;
				if (scriptIndex < scriptEntities.size()) {
					// "Run" script
					auto &script = scriptEntities[scriptIndex]->Get<Script>();
					for (int i = 0; i < script.data.size(); i++) {
						script.data[i]++;
					}
				} else {
					break;
				}
			}
			if (master) { scriptWorkerQueue = 0; }
		}
		std::this_thread::sleep_until(start + std::chrono::milliseconds(11));
	}
}

void transformWorkerThread() {
	MultiTimer timer("TransformWorkerThread");
	while (running) {
		auto start = std::chrono::high_resolution_clock::now();
		{
			Timer t(timer);
			std::unique_lock lock(transformLock);
			for (auto ent : transformEntities) {
				auto &transform = ent->Get<Transform>();
				transform.pos[0]++;
				transform.pos[1]++;
				transform.pos[2]++;
			}
		}
		std::this_thread::sleep_until(start + std::chrono::milliseconds(11));
	}
}

#define TRANSFORM_DIVISOR 1
#define RENDERABLE_DIVISOR 1
#define SCRIPT_DIVISOR 10
#define EXPECTED_COUNT 4

int main(int argc, char **argv) {
	{
		Timer t("Alloc entities");
		entities.resize(ENTITY_COUNT);
		transformEntities.resize(ENTITY_COUNT / TRANSFORM_DIVISOR);
		renderableEntities.resize(ENTITY_COUNT / RENDERABLE_DIVISOR);
		scriptEntities.resize(ENTITY_COUNT / SCRIPT_DIVISOR);
	}

	{
		Timer t("Populate entities");
		for (size_t i = 0; i < ENTITY_COUNT; i++) {
			if (i % TRANSFORM_DIVISOR == 0) {
				entities[i].Set<Transform>(0.0, 0.0, 0.0, 1);
				transformEntities[i / TRANSFORM_DIVISOR] = &entities[i];
			}
			if (i % RENDERABLE_DIVISOR == 0) {
				entities[i].Set<Renderable>("entity" + std::to_string(i));
				renderableEntities[i / RENDERABLE_DIVISOR] = &entities[i];
			}
			if (i % SCRIPT_DIVISOR == 0) {
				entities[i].Set<Script>(std::initializer_list<uint8_t>({0, 0, 0, 0, 0, 0, 0, 0}));
				scriptEntities[i / SCRIPT_DIVISOR] = &entities[i];
			}
		}
	}

	{
		Timer t("Run threads");
		running = true;

		std::thread threads[2 + THREAD_COUNT];
		threads[0] = std::thread(renderThread);
		threads[1] = std::thread(transformWorkerThread);
		for (size_t i = 0; i < THREAD_COUNT; i++) {
			threads[2 + i] = std::thread(scriptWorkerThread, i == 0);
		}

		std::this_thread::sleep_for(std::chrono::seconds(10));

		running = false;

		threads[0].join();
		threads[1].join();
		for (size_t i = 0; i < THREAD_COUNT; i++) {
			threads[2 + i].join();
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
		entities.clear();
		transformEntities.clear();
	}

	return 0;
}
