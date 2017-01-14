#pragma once

#include "Common.hh"

namespace sp
{
	class Model;

	struct ConvexHull
	{
		uint32 pointCount;
		uint32 pointByteStride;
		uint32 triangleCount;
		uint32 triangleByteStride;

		float *points;
		int *triangles;
	};

	struct ConvexHullSet
	{
		vector<ConvexHull> hulls;
	};

	namespace ConvexHullBuilding
	{
		// Builds convex hull set for a model without caching
		void BuildConvexHulls(ConvexHullSet *set, Model *model);
	}
}
