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

	size_t byteStrideForAccessor(int componentType, size_t componentCount, size_t existingByteStride)
	{
		if (existingByteStride)
			return existingByteStride;

		size_t componentWidth = 0;

		switch (componentType)
		{
			case TINYGLTF_COMPONENT_TYPE_BYTE:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
				componentWidth = 1;
				break;
			case TINYGLTF_COMPONENT_TYPE_SHORT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				componentWidth = 2;
				break;
			case TINYGLTF_COMPONENT_TYPE_INT:
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
			case TINYGLTF_COMPONENT_TYPE_FLOAT:
				componentWidth = 4;
				break;
			case TINYGLTF_COMPONENT_TYPE_DOUBLE:
				componentWidth = 8;
				break;
			default:
				Assert(false, "invalid component type");
				break;
		}

		return componentCount * componentWidth;
	}

	Model::Attribute GetPrimitiveAttribute(shared_ptr<tinygltf::Scene> scene, tinygltf::Primitive *p, string attribute)
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
			byteStrideForAccessor(accessor.componentType, componentCount, accessor.byteStride),
			accessor.componentType,
			componentCount,
			accessor.count,
			bufView.buffer
		};
	}

	Model::Model(const std::string &name, shared_ptr<Asset> asset, shared_ptr<tinygltf::Scene> scene) : name(name), scene(scene), asset(asset)
	{
		for (auto node : scene->scenes[scene->defaultScene])
		{
			AddNode(node, glm::mat4());
		}
	}

	Model::~Model()
	{
		Debugf("Destroying model %s (prepared: %d)", name, !!glModel);
		for (auto primitive : primitives)
		{
			delete primitive;
		}
		asset->manager->UnregisterModel(*this);
	}

	vector<unsigned char> Model::GetBuffer(string name)
	{
		return scene->buffers[name].data;
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

				Assert(iAcc.type == TINYGLTF_TYPE_SCALAR, "index buffer type must be scalar");

				primitives.push_back(new Primitive
				{
					matrix,
					mode,
					Attribute{
						iAcc.byteOffset + iBufView.byteOffset,
						byteStrideForAccessor(iAcc.componentType, 1, iAcc.byteStride),
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
					}
				});
			}
		}

		for (auto child : scene->nodes[nodeName].children)
		{
			AddNode(child, matrix);
		}
	}

	GLModel::GLModel(Model *model) : model(model)
	{
		static BasicMaterial defaultMat;

		for (auto primitive : model->primitives)
		{
			Primitive glPrimitive;
			glPrimitive.parent = primitive;
			glPrimitive.indexBufferHandle = LoadBuffer(primitive->indexBuffer.bufferName);

			glPrimitive.baseColorTex = LoadTexture(primitive->materialName, "baseColor");
			glPrimitive.roughnessTex = LoadTexture(primitive->materialName, "roughness");
			glPrimitive.metallicTex = LoadTexture(primitive->materialName, "metallic");
			glPrimitive.heightTex = LoadTexture(primitive->materialName, "height");

			if (!glPrimitive.baseColorTex) glPrimitive.baseColorTex = &defaultMat.baseColorTex;
			if (!glPrimitive.roughnessTex) glPrimitive.roughnessTex = &defaultMat.roughnessTex;
			if (!glPrimitive.metallicTex) glPrimitive.metallicTex = &defaultMat.metallicTex;
			if (!glPrimitive.heightTex) glPrimitive.heightTex = &defaultMat.heightTex;

			glCreateVertexArrays(1, &glPrimitive.vertexBufferHandle);
			for (int i = 0; i < 3; i++)
			{
				auto *attr = &primitive->attributes[i];
				if (attr->componentCount == 0) continue;
				glEnableVertexArrayAttrib(glPrimitive.vertexBufferHandle, i);
				glVertexArrayAttribFormat(glPrimitive.vertexBufferHandle, i, attr->componentCount, attr->componentType, GL_FALSE, 0);
				glVertexArrayVertexBuffer(glPrimitive.vertexBufferHandle, i, LoadBuffer(attr->bufferName), attr->byteOffset, attr->byteStride);
			}

			primitives.push_back(glPrimitive);
		}
	}

	GLModel::~GLModel()
	{
		for (auto primitive : primitives)
		{
			glDeleteVertexArrays(1, &primitive.vertexBufferHandle);
		}
		for (auto buf : buffers)
		{
			glDeleteBuffers(1, &buf.second);
		}
		for (auto tex : textures)
		{
			tex.second.Delete();
		}
	}

	void GLModel::Draw()
	{
		for (auto primitive : primitives)
		{
			glBindVertexArray(primitive.vertexBufferHandle);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive.indexBufferHandle);

			if (primitive.baseColorTex)
				primitive.baseColorTex->Bind(0);

			if (primitive.roughnessTex)
				primitive.roughnessTex->Bind(1);

			if (primitive.metallicTex)
				primitive.metallicTex->Bind(2);

			if (primitive.heightTex)
				primitive.heightTex->Bind(3);

			glDrawElements(
				primitive.parent->drawMode,
				primitive.parent->indexBuffer.components,
				primitive.parent->indexBuffer.componentType,
				(char *) primitive.parent->indexBuffer.byteOffset
			);
		}
	}

	GLuint GLModel::LoadBuffer(string name)
	{
		if (buffers.count(name)) return buffers[name];

		auto buffer = model->scene->buffers[name];
		GLuint handle;
		glCreateBuffers(1, &handle);
		glNamedBufferData(handle, buffer.data.size(), buffer.data.data(), GL_STATIC_DRAW);
		buffers[name] = handle;
		return handle;
	}

	Texture *GLModel::LoadTexture(string materialName, string type)
	{
		auto &material = model->scene->materials[materialName];

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

			auto texture = model->scene->textures[name];
			auto img = model->scene->images[texture.source];

			return &textures[name].Create(texture.target)
				   .Filter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR, 4.0)
				   .Wrap(GL_REPEAT, GL_REPEAT)
				   .Size(img.width, img.height)
				   .Storage(texture.internalFormat, texture.format, texture.type, Texture::FullyMipmap, true)
				   .Image2D(img.image.data());
		}
	}
}
