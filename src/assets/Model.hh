#pragma once

#include "Common.hh"
#include "graphics/Graphics.hh"
#include "graphics/Texture.hh"

#include <tiny_gltf_loader.h>

namespace sp
{
	class Asset;
	class GLModel;

	class Model : public NonCopyable
	{
		friend GLModel;
	public:
		Model(const string &name, shared_ptr<Asset> asset, shared_ptr<tinygltf::Scene> scene);
		~Model();

		struct Attribute
		{
			size_t byteOffset, byteStride;
			int componentType;
			size_t componentCount;
			size_t components;
			string bufferName;
		};

		struct Primitive
		{
			glm::mat4 matrix;
			int drawMode;
			Attribute indexBuffer;
			string materialName;
			Attribute attributes[3];
		};

		const string name;
		shared_ptr<GLModel> glModel;
		vector<Primitive *> primitives;

		vector<unsigned char> GetBuffer(string name);
	private:
		void AddNode(string nodeName, glm::mat4 parentMatrix);

		shared_ptr<tinygltf::Scene> scene;
		shared_ptr<Asset> asset;
	};

	class GLModel : public NonCopyable
	{
	public:
		GLModel(Model *model);
		~GLModel();

		struct Primitive
		{
			Model::Primitive *parent;
			GLuint vertexBufferHandle;
			GLuint indexBufferHandle;
			Texture *baseColorTex, *roughnessTex, *metallicTex, *heightTex;
		};

		void Draw();
	private:
		GLuint LoadBuffer(string name);
		Texture *LoadTexture(string materialName, string type);

		Model *model;
		std::map<string, GLuint> buffers;
		std::map<string, Texture> textures;
		vector<Primitive> primitives;
	};

	struct DefaultMaterial
	{
		Texture baseColorTex, roughnessTex, metallicTex, heightTex;

		DefaultMaterial()
		{
			unsigned char baseColor[4] = { 255, 255, 255, 255 };
			unsigned char roughness[4] = { 255, 255, 255, 255 };
			unsigned char metallic[4] = { 0, 0, 0, 0 };
			unsigned char bump[4] = { 127, 127, 127, 255 };

			baseColorTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_RGB8).Image2D(baseColor);

			roughnessTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_R8).Image2D(roughness);

			metallicTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_R8).Image2D(metallic);

			heightTex.Create()
			.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
			.Size(1, 1).Storage(PF_R8).Image2D(bump);
		}
	};
}
