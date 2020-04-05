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
			OpenVrModel(std::string name, vr::RenderModel_t *vrModel, vr::RenderModel_TextureMap_t *vrTex);
			virtual ~OpenVrModel();

			static std::shared_ptr<XrModel> LoadOpenVRModel(vr::TrackedDeviceIndex_t deviceIndex);

		private:
			Texture baseColorTex, metallicRoughnessTex, heightTex;
			VertexBuffer vbo;
			Buffer ibo;
			Model::Primitive sourcePrim;
		};

	}
}