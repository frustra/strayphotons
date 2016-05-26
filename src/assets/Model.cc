#include "assets/AssetManager.hh"
#include "assets/Model.hh"
#include "core/Logging.hh"

#include <iostream>
#include <fstream>
#include <glm/gtc/type_ptr.hpp>

namespace sp
{
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

	Model::Model(const std::string &name, shared_ptr<Asset> asset, tinygltf::Scene *scene) : name(name), scene(scene), asset(asset)
	{
		for (auto node : scene->scenes[scene->defaultScene])
		{
			AddNode(node, glm::mat4());
		}
	}

	void Model::AddNode(string nodeName, glm::mat4 parentMatrix)
	{
		glm::mat4 matrix = parentMatrix * GetNodeMatrix(&scene->nodes[nodeName]);

		for (auto mesh : scene->nodes[nodeName].meshes)
		{
			for (auto primitive : scene->meshes[mesh].primitives)
			{
				auto iAcc = scene->accessors[primitive.indices];
				auto iBufView = scene->bufferViews[iAcc.bufferView];

				primitives.push_back(Primitive
				{
					matrix,
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

		for (auto child : scene->nodes[nodeName].children)
		{
			AddNode(child, matrix);
		}
	}
}
