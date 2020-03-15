#pragma once

#include "xr/XrModel.hh"

#include <openvr.h>

namespace sp
{
	namespace xr
	{

		class OpenVrModel : public XrModel
		{
		public:
			OpenVrModel(vr::RenderModel_t *vrModel, vr::RenderModel_TextureMap_t *vrTex);
			virtual ~OpenVrModel();

		private:
			Texture baseColorTex, metallicRoughnessTex, heightTex;
			VertexBuffer vbo;
			Buffer ibo;
			Model::Primitive sourcePrim;
		};

	}
}