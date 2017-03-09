#include "SceneContext.hh"
#include "core/Logging.hh"
#include <thread>
#include <mutex>

namespace sp
{
	namespace raytrace
	{
		struct BVHBuildContext
		{
			GLMesh *mesh;
			vector<uint32> newFaceIndexes;
			std::mutex nfiMutex;
			int activeThreads = 1;
		};

		void GPUSceneContext::BuildBVH(GLMesh &mesh)
		{
			vector<BVHLeaf> faces;

			auto start = glfwGetTime();

			for (int i = mesh.indexOffset; i < mesh.indexCount + mesh.indexOffset; i += 3)
			{
				glm::vec3 v0 = vtxData[faceData[i + 0]].pos;
				glm::vec3 v1 = vtxData[faceData[i + 1]].pos;
				glm::vec3 v2 = vtxData[faceData[i + 2]].pos;

				BVHLeaf face;
				face.aabb.min = v0;
				face.aabb.min = glm::min(face.aabb.min, v1);
				face.aabb.min = glm::min(face.aabb.min, v2);
				face.aabb.max = v0;
				face.aabb.max = glm::max(face.aabb.max, v1);
				face.aabb.max = glm::max(face.aabb.max, v2);
				face.faceOffset = i;
				face.centroid = (v0 + v1 + v2) / 3.0f;
				face.center = (face.aabb.min + face.aabb.max) / 2.0f;
				faces.push_back(face);
			}

			// Generate BVH
			BVHBuildContext bvhctx;
			bvhctx.newFaceIndexes.reserve(mesh.indexCount);
			bvhctx.mesh = &mesh;

			Logf("BVH build started / tris: %d", faces.size());
			BVHNode *root = SubdivideBVH(faces, bvhctx);

			// Copy face indexes in BVH order
			Assert(bvhctx.newFaceIndexes.size() == (size_t) mesh.indexCount, "assertion failed");

			for (size_t i = 0; i < bvhctx.newFaceIndexes.size(); i++)
				faceData[mesh.indexOffset + i] = bvhctx.newFaceIndexes[i];

			// Collect BVH nodes
			int maxDepth = 0, nodeCount = 0, leafCount = 0;
			mesh.bvhRoot = AccumulateBVH(root, nodeCount, leafCount, maxDepth);

			auto end = glfwGetTime();
			Logf("BVH built / seconds: %f depth: %d nodes: %d leaves: %d tris: %d", end - start, maxDepth, nodeCount, leafCount, faces.size());

			delete root;
		}

		BVHNode *GPUSceneContext::SubdivideBVH(vector<BVHLeaf> &faces, BVHBuildContext &ctx, int depth)
		{
			BVHNode *node = new BVHNode;

			// Generate AABB for entire input
			BVHAABB aabb = faces[0].aabb;
			for (auto &f : faces)
			{
				aabb.min = glm::min(aabb.min, f.aabb.min);
				aabb.max = glm::max(aabb.max, f.aabb.max);
			}

			node->aabb = aabb;

			// Base leaf node
			if (faces.size() <= 4)
			{
returnLeafNode:
				int size = faces.size() * 3;

				ctx.nfiMutex.lock();
				int offset = ctx.newFaceIndexes.size();
				ctx.newFaceIndexes.resize(offset + size);
				ctx.nfiMutex.unlock();

				node->leafFaceOffset = offset + ctx.mesh->indexOffset;
				node->leafFaceCount = size;

				// Copy face indexes to secondary buffer in BVH order
				int index = offset;
				for (auto &f : faces)
				{
					for (int i = 0; i < 3; i++)
					{
						ctx.newFaceIndexes[index++] = faceData[f.faceOffset + i];
					}
				}
				return node;
			}

			// Determine best partition plane by applying a surface area heuristic
			glm::vec3 aabbSize = aabb.max - aabb.min;
			float bestCost = faces.size() * (aabbSize.x * aabbSize.y + aabbSize.y * aabbSize.z + aabbSize.z * aabbSize.x);
			float bestPartition = FLT_MAX;
			int bestAxis = -1;

			float costs[3] = { -1.0f }, partitions[3];

			if (depth < 3 && faces.size() > 128)
			{
				std::thread children[3];

				for (int axis = 0; axis < 3; axis++)
				{
					children[axis] = std::thread([ &, axis]
					{
						BVHPartitionForAxis(axis, aabb, depth, faces, &costs[axis], &partitions[axis]);
					});
				}

				for (int axis = 0; axis < 3; axis++)
				{
					children[axis].join();
				}
			}
			else
			{
				for (int axis = 0; axis < 3; axis++)
				{
					BVHPartitionForAxis(axis, aabb, depth, faces, &costs[axis], &partitions[axis]);
				}
			}

			for (int axis = 0; axis < 3; axis++)
			{
				if (costs[axis] < bestCost)
				{
					bestCost = costs[axis];
					bestPartition = partitions[axis];
					bestAxis = axis;
				}
			}

			if (bestAxis < 0)
			{
				goto returnLeafNode;
			}

			vector<BVHLeaf> leftFaces, rightFaces;

			// Perform final partitioning
			for (auto &f : faces)
			{
				if (f.center[bestAxis] <= bestPartition)
					leftFaces.push_back(f);
				else
					rightFaces.push_back(f);
			}

			//Logf("%d %d, %d, %f", leftFaces.size(), rightFaces.size(), bestAxis, bestPartition);
			Assert(leftFaces.size() > 0 && rightFaces.size() > 0, "assertion failed");

			if (ctx.activeThreads >= 4 || faces.size() < 64)
			{
				node->left = SubdivideBVH(leftFaces, ctx, depth + 1);
				node->right = SubdivideBVH(rightFaces, ctx, depth + 1);
				return node;
			}

			ctx.activeThreads++;

			std::thread child([&]
			{
				node->left = SubdivideBVH(leftFaces, ctx, depth + 1);
				ctx.activeThreads--;
			});

			node->right = SubdivideBVH(rightFaces, ctx, depth + 1);

			child.join();
			return node;
		}

