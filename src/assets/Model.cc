#include "assets/AssetManager.hh"
#include "assets/Model.hh"

#include <iostream>
#include <fstream>

namespace sp
{
	Model::Model(const std::string &name, shared_ptr<Asset> asset) : name(name), asset(asset)
	{
	}

	Model::~Model()
	{
	}
}
