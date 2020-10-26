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
static std::deque<Entity> readEntities;
static std::deque<Entity> writeEntities;
static ComponentIndex<ValidComponents> validComponents;
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
			validComponents.RLock();
			transforms.RLock();
			renderables.RLock();
			Timer t2(timer2);
			auto &validRenderables = renderables.ReadValidIndexes();
			auto &validTransforms = transforms.ReadValidIndexes();
			auto &validIndexes = validRenderables.size() > validTransforms.size() ? validTransforms : validRenderables;
			auto &validComps = validComponents.ReadComponents();
			auto &renderableComponents = renderables.ReadComponents();
			auto &transformComponents = transforms.ReadComponents();
			for (auto i : validIndexes) {
				if (component_valid<Renderable, Transform>(validComps[i])) {
					auto &renderable = renderableComponents[i];
					auto &transform = transformComponents[i];
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
			validComponents.RUnlock();
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
			auto &validScripts = scripts.WriteValidIndexes();
			auto &scriptComponents = scripts.WriteComponents();
			{
				Timer t2(timer2);
				while (running) {
					size_t entIndex = scriptWorkerQueue++;
					if (entIndex < validScripts.size()) {
						auto &script = scriptComponents[validScripts[entIndex]];
						// "Run" script
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
			transforms.StartWrite();
			Timer t2(timer2);
			auto &validTransforms = transforms.WriteValidIndexes();
			auto &transformComponents = transforms.WriteComponents();
			for (auto i : validTransforms) {
				auto &transform = transformComponents[i];
				// for (auto &transform : transformComponents) {
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

int main(int argc, char **argv) {
	{
		Timer t("Alloc entities");
		readEntities.resize(ENTITY_COUNT);
		writeEntities.resize(ENTITY_COUNT);
	}

	{
		Timer t("Build links");
		validComponents.Init(readEntities, writeEntities);
		transforms.Init(readEntities, writeEntities);
		renderables.Init(readEntities, writeEntities);
		scripts.Init(readEntities, writeEntities);
	}

	{
		Timer t("Populate entities");
		auto &readTransform = transforms.ReadValidIndexes();
		auto &writeTransform = transforms.WriteValidIndexes();
		auto &readRenderable = renderables.ReadValidIndexes();
		auto &writeRenderable = renderables.WriteValidIndexes();
		auto &readScript = scripts.ReadValidIndexes();
		auto &writeScript = scripts.WriteValidIndexes();
		for (size_t i = 0; i < ENTITY_COUNT; i++) {
			readEntities[i] = Entity((ecs::eid_t)i);
			writeEntities[i] = Entity((ecs::eid_t)i);
			auto &readValid = validComponents.ReadComponents()[i];
			auto &writeValid = validComponents.WriteComponents()[i];
			if (i % TRANSFORM_DIVISOR == 0) {
				// readEntities[i].Set<Transform>(0.0, 0.0, 0.0, 1);
				// writeEntities[i].Set<Transform>(0.0, 0.0, 0.0, 1);
				transforms.ReadComponents()[i] = Transform(0.0, 0.0, 0.0, 1);
				transforms.WriteComponents()[i] = Transform(0.0, 0.0, 0.0, 1);
				set_component_valid<Transform>(readValid, true);
				set_component_valid<Transform>(writeValid, true);
				readTransform.emplace_back(i);
				writeTransform.emplace_back(i);
			}
			if (i % RENDERABLE_DIVISOR == 0) {
				// readEntities[i].Set<Renderable>("entity" + std::to_string(i));
				// writeEntities[i].Set<Renderable>("entity" + std::to_string(i));
				renderables.ReadComponents()[i] = Renderable("entity" + std::to_string(i));
				renderables.WriteComponents()[i] = Renderable("entity" + std::to_string(i));
				set_component_valid<Renderable>(readValid, true);
				set_component_valid<Renderable>(writeValid, true);
				readRenderable.emplace_back(i);
				writeRenderable.emplace_back(i);
			}
			if (i % SCRIPT_DIVISOR == 0) {
				// readEntities[i].Set<Script>(std::initializer_list<uint8_t>({0, 0, 0, 0, 0, 0, 0, 0}));
				// writeEntities[i].Set<Script>(std::initializer_list<uint8_t>({0, 0, 0, 0, 0, 0, 0, 0}));
				scripts.ReadComponents()[i] = {0, 0, 0, 0, 0, 0, 0, 0};
				scripts.WriteComponents()[i] = {0, 0, 0, 0, 0, 0, 0, 0};
				set_component_valid<Script>(readValid, true);
				set_component_valid<Script>(writeValid, true);
				readScript.emplace_back(i);
				writeScript.emplace_back(i);
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

	{
		Timer t("Validate entities");
		int invalid = 0;
		int valid = 0;
		double commonValue;
		auto &validTransforms = transforms.ReadComponents();
		auto &validIndexes = transforms.ReadValidIndexes();
		for (auto i : validIndexes) {
			auto transform = validTransforms[i];
			if (transform.pos[0] != transform.pos[1] || transform.pos[1] != transform.pos[2]) {
				if (invalid == 0) {
					std::cerr << "Component is not in correct place! " << transform.pos[0] << ", " << transform.pos[1]
							  << ", " << transform.pos[2] << std::endl;
				}
				invalid++;
			} else {
				if (valid == 0) {
					commonValue = transform.pos[0];
				} else if (transform.pos[0] != commonValue) {
					if (invalid == 0) {
						std::cerr << "Component is not in correct place! " << transform.pos[0] << ", "
								  << transform.pos[1] << ", " << transform.pos[2] << std::endl;
					}
					invalid++;
				}
				valid++;
			}
		}
		if (invalid != 0) { std::cerr << "Error: " << std::to_string(invalid) << " invalid components" << std::endl; }
		std::cout << validIndexes.size() << " total components (" << valid << " with value " << commonValue << ")"
				  << std::endl;
	}

	{
		Timer t("Remove entities");
		readEntities.clear();
		writeEntities.clear();
	}

	return 0;
}