		bool GPUSceneContext::BVHPartitionForAxis(int axis, BVHAABB aabb, int depth, vector<BVHLeaf> &faces, float *bestCost, float *bestPartition)
		{
			float axisMin = aabb.min[axis];
			float axisMax = aabb.max[axis];
			*bestCost = FLT_MAX;

			if ((axisMax - axisMin) < 0.0001)
			{
				return false;
			}

			float axisStep = (axisMax - axisMin) * (float)(depth) / 1024.0f;
			axisStep = glm::max(axisStep, 0.0001f);
			//Logf("%f", axisStep);

			for (float partition = axisMin + axisStep; partition <= axisMax - axisStep; partition += axisStep)
			{
				BVHAABB leftBox, rightBox;
				int leftCount = 0, rightCount = 0;

				leftBox.min = rightBox.min = glm::vec3(FLT_MAX);
				leftBox.max = rightBox.max = glm::vec3(FLT_MIN);

				// Partition faces based on split point
				for (auto &f : faces)
				{
					if (f.center[axis] <= partition)
					{
						leftBox.min = glm::min(leftBox.min, f.aabb.min);
						leftBox.max = glm::max(leftBox.max, f.aabb.max);
						leftCount++;
					}
					else
					{
						rightBox.min = glm::min(rightBox.min, f.aabb.min);
						rightBox.max = glm::max(rightBox.max, f.aabb.max);
						rightCount++;
					}
				}

				if (leftCount == 0 || rightCount == 0)
				{
					// Redundant split
					continue;
				}

				// Test surface area heuristic
				glm::vec3 leftSize = leftBox.max - leftBox.min;
				float leftHalfArea = leftSize.x * leftSize.y + leftSize.y * leftSize.z + leftSize.z * leftSize.x;

				glm::vec3 rightSize = rightBox.max - rightBox.min;
				float rightHalfArea = rightSize.x * rightSize.y + rightSize.y * rightSize.z + rightSize.z * rightSize.x;

				float cost = leftHalfArea * leftCount + rightHalfArea * rightCount;

				if (cost < *bestCost)
				{
					*bestCost = cost;
					*bestPartition = partition;
				}
			}

			return true;
		}

		int GPUSceneContext::AccumulateBVH(BVHNode *node, int &nodeCount, int &leafCount, int &maxDepth, int depth)
		{
			GLBVHNode glnode;
			glnode.aabb1 = node->aabb.min;
			glnode.aabb2 = node->aabb.max;

			maxDepth = glm::max(maxDepth, depth);

			if (node->leafFaceOffset != -1)
			{
				glnode.left = -node->leafFaceCount;
				glnode.right = node->leafFaceOffset;
				leafCount++;
				//Logf("leaf has %d faces", node->leafFaceCount);
				//Logf("%f/%f", glnode.aabb1.x, glnode.aabb2.x);
				//Logf("%f/%f", glnode.aabb1.y, glnode.aabb2.y);
				//Logf("%f/%f", glnode.aabb1.z, glnode.aabb2.z);
			}
			else
			{
				glnode.left = AccumulateBVH(node->left, nodeCount, leafCount, maxDepth, depth + 1);
				glnode.right = AccumulateBVH(node->right, nodeCount, leafCount, maxDepth, depth + 1);
			}

			nodeCount++;
			bvhData.push_back(glnode);
			return bvhData.size() - 1;
		}
	}
}
