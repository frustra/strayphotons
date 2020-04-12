#pragma once

#include "xr/XrModel.hh"

#include <openvr.h>

namespace sp
{
	namespace xr
	{
		static const char* HandModelResourceDir = "rendermodels\\vr_glove\\";
		static const char* LeftHandModelResource = "vr_glove_left_model.glb";
		static const char* RightHandModelResource = "vr_glove_right_model.glb";

		class OpenVrModel : public XrModel
		{
		public:
			OpenVrModel(std::string name, vr::RenderModel_t *vrModel, vr::RenderModel_TextureMap_t *vrTex);
			OpenVrModel(const string &name, shared_ptr<tinygltf::Model> model) : XrModel(name, model) {};
			virtual ~OpenVrModel();

			static std::shared_ptr<XrModel> LoadOpenVRModel(vr::TrackedDeviceIndex_t deviceIndex);
			static std::shared_ptr<XrModel> LoadOpenVrSkeleton(std::string skeletonAction);

		private:
			Texture baseColorTex, metallicRoughnessTex, heightTex;
			VertexBuffer vbo;
			Buffer ibo;
			Model::Primitive sourcePrim;
		};

	}
}