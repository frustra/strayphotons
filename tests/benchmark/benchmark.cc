#include "Timer.hh"
#include "test_components.hh"

#include <atomic>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <thread>

using namespace benchmark;

std::atomic_bool running;
static ecs::EntityManager entityManager;

#define ENTITY_COUNT 1000000
#define ADD_REMOVE_COUNT 100000
#define THREAD_COUNT 0

#define TRANSFORM_DIVISOR 2
#define RENDERABLE_DIVISOR 3
#define SCRIPT_DIVISOR 10

void workerThread() {
    std::vector<std::string> bad;
    double currentValue = 0;
    size_t readCount = 0;
    size_t badCount = 0;
    auto start = std::chrono::high_resolution_clock::now();
    MultiTimer renderTimer("RenderThread Run");
    MultiTimer transformTimer("TransformWorkerThread Run");
    while (running) {
        {
            Timer t(transformTimer);
            for (auto e : entityManager.EntitiesWith<Transform>()) {
                auto transform = e.Get<Transform>();
                transform->pos[0]++;
                transform->pos[1]++;
                transform->pos[2]++;
            }
        }
        {
            Timer t(renderTimer);
            auto validEntities = entityManager.EntitiesWith<Renderable, Transform>();
            auto firstName = &(*validEntities.begin()).Get<Renderable>()->name;
            for (auto e : validEntities) {
                auto renderable = e.Get<Renderable>();
                auto transform = e.Get<Transform>();
                if (transform->pos[0] != transform->pos[1] || transform->pos[1] != transform->pos[2]) {
                    bad.emplace_back(renderable->name);
                } else {
                    if (&renderable->name == firstName) {
                        currentValue = transform->pos[0];
                    } else if (transform->pos[0] != currentValue) {
                        bad.emplace_back(renderable->name);
                    }
                }
            }
        }
        readCount++;
        badCount += bad.size();
        bad.clear();
    }

    auto delta = std::chrono::high_resolution_clock::now() - start;
    double durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(delta).count();
    double avgFrameRate = readCount * 1000 / (double)durationMs;
    double avgUpdateRate = currentValue * 1000 / (double)durationMs;
    if (badCount != 0) {
        std::cerr << "[RenderThread Error] Detected " << badCount << " invalid entities during reading." << std::endl;
    }
    std::cout << "[RenderThread] Average frame rate: " << avgFrameRate << "Hz" << std::endl;
    std::cout << "[TransformWorkerThread] Average update rate: " << avgUpdateRate << "Hz" << std::endl;
}

int main(int argc, char **argv) {
    {
        Timer t("Register component types");
        entityManager.RegisterComponentType<Transform>();
        entityManager.RegisterComponentType<Renderable>();
        entityManager.RegisterComponentType<Script>();
    }

    size_t entityCount = 0;
    {
        Timer t("Create entities");
        for (size_t i = 0; i < ENTITY_COUNT; i++) {
            ecs::Entity e = entityManager.NewEntity();
            entityCount++;
            if (i % TRANSFORM_DIVISOR == 0) { e.Assign<Transform>(0.0, 0.0, 0.0); }
            if (i % RENDERABLE_DIVISOR == 0) { e.Assign<Renderable>("entity" + std::to_string(i)); }
            if (i % SCRIPT_DIVISOR == 0) { e.Assign<Script>(std::initializer_list<uint8_t>({0, 0, 0, 0, 0, 0, 0, 0})); }
        }
    }

    struct RemovedEntity {
        std::string name;
        std::bitset<3> components;
    };
    std::vector<RemovedEntity> removedList;
    {
        Timer t("Remove the first " + std::to_string(ADD_REMOVE_COUNT) + " entities");
        auto entities = entityManager.EntitiesWith<Script>();
        size_t count = 0;
        for (auto itr = entities.begin(); itr != entities.end() && count < ADD_REMOVE_COUNT; ++itr, count++) {
            ecs::Entity e = *itr;

            auto &removedEntity = removedList.emplace_back();
            removedEntity.components[0] = e.Has<Transform>();
            if (e.Has<Renderable>()) {
                removedEntity.name = e.Get<Renderable>()->name;
                removedEntity.components[1] = true;
            }
            removedEntity.components[2] = e.Has<Script>();
            e.Destroy();
            entityCount--;
        }
    }
    {
        Timer t("Recreate removed entities");
        for (auto removedEntity : removedList) {
            ecs::Entity e = entityManager.NewEntity();
            entityCount++;
            if (removedEntity.components[0]) { e.Assign<Transform>(0.0, 0.0, 0.0); }
            if (removedEntity.components[1]) { e.Assign<Renderable>(removedEntity.name); }
            if (removedEntity.components[2]) {
                e.Assign<Script>(std::initializer_list<uint8_t>({0, 0, 0, 0, 0, 0, 0, 0}));
            }
        }
    }

    std::cout << "Running with " << entityCount << " Entities and "
              << entityManager.CreateComponentMask<Transform>().size() << " Component types" << std::endl;

    {
        Timer t("Run thread");
        std::thread thread(workerThread);
        running = true;
        std::this_thread::sleep_for(std::chrono::seconds(10));
        running = false;
        thread.join();
    }

    {
        Timer t("Validate entities");
        int invalid = 0;
        int valid = 0;
        double commonValue;
        auto entityList = entityManager.EntitiesWith<Transform>();
        size_t entityCount = 0;
        for (auto e : entityList) {
            entityCount++;

            auto transform = e.Get<Transform>();

            if (transform->pos[0] != transform->pos[1] || transform->pos[1] != transform->pos[2]) {
                if (invalid == 0) {
                    std::cerr << "Component is not in correct place! " << transform->pos[0] << ", " << transform->pos[1]
                              << ", " << transform->pos[2] << std::endl;
                }
                invalid++;
            } else {
                if (valid == 0) {
                    commonValue = transform->pos[0];
                } else if (transform->pos[0] != commonValue) {
                    if (invalid == 0) {
                        std::cerr << "Component is not in correct place! " << transform->pos[0] << ", "
                                  << transform->pos[1] << ", " << transform->pos[2] << std::endl;
                    }
                    invalid++;
                }
                valid++;
            }
        }
        if (invalid != 0) { std::cerr << "Error: " << std::to_string(invalid) << " invalid components" << std::endl; }
        std::cout << entityCount << " total components (" << valid << " with value " << commonValue << ")" << std::endl;
    }

    return 0;
}
