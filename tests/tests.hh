#pragma once

#include "ecs/components/Events.hh"

#include <algorithm>
#include <chrono>
#include <functional>
#include <glm/gtx/string_cast.hpp>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

static inline std::ostream &operator<<(std::ostream &out, const glm::vec2 &v) {
    return out << glm::to_string(v);
}

static inline std::ostream &operator<<(std::ostream &out, const glm::vec3 &v) {
    return out << glm::to_string(v);
}

static inline std::ostream &operator<<(std::ostream &out, const ecs::Event::EventData &v) {
    std::visit(
        [&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, glm::vec2>) {
                out << "glm::" << glm::to_string(arg);
            } else if constexpr (std::is_same_v<T, Tecs::Entity>) {
                out << "Entity(" << arg.id << ")";
            } else if constexpr (std::is_same_v<T, std::string>) {
                out << "\"" << arg << "\"";
            } else {
                out << typeid(arg).name() << "(" << arg << ")";
            }
        },
        v);
    return out;
}

namespace testing {
    extern std::vector<std::function<void()>> registeredTests;

    class Test {
    public:
        Test(std::function<void()> testFunc) {
            registeredTests.emplace_back(testFunc);
        }
    };

    static inline void Assert(bool condition, const std::string message) {
        if (!condition) {
            std::stringstream ss;
            ss << "Assertion failed: " << message << std::endl;
            std::cerr << ss.str() << std::flush;
            throw std::runtime_error(message);
        }
    }

    template<typename Ta, typename Tb>
    inline void AssertEqual(Ta a, Tb b, const std::string message) {
        if (!(a == b)) {
            std::stringstream ss;
            ss << "Assertion failed: " << message << " (" << a << " != " << b << ")" << std::endl;
            std::cerr << ss.str() << std::flush;
            throw std::runtime_error(message);
        }
    }

    class MultiTimer {
    public:
        MultiTimer(const MultiTimer &) = delete;

        MultiTimer() : name(), print(false) {}
        MultiTimer(std::string name, bool print = true) {
            Reset(name, print);
        }

        void Reset(std::string name, bool print = true) {
            this->name = name;
            this->print = print;
            if (print) {
                std::stringstream ss;
                ss << "[" << name << "] Start" << std::endl;
                std::cout << ss.str();
            }
        }

        void AddValue(std::chrono::nanoseconds value) {
            values.emplace_back(value);
        }

        ~MultiTimer() {
            if (print) {
                std::stringstream ss;
                if (values.size() > 1) {
                    std::chrono::nanoseconds total(0);
                    for (auto value : values) {
                        total += value;
                    }
                    std::sort(values.begin(), values.end(), std::less<std::chrono::nanoseconds>());

                    ss << "[" << name << "] Min: " << (values[0].count() / 1000.0)
                       << " usec, Avg: " << (total.count() / values.size() / 1000.0)
                       << " usec, P95: " << (values[(size_t)((double)values.size() * 0.95) - 1].count() / 1000.0)
                       << " usec, P99: " << (values[(size_t)((double)values.size() * 0.99) - 1].count() / 1000.0)
                       << " usec, Total: " << (total.count() / 1000000.0) << " ms" << std::endl;
                } else if (values.size() == 1) {
                    ss << "[" << name << "] End: " << (values[0].count() / 1000000.0) << " ms" << std::endl;
                } else {
                    ss << "[" << name << "] No timers completed" << std::endl;
                }
                std::cout << ss.str();
            }
        }

    private:
        std::string name;
        bool print;
        std::vector<std::chrono::nanoseconds> values;
    };

    class Timer {
    public:
        Timer(const Timer &) = delete;

        Timer(std::string name) : name(name) {
            std::stringstream ss;
            ss << "[" << name << "] Start" << std::endl;
            std::cout << ss.str();
            start = std::chrono::high_resolution_clock::now();
        }
        Timer(MultiTimer &parent) : parent(&parent) {
            start = std::chrono::high_resolution_clock::now();
        }

        ~Timer() {
            auto end = std::chrono::high_resolution_clock::now();
            if (parent != nullptr) {
                parent->AddValue(end - start);
            } else if (!name.empty()) {
                std::stringstream ss;
                ss << "[" << name << "] End: " << ((end - start).count() / 1000000.0) << " ms" << std::endl;
                std::cout << ss.str();
            }
        }

        Timer &operator=(MultiTimer &newParent) {
            this->~Timer();
            this->name = "";
            this->parent = &newParent;
            start = std::chrono::high_resolution_clock::now();

            return *this;
        }

    private:
        std::string name;
        std::chrono::high_resolution_clock::time_point start;
        MultiTimer *parent = nullptr;
    };
} // namespace testing
