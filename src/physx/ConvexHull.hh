#pragma once

#include "Common.hh"

namespace sp
{
	class Model;

	struct ConvexHull
	{
		float *points;
		uint32 pointCount;
		uint32 pointByteStride;

		int *triangles;
		uint32 triangleCount;
		uint32 triangleByteStride;
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
