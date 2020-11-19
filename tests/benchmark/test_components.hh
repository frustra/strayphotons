#pragma once

#include <Ecs.hh>
#include <cstdint>
#include <string>
#include <vector>

namespace benchmark {
	struct Transform {
		double pos[3] = {0};
		ecs::Entity parent;

		Transform() {}
		Transform(double x, double y, double z) : parent() {
			pos[0] = x;
			pos[1] = y;
			pos[2] = z;
		}
		Transform(double x, double y, double z, ecs::Entity parent) : parent(parent) {
			pos[0] = x;
			pos[1] = y;
			pos[2] = z;
		}

		bool HasParent() {
			return parent.Valid();
		}
	};

	struct Script {
		std::vector<uint8_t> data;

		Script() {}
		Script(uint8_t *data, size_t size) : data(data, data + size) {}
		Script(std::initializer_list<uint8_t> init) : data(init) {}
	};

	struct Renderable {
		std::string name;

		Renderable() {}
		Renderable(std::string name) : name(name) {}
	};
}; // namespace benchmark
