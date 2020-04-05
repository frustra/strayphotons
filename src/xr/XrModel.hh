#pragma once

#include "assets/Model.hh"

namespace sp
{
	namespace xr
	{
		class XrModel : public Model
		{
		public:
			XrModel(const string &name) : Model(name) {};
		};

	}
}