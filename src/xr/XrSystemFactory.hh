#pragma once

#include "xr/XrSystem.hh"

#include <memory>
#include <list>

namespace sp
{
	namespace xr
	{

		class XrSystemFactory
		{
		public:
			XrSystemFactory();
			std::shared_ptr<XrSystem> GetBestXrSystem();

			// TODO: GetNumAvailableXrSystems() to check if any are installed

		private:
			std::list<std::shared_ptr<XrSystem>> compiledXrSystems;
		};

	}
}