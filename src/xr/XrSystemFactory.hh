#pragma once

#include "xr/XrSystem.hh"

#include <list>
#include <memory>

namespace sp {
	namespace xr {

		class XrSystemFactory {
		public:
			XrSystemFactory();

			// This function picks the "best" XR system based on the current execution environment.
			// In many cases, this returns a NULL pointer
			std::shared_ptr<XrSystem> GetBestXrSystem();

		private:
			std::list<std::shared_ptr<XrSystem>> compiledXrSystems;
		};

	} // namespace xr
} // namespace sp