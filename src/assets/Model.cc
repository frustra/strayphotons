#include "assets/AssetManager.hh"
#include "assets/Model.hh"
#include "core/Logging.hh"

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

	Model::node_iterator Model::ListNodes()
	{
		return Model::node_iterator(&scene);
	}

	Model::primitive_iterator Model::ListPrimitives(Node *node)
	{
		return Model::primitive_iterator(&scene, node);
	}

	glm::mat4 GetNodeMatrix(tinygltf::Node *node)
	{
		glm::mat4 out;
		std::copy(node->matrix.begin(), node->matrix.end(), glm::value_ptr(out));
		return out;
	}

	Model::Attribute GetPrimitiveAttribute(tinygltf::Scene *scene, tinygltf::Primitive *p, string attribute)
	{
		auto accessor = scene->accessors[p->attributes[attribute]];
		auto bufView = scene->bufferViews[accessor.bufferView];

		int componentCount = 1;
		if (accessor.type == TINYGLTF_TYPE_SCALAR)
		{
			componentCount = 1;
		}
		else if (accessor.type == TINYGLTF_TYPE_VEC2)
		{
			componentCount = 2;
		}
		else if (accessor.type == TINYGLTF_TYPE_VEC3)
		{
			componentCount = 3;
		}
		else if (accessor.type == TINYGLTF_TYPE_VEC4)
		{
			componentCount = 4;
		}

		return Model::Attribute
		{
			accessor.byteOffset + bufView.byteOffset,
			accessor.byteStride,
			accessor.componentType,
			componentCount,
			bufView.buffer
		};
	}

	Model::node_iterator::node_iterator(tinygltf::Scene *s) : scene(s)
	{
		for (auto node_name : scene->scenes[scene->defaultScene])
		{
			tinygltf::Node *node = &scene->nodes[node_name];
			nodes.push_back(Node{GetNodeMatrix(node), node});
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
				nodes.push_back(Node{n_iter->matrix * GetNodeMatrix(node), node});
			}
		}
		n_iter++;
	}

	Model::primitive_iterator::primitive_iterator(tinygltf::Scene *scene, Node *node) : scene(scene), node(node)
	{
		for (auto mesh : node->node->meshes)
		{
			for (auto primitive : scene->meshes[mesh].primitives)
			{
				auto iAcc = scene->accessors[primitive.indices];
				auto iBufView = scene->bufferViews[iAcc.bufferView];

				primitives.push_back(Primitive
				{
					*node,
					iAcc.count,
					iBufView.buffer,
					iAcc.byteOffset + iBufView.byteOffset,
					{
						GetPrimitiveAttribute(scene, &primitive, "POSITION"),
						GetPrimitiveAttribute(scene, &primitive, "NORMAL"),
						GetPrimitiveAttribute(scene, &primitive, "TEXCOORD_0")
					}
				});
			}
		}
		p_iter = primitives.begin();
	}

	Model::primitive_iterator Model::primitive_iterator::begin()
	{
		return primitive_iterator(this, primitives.begin());
	}

	Model::primitive_iterator Model::primitive_iterator::end()
	{
		return primitive_iterator(this, primitives.end());
	}
}
