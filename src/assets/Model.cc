#include "assets/AssetManager.hh"
#include "assets/Model.hh"

#include <iostream>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>

namespace sp
{
	Model::Model(const std::string &name, shared_ptr<Asset> asset) : name(name), asset(asset)
	{
	}

	Model::~Model()
	{
	}

	Model::node_iterator Model::list_nodes()
	{
		return Model::node_iterator(&scene);
	}

	glm::mat4 get_node_matrix(tinygltf::Node *node)
	{
		float arr[node->matrix.size()];
		std::copy(node->matrix.begin(), node->matrix.end(), arr);
		return glm::make_mat4(arr);
	}

	Model::node_iterator::node_iterator(tinygltf::Scene *s) : scene(s)
	{
		for (auto node_name : scene->scenes[scene->defaultScene])
		{
			tinygltf::Node *node = &scene->nodes[node_name];
			nodes.push_back(Node{get_node_matrix(node), node});
		}
		n_iter = nodes.begin();
	}

	Model::node_iterator Model::node_iterator::begin()
	{
		return node_iterator(this, nodes.begin());
	}

	Model::node_iterator Model::node_iterator::end()
	{
		return node_iterator(this, nodes.end());
	}

	void Model::node_iterator::increment()
	{
		if (!n_iter->node->children.empty())
		{
			for (auto node_name : n_iter->node->children)
			{
				tinygltf::Node *node = &scene->nodes[node_name];
				nodes.push_back(Node{n_iter->matrix * get_node_matrix(node), node});
			}
		}
		n_iter++;
	}
}
