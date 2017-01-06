#include "assets/AssetManager.hh"
#include "assets/Asset.hh"
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
		if (!p->attributes.count(attribute)) return Model::Attribute();
		auto accessor = scene->accessors[p->attributes[attribute]];
		auto bufView = scene->bufferViews[accessor.bufferView];

		size_t componentCount = 1;
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
			accessor.byteStride ? accessor.byteStride : 4 * componentCount,
			accessor.componentType,
			componentCount,
			accessor.count,
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

	Model::~Model()
	{
		asset->manager->UnregisterModel(*this);
	}

	GLuint Model::LoadBuffer(string name)
	{
		if (buffers.count(name)) return buffers[name];

		auto buffer = scene->buffers[name];
		GLuint handle;
		glCreateBuffers(1, &handle);
		glNamedBufferData(handle, buffer.data.size(), buffer.data.data(), GL_STATIC_DRAW);
		buffers[name] = handle;
		return handle;
	}

	Texture *Model::LoadTexture(string materialName, string type)
	{
		auto &material = scene->materials[materialName];

		if (!material.values.count(type)) return NULL;

		auto value = material.values[type];
		if (value.string_value.empty())
		{
			char name[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
			unsigned char data[4];
			for (size_t i = 0; i < 4; i++)
			{
				data[i] = 255 * value.number_array.at(std::min(value.number_array.size() - 1, i));
				name[i * 2] = 'A' + ((data[i] & 0xF0) >> 4);
				name[i * 2 + 1] = 'A' + (data[i] & 0xF);
			}

			if (textures.count(name)) return &textures[name];

			return &textures[name].Create()
				   .Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
				   .Size(1, 1).Storage(PF_RGB8).Image2D(data);
		}
		else
		{
			auto name = value.string_value;
			if (textures.count(name)) return &textures[name];

			auto texture = scene->textures[name];
			auto img = scene->images[texture.source];

			return &textures[name].Create(texture.target)
				   .Filter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, 4.0)
				   .Wrap(GL_REPEAT, GL_REPEAT)
				   .Size(img.width, img.height)
				   .Storage(texture.internalFormat, texture.format, texture.type, Texture::FullyMipmap, true)
				   .Image2D(img.image.data());
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

				int mode = -1;
				if (primitive.mode == TINYGLTF_MODE_TRIANGLES)
				{
					mode = GL_TRIANGLES;
				}
				else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_STRIP)
				{
					mode = GL_TRIANGLE_STRIP;
				}
				else if (primitive.mode == TINYGLTF_MODE_TRIANGLE_FAN)
				{
					mode = GL_TRIANGLE_FAN;
				}
				else if (primitive.mode == TINYGLTF_MODE_POINTS)
				{
					mode = GL_POINTS;
				}
				else if (primitive.mode == TINYGLTF_MODE_LINE)
				{
					mode = GL_LINES;
				}
				else if (primitive.mode == TINYGLTF_MODE_LINE_LOOP)
				{
					mode = GL_LINE_LOOP;
				};

				Assert(iAcc.type == TINYGLTF_TYPE_SCALAR);

				primitives.push_back(new Primitive
				{
					matrix,
					mode,
					Attribute{
						iAcc.byteOffset + iBufView.byteOffset,
						iAcc.byteStride,
						iAcc.componentType,
						1,
						iAcc.count,
						iBufView.buffer
					},
					primitive.material,
					{
						GetPrimitiveAttribute(scene, &primitive, "POSITION"),
						GetPrimitiveAttribute(scene, &primitive, "NORMAL"),
						GetPrimitiveAttribute(scene, &primitive, "TEXCOORD_0")
					},
					0, 0, nullptr, nullptr, nullptr, nullptr
				});
			}
		}

		for (auto child : scene->nodes[nodeName].children)
		{
			AddNode(child, matrix);
		}
	}
}
