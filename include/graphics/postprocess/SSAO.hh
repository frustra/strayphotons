#pragma once

#include "PostProcess.hh"
#include "core/Logging.hh"

namespace sp
{
	class SSAO : public PostProcessPass<1, 1>
	{
	public:
		void Process()
		{
			Debugf("yo");
		}
	};
}