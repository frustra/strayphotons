#pragma once

#include <chrono>
#include <string>

namespace benchmark {
	class Timer {
	public:
		Timer(std::string name) : name(name) {
			std::cout << "[" << name << "] Start" << std::endl;
			start = std::chrono::high_resolution_clock::now();
		}
		~Timer() {
			auto end = std::chrono::high_resolution_clock::now();
			std::cout << "[" << name << "] End: "
					  << (std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0) << " ms"
					  << std::endl;
		}

	private:
		std::string name;
		std::chrono::high_resolution_clock::time_point start;
	};
} // namespace benchmark
