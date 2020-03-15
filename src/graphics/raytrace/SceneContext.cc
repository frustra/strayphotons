#include "SceneContext.hh"
#include "core/Logging.hh"

namespace sp
{
	namespace raytrace
	{
		void GPUSceneContext::AppendPrimitive(glm::mat4 tr, Model *model, Model::Primitive *primitive)
		{
			Assert(sceneData.nMeshes < MAX_MESHES, "reached max meshes");
			GLMesh &mesh = sceneData.meshes[sceneData.nMeshes++];
			mesh.indexOffset = faceData.size();
			mesh.indexCount = primitive->indexBuffer.components;
			mesh.trans = tr;
			mesh.invtrans = glm::inverse(tr);

			auto mat = materials.find(model->name + std::to_string(primitive->materialIndex));

			if (mat != materials.end())
				mesh.materialID = mat->second.id;

			auto idxAttrib = primitive->indexBuffer;
			Assert(idxAttrib.componentCount == 1, "assertion failed");
			Assert(idxAttrib.components % 3 == 0, "assertion failed");

			auto idxBuf = model->GetBuffer(idxAttrib.bufferIndex);
			auto idxBufData = idxBuf.data() + idxAttrib.byteOffset;
			auto idxStride = idxAttrib.byteStride;

			switch (idxAttrib.componentType)
			{
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
					idxStride = sizeof(uint16);
					break;
				case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
					idxStride = sizeof(uint32);
					break;
				default:
					Assert(false, "assertion failed");
			}

			for (size_t i = 0; i < idxAttrib.components; i++)
			{
				auto addr = i * idxStride + idxBufData;
				uint32 idx = 0;

				switch (idxAttrib.componentType)
				{
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
						idx = *(uint16 *)addr;
						break;
					case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
						idx = *(uint32 *)addr;
						break;
					default:
						Assert(false, "assertion failed");
				}

				faceData.push_back(idx + vtxData.size());
			}

			auto posAttrib = primitive->attributes[0];
			Assert(posAttrib.componentCount == 3, "assertion failed");
			Assert(posAttrib.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "assertion failed");

			auto posBuf = model->GetBuffer(posAttrib.bufferIndex);
			auto posBufData = posBuf.data() + posAttrib.byteOffset;

			auto normAttrib = primitive->attributes[1];
			Assert(normAttrib.componentCount == 3, "assertion failed");
			Assert(normAttrib.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "assertion failed");

			auto normBuf = model->GetBuffer(normAttrib.bufferIndex);
			auto normBufData = normBuf.data() + normAttrib.byteOffset;

			Assert(normAttrib.components == posAttrib.components, "assertion failed");

			auto uvAttrib = primitive->attributes[2];
			uint8 *uvBufData = nullptr;

			if (uvAttrib.componentCount > 0)
			{
				Assert(uvAttrib.componentCount == 2, "assertion failed");
				Assert(uvAttrib.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "assertion failed");

				auto uvBuf = model->GetBuffer(uvAttrib.bufferIndex);
				uvBufData = uvBuf.data() + uvAttrib.byteOffset;

				Assert(uvAttrib.components == posAttrib.components, "assertion failed");
			}

			for (size_t i = 0; i < posAttrib.components; i++)
			{
				auto pos = *(glm::vec3 *) (i * posAttrib.byteStride + posBufData);
				auto normal = *(glm::vec3 *) (i * normAttrib.byteStride + normBufData);

				auto uv = glm::vec2(0.0f);
				if (uvBufData)
				{
					uv = *(glm::vec2 *) (i * uvAttrib.byteStride + uvBufData);
				}

				vtxData.push_back(GLVertex(pos, normal, uv));

				mesh.aabb.bounds1 = glm::min(mesh.aabb.bounds1, pos);
				mesh.aabb.bounds2 = glm::max(mesh.aabb.bounds2, pos);
			}

			BuildBVH(mesh);
		}
	}
}
