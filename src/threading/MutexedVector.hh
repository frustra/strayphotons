#pragma once

#include <mutex>
#include <thread>
#include <vector>

namespace sp {
	template<typename T>
	class MutexedVector {
	public:
		MutexedVector(std::vector<T> &vec, std::mutex &m) : vec(vec), lock(m) {}
		MutexedVector(MutexedVector &&vec) = default;
		MutexedVector(MutexedVector &vec) = delete;
		~MutexedVector() {}

		std::vector<T> &Vector() {
			return vec;
		}

	private:
		std::vector<T> &vec;
		std::unique_lock<std::mutex> lock;
	};
} // namespace sp
