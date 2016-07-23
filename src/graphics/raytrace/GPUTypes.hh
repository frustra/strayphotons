#pragma once

#include <glm/glm.hpp>
#include "Common.hh"

namespace sp
{
	namespace raytrace
	{
		const int MAX_LIGHTS = 32;
		const int MAX_MATERIALS = 32;
		const int MAX_MESHES = 64;

		struct GLLight
		{
			glm::vec3 colour;
			float padding1[1];
			glm::vec3 position;
			float padding2[1];
			glm::vec3 falloff;
			float padding3[1];
		};

		static_assert((sizeof(GLLight) == 4 * 3 * sizeof(float)), "GLLight size incorrect");

		struct GLMaterial
		{
			glm::vec2 baseColorRoughnessSize;
			glm::vec2 normalMetalnessSize;
			glm::vec3 f0;
			float padding0[1];
			int32 baseColorRoughnessIdx = -1;
			int32 normalMetalnessIdx = -1;
			float padding1[2];
		};

		static_assert((sizeof(GLMaterial) == 4 * 3 * sizeof(float)), "GLMaterial size incorrect");

		struct GLAABB
		{
			glm::vec3 bounds1;
			float padding1[1];
			glm::vec3 bounds2;
			float padding2[1];
		};

		struct GLMesh
		{
			int materialID = -1;
			int indexOffset;
			int indexCount;
			int bvhRoot;
			glm::mat4 trans, invtrans;
			GLAABB aabb;
		};

		static_assert((sizeof(GLMesh) == 4 * 11 * sizeof(float)), "GLMesh size incorrect");

		struct GLVertex
		{
			GLVertex(glm::vec3 &pos, glm::vec3 &normal, glm::vec2 &uv) : pos(pos), u(uv.x), normal(normal), v(uv.y) { }
			glm::vec3 pos;
			float u;
			glm::vec3 normal;
			float v;
		};

		static_assert((sizeof(GLVertex) == 4 * 2 * sizeof(float)), "GLVertex size incorrect");

		struct GLBVHNode
		{
			glm::vec3 aabb1;
			int left;
			glm::vec3 aabb2;
			int right;

			/*int faceOffset, faceCount;
			float padding1[2];*/
		};

		static_assert((sizeof(GLBVHNode) == 4 * 2 * sizeof(float)), "GLBVHNode size incorrect");

		struct GLMaterialDataBuffer
		{
			int nMaterials = 0;
			float padding2[3];
			GLMaterial materials[MAX_MATERIALS];
		};

		struct GLLightDataBuffer
		{
			int nLights = 0;
			float padding1[3];
			GLLight lights[MAX_LIGHTS];
		};

		struct GLSceneDataBuffer
		{
			int nMeshes = 0;
			float padding3[3];
			GLMesh meshes[MAX_MESHES];
		};
	}
}
