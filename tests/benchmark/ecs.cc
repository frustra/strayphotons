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
static std::deque<Entity> readEntities;
static std::deque<Entity> writeEntities;
static ComponentIndex<Transform> transforms;
static ComponentIndex<Renderable> renderables;
static ComponentIndex<Script> scripts;

void renderThread() {
	MultiTimer timer("RenderThread");
	MultiTimer timer2("RenderThread Aquired");
	while (running) {
		auto start = std::chrono::high_resolution_clock::now();
		size_t goodCount = 0;
		std::string maxName;
		double maxValue = 0;
		std::vector<std::string> bad;
		{
			Timer t(timer);
			transforms.RLock();
			renderables.RLock();
			Timer t2(timer2);
			for (auto &ent : readEntities) {
				if (ent.Has<Renderable>()) {
					auto &transform = ent.Get<Transform>();
					auto &renderable = ent.Get<Renderable>();
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
			renderables.RUnlock();
			transforms.RUnlock();
		}
		// std::cout << "[RenderThread] Max value (" << maxName << ") at " << maxValue << ", Good: " << goodCount
		// 		  << ", Bad: " << bad.size() << "" << std::endl;
		std::this_thread::sleep_until(start + std::chrono::milliseconds(11));
	}
}

static std::atomic_size_t scriptWorkerQueue;

void scriptWorkerThread(bool master) {
	MultiTimer timer("ScriptWorkerThread", master);
	MultiTimer timer2("ScriptWorkerThread Aquired", master);
	while (running) {
		auto start = std::chrono::high_resolution_clock::now();
		{
			Timer t(timer);
			if (master) scripts.StartWrite();
			auto &validScripts = scripts.ValidIndexes();
			{
				Timer t2(timer2);
				while (running) {
					size_t entIndex = scriptWorkerQueue++;
					if (entIndex < validScripts.size()) {
						auto &valid = validScripts[entIndex];
						// "Run" script
						auto &script = *std::get<Script *>(valid);
						for (int i = 0; i < script.data.size(); i++) {
							script.data[i]++;
						}
					} else {
						break;
					}
				}
			}
			while (running && scriptWorkerQueue < THREAD_COUNT + validScripts.size()) {
				std::this_thread::yield();
			}

			if (master) {
				scripts.CommitWrite();
				scriptWorkerQueue = 0;
			}
		}
		std::this_thread::sleep_until(start + std::chrono::milliseconds(11));
	}
}

void transformWorkerThread() {
	MultiTimer timer("TransformWorkerThread");
	MultiTimer timer2("TransformWorkerThread Aquired");
	while (running) {
		auto start = std::chrono::high_resolution_clock::now();
		{
			Timer t(timer);
			auto &validTransforms = transforms.StartWrite();
			Timer t2(timer2);
			for (auto &valid : validTransforms) {
				auto &transform = *std::get<Transform *>(valid);
				transform.pos[0]++;
				transform.pos[1]++;
				transform.pos[2]++;
			}
			transforms.CommitWrite();
		}
		std::this_thread::sleep_until(start + std::chrono::milliseconds(11));
	}
}

#define TRANSFORM_DIVISOR 3
#define RENDERABLE_DIVISOR 3
#define SCRIPT_DIVISOR 10
#define EXPECTED_COUNT 900

int main(int argc, char **argv) {
	{
		Timer t("Alloc entities");
		readEntities.resize(ENTITY_COUNT);
		writeEntities.resize(ENTITY_COUNT);
	}

	{
		Timer t("Build links");
		transforms.Init(readEntities, writeEntities);
		renderables.Init(readEntities, writeEntities);
		scripts.Init(readEntities, writeEntities);
	}

	{
		Timer t("Populate entities");
		for (size_t i = 0; i < ENTITY_COUNT; i++) {
			readEntities[i] = Entity((ecs::eid_t)i);
			writeEntities[i] = Entity((ecs::eid_t)i);
			if (i % TRANSFORM_DIVISOR == 0) {
				readEntities[i].Set<Transform>(0.0, 0.0, 0.0, 1);
				writeEntities[i].Set<Transform>(0.0, 0.0, 0.0, 1);
			}
			if (i % RENDERABLE_DIVISOR == 0) {
				readEntities[i].Set<Renderable>("entity" + std::to_string(i));
				writeEntities[i].Set<Renderable>("entity" + std::to_string(i));
			}
			if (i % SCRIPT_DIVISOR == 0) {
				readEntities[i].Set<Script>(std::initializer_list<uint8_t>({0, 0, 0, 0, 0, 0, 0, 0}));
				writeEntities[i].Set<Script>(std::initializer_list<uint8_t>({0, 0, 0, 0, 0, 0, 0, 0}));
			}
		}
	}

	{
		Timer t("Build indexes");
		transforms.UpdateIndex(writeEntities);
		renderables.UpdateIndex(writeEntities);
		scripts.UpdateIndex(writeEntities);
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
		for (auto ent : readEntities) {
			if (ent.Has<Transform>()) {
				auto transform = ent.Get<Transform>();
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
		readEntities.clear();
		writeEntities.clear();
	}

	return 0;
}
