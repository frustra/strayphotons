#pragma once

#include <glm/glm.hpp>
#include <iostream>

namespace sp {
	template<typename T>
	std::istream &operator>>(std::istream &is, glm::tvec2<T, glm::highp> &v) {
		return is >> v[0] >> v[1];
	}

	template<typename T>
	std::istream &operator>>(std::istream &is, glm::tvec3<T, glm::highp> &v) {
		return is >> v[0] >> v[1] >> v[2];
	}

	template<typename T>
	std::ostream &operator<<(std::ostream &os, glm::tvec2<T, glm::highp> &v) {
		return os << v[0] << " " << v[1];
	}

	template<typename T>
	std::ostream &operator<<(std::ostream &os, glm::tvec3<T, glm::highp> &v) {
		return os << v[0] << " " << v[1] << " " << v[2];
	}
} // namespace sp