#pragma once

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

namespace benchmark {
    class MultiTimer {
    public:
        MultiTimer(const MultiTimer &) = delete;

        MultiTimer(std::string name, bool print = true) : name(name), print(print) {
            if (print) { std::cout << "[" << name << "] Start" << std::endl; }
        }

        void AddValue(std::chrono::nanoseconds value) {
            values.emplace_back(value);
        }

        ~MultiTimer() {
            if (print) {
                if (values.size() > 1) {
                    std::chrono::nanoseconds total(0);
                    for (auto value : values) {
                        total += value;
                    }
                    std::sort(values.begin(), values.end(), std::less<std::chrono::nanoseconds>());

                    std::cout << "[" << name << "] Min: " << (values[0].count() / 1000.0)
                              << " usec, Avg: " << (total.count() / values.size() / 1000.0)
                              << " usec, P95: " << (values[(size_t)((double)values.size() * 0.95) - 1].count() / 1000.0)
                              << " usec, P99: " << (values[(size_t)((double)values.size() * 0.99) - 1].count() / 1000.0)
                              << " usec" << std::endl;
                } else if (values.size() == 1) {
                    std::cout << "[" << name << "] End: " << (values[0].count() / 1000000.0) << " ms" << std::endl;
                } else {
                    std::cout << "[" << name << "] No timers completed" << std::endl;
                }
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
            std::cout << "[" << name << "] Start" << std::endl;
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
                std::cout << "[" << name << "] End: " << ((end - start).count() / 1000000.0) << " ms" << std::endl;
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
} // namespace benchmark
