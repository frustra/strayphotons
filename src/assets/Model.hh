#pragma once

#include "Common.hh"
#include "graphics/Graphics.hh"
#include "graphics/Texture.hh"

#include <tiny_gltf_loader.h>

namespace sp
{
	class Asset;

	class Model : public NonCopyable
	{
	public:
		Model(const string &name, shared_ptr<Asset> asset, tinygltf::Scene *scene);
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
			string baseColorName, roughnessName;
			Attribute attributes[3];

			GLuint vertexBufferHandle;
			GLuint indexBufferHandle;
			Texture *baseColorTex, *roughnessTex;
		};

		const string name;
		vector<Primitive *> primitives;
		std::map<string, GLuint> buffers;
		std::map<string, Texture> textures;

		GLuint LoadBuffer(string name);
		Texture *LoadTexture(string name);

		tinygltf::Scene *GetScene()
		{
			return scene;
		}
	private:
		void AddNode(string nodeName, glm::mat4 parentMatrix);

		tinygltf::Scene *scene;
		shared_ptr<Asset> asset;
	};
}
