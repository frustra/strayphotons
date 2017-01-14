#include "ConvexHull.hh"
#include "assets/Model.hh"
#include "core/Logging.hh"

#include <v-hacd/VHACD_Lib/public/VHACD.h>
#include <v-hacd/VHACD_Lib/inc/btConvexHullComputer.h>
#include <v-hacd/VHACD_Lib/inc/vhacdICHull.h>
#include <v-hacd/VHACD_Lib/inc/vhacdVector.h>

namespace sp
{
	class VHACDCallback : public VHACD::IVHACD::IUserCallback
	{
	public:
		VHACDCallback() {}
		~VHACDCallback() {}

		void Update(const double overallProgress, const double stageProgress, const double operationProgress,
					const char *const stage, const char *const operation)
		{
			Logf("VHACD %d (%s) %s", (int)(overallProgress + 0.5), stage, operation);
		};
	};

	void decomposeConvexHullsForPrimitive(ConvexHullSet *set, Model *model, Model::Primitive *prim)
	{
		auto posAttrib = prim->attributes[0];
		Assert(posAttrib.componentCount == 3);
		Assert(posAttrib.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

		auto indexAttrib = prim->indexBuffer;
		Assert(prim->drawMode == GL_TRIANGLES);
		Assert(indexAttrib.componentCount == 1);

		auto pbuffer = model->GetScene()->buffers[posAttrib.bufferName];
		auto points = (const float *)(pbuffer.data.data() + posAttrib.byteOffset);

		auto ibuffer = model->GetScene()->buffers[indexAttrib.bufferName];
		auto indices = (const int *)(ibuffer.data.data() + indexAttrib.byteOffset);
		int *indicesCopy = nullptr;

		switch (indexAttrib.componentType)
		{
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
				// indices is already compatible
				break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
				indicesCopy = new int[indexAttrib.components];

				for (uint32 i = 0; i < indexAttrib.components; i++)
				{
					indicesCopy[i] = (int)((const uint16 *) indices)[i];
				}

				indices = (const int *)indicesCopy;
				break;
			default:
				Assert(false, "invalid index component type");
				break;
		}

		static VHACDCallback vhacdCallback;

		auto interfaceVHACD = VHACD::CreateVHACD();
		int pointStride = posAttrib.byteStride / sizeof(float);

		VHACD::IVHACD::Parameters params;
		params.m_callback = &vhacdCallback;
		params.m_oclAcceleration = false;
		//params.m_resolution = 1000000;
		//params.m_convexhullDownsampling = 8;

		bool res = interfaceVHACD->Compute(points, pointStride, posAttrib.components, indices, 3, indexAttrib.components / 3, params);
		Assert(res, "building convex decomposition");

		for (uint32 i = 0; i < interfaceVHACD->GetNConvexHulls(); i++)
		{
			VHACD::IVHACD::ConvexHull ihull;
			interfaceVHACD->GetConvexHull(i, ihull);

			ConvexHull hull;
			hull.points = new float[ihull.m_nPoints * 3];
			hull.pointCount = ihull.m_nPoints;
			hull.pointByteStride = sizeof(float) * 3;

			for (int i = 0; i < ihull.m_nPoints * 3; i++)
			{
				hull.points[i] = (float) ihull.m_points[i];
			}

			hull.triangles = new int[ihull.m_nTriangles * 3];
			hull.triangleCount = ihull.m_nTriangles;
			hull.triangleByteStride = sizeof(int) * 3;
			memcpy(hull.triangles, ihull.m_triangles, sizeof(int) * 3 * ihull.m_nTriangles);

			set->hulls.push_back(hull);
			Logf("Adding hull, %d points, %d triangles", hull.pointCount, hull.triangleCount);
		}

		interfaceVHACD->Clean();
		interfaceVHACD->Release();

		if (indicesCopy)
			delete indicesCopy;
	}

	void copyVhacdManifoldMeshToConvexHull(ConvexHull &hull, VHACD::TMMesh &mesh)
	{
		const size_t nT = mesh.GetNTriangles();
		const size_t nV = mesh.GetNVertices();

		hull.points = new float[nV * 3];
		hull.pointCount = nV;
		hull.pointByteStride = sizeof(float) * 3;

		for (size_t v = 0; v < nV; v++)
		{
			auto &pos = mesh.GetVertices().GetData().m_pos;
			hull.points[v * 3 + 0] = pos[0];
			hull.points[v * 3 + 1] = pos[1];
			hull.points[v * 3 + 2] = pos[2];
			mesh.GetVertices().GetData().m_id = v;
			mesh.GetVertices().Next();
		}

		hull.triangles = new int[nT * 3];
		hull.triangleCount = nT;
		hull.triangleByteStride = sizeof(int) * 3;

		for (size_t f = 0; f < nT; f++)
		{
			auto &currentTriangle = mesh.GetTriangles().GetData();
			hull.triangles[f * 3 + 0] = static_cast<int>(currentTriangle.m_vertices[0]->GetData().m_id);
			hull.triangles[f * 3 + 1] = static_cast<int>(currentTriangle.m_vertices[1]->GetData().m_id);
			hull.triangles[f * 3 + 2] = static_cast<int>(currentTriangle.m_vertices[2]->GetData().m_id);
			mesh.GetTriangles().Next();
		}
	}

	void buildConvexHullForPrimitive(ConvexHullSet *set, Model *model, Model::Primitive *prim)
	{
		auto posAttrib = prim->attributes[0];
		Assert(posAttrib.componentCount == 3);
		Assert(posAttrib.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

		auto pbuffer = model->GetScene()->buffers[posAttrib.bufferName];
		auto points = (float *)(pbuffer.data.data() + posAttrib.byteOffset);

		btConvexHullComputer chc;
		chc.compute(points, posAttrib.byteStride, posAttrib.components, -1, -1);

		VHACD::ICHull icc;

		for (int i = 0; i < chc.vertices.size(); i++)
		{
			auto &v = chc.vertices[i];
			VHACD::Vec3<double> point(v[0], v[1], v[2]);
			icc.AddPoint(point);
		}

		icc.Process(128, 0);

		ConvexHull hull;
		copyVhacdManifoldMeshToConvexHull(hull, icc.GetMesh());
		set->hulls.push_back(hull);
		Logf("Adding hull, %d points, %d triangles", hull.pointCount, hull.triangleCount);
	}

	void ConvexHullBuilding::BuildConvexHulls(ConvexHullSet *set, Model *model)
	{
		for (auto prim : model->primitives)
		{
			if (prim->attributes[0].components < 255)
			{
				// Use points for a single hull without decomposing.
				buildConvexHullForPrimitive(set, model, prim);
			}
			else
			{
				// Break primitive into one or more convex hulls.
				decomposeConvexHullsForPrimitive(set, model, prim);
			}
		}
	}
}
