#pragma once

#include "assets/Model.hh"

namespace sp
{
	namespace xr
	{
		// TODO: reconsider this class. It seems unnecessary.
		class XrModel : public Model
		{
		public:
			XrModel(const string &name) : Model(name) {};
			XrModel(const string &name, shared_ptr<tinygltf::Model> model) : Model(name, model) {};
		};

	}
}