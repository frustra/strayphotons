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

	GLuint Model::LoadTexture(string name)
	{
		if (textures.count(name)) return textures[name];

		auto texture = scene->textures[name];
		GLuint handle;
		glCreateTextures(GL_TEXTURE_2D, 1, &handle);
		auto img = scene->images[texture.source];
		glTextureStorage2D(handle, 1, GL_RGBA8, img.width, img.height);
		glTextureSubImage2D(handle, 0, 0, 0, img.width, img.height, GL_RGB, texture.type, img.image.data());
		textures[name] = handle;
		return handle;
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

				primitives.push_back(new Primitive
				{
					matrix,
					mode,
					Attribute{
						iAcc.byteOffset + iBufView.byteOffset,
						iAcc.byteStride,
						iAcc.componentType,
						iAcc.count,
						iBufView.buffer
					},
					scene->materials[primitive.material].values["diffuse"].string_value,
					{
						GetPrimitiveAttribute(scene, &primitive, "POSITION"),
						GetPrimitiveAttribute(scene, &primitive, "NORMAL"),
						GetPrimitiveAttribute(scene, &primitive, "TEXCOORD_0")
					},
					0, 0, 0
				});
			}
		}

		for (auto child : scene->nodes[nodeName].children)
		{
			AddNode(child, matrix);
		}
	}
}
