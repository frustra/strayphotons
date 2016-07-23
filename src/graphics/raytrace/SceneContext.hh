#pragma once

#include "GPUTypes.hh"
#include "graphics/Renderer.hh"
#include "ecs/components/Renderable.hh"
#include "core/Game.hh"

namespace sp
{
	namespace raytrace
	{
		struct MaterialInfo
		{
			int id = -1;
			tinygltf::Texture *baseColorTex = 0, *roughnessTex = 0, *metalnessTex = 0, *normalTex = 0;
			tinygltf::Image *baseColorImg = 0, *roughnessImg = 0, *metalnessImg = 0, *normalImg = 0;
			bool roughnessInverted = false;
			glm::vec3 f0 = glm::vec3(0.04);
			float roughness = 1.0, metalness = 0.0;
		};

		struct BVHAABB
		{
			glm::vec3 min, max;
		};

		struct BVHLeaf
		{
			BVHAABB aabb;
			glm::vec3 centroid, center;
			int faceOffset;
		};

		struct BVHNode
		{
			BVHAABB aabb;
			BVHNode *left = nullptr, *right = nullptr;
			int leafFaceOffset = -1;
			int leafFaceCount = 0;

			~BVHNode()
			{
				if (left) delete left;
				if (right) delete right;
			}
		};

		struct BVHBuildContext;

		class GPUSceneContext
		{
		public:
			vector<GLVertex> vtxData;
			vector<uint32> faceData;
			vector<GLBVHNode> bvhData;
			GLSceneDataBuffer sceneData;
			GLMaterialDataBuffer matData;

			std::unordered_map<string, MaterialInfo> materials;

			void AppendPrimitive(glm::mat4 tr, Model *model, Model::Primitive *primitive);
			void BuildBVH(GLMesh &mesh);

		private:
			BVHNode *SubdivideBVH(vector<BVHLeaf> &faces, BVHBuildContext &ctx, int depth = 1);
			bool BVHPartitionForAxis(int axis, BVHAABB aabb, int depth, vector<BVHLeaf> &faces, float *partitionCost, float *partition);
			int AccumulateBVH(BVHNode *node, int &nodeCount, int &leafCount, int &maxDepth, int depth = 1);
		};
	}
}
