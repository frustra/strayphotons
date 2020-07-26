#pragma once

#include "Common.hh"

namespace ecs {
	class SignalChange {
	public:
		SignalChange(float signal) : signal(signal) {}
		float signal;
	};
} // namespace ecs