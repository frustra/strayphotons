#pragma once

#include "xr/XrModel.hh"

#include <openvr.h>

namespace sp {
    namespace xr {
        namespace openvr {
            // TODO: Use system-independent paths for SteamVR model loading. #42
            static const char *HandModelResourceDir = "rendermodels\\vr_glove\\";
            static const char *LeftHandModelResource = "vr_glove_left_model.glb";
            static const char *RightHandModelResource = "vr_glove_right_model.glb";
        }; // namespace openvr

        class OpenVrModel : public XrModel {
        public:
            ~OpenVrModel();

            static std::shared_ptr<XrModel> LoadOpenVrModel(vr::TrackedDeviceIndex_t deviceIndex);
            static std::string ModelName(vr::TrackedDeviceIndex_t deviceIndex);

        private:
            // OpenVrModels can onyl be created using OpenVrModel::LoadOpenVRModel()
            OpenVrModel(std::string name, vr::RenderModel_t *vrModel, vr::RenderModel_TextureMap_t *vrTex);

            Texture baseColorTex, metallicRoughnessTex, heightTex;
            VertexBuffer vbo;
            Buffer ibo;
            Model::Primitive sourcePrim;
        };

        class OpenVrSkeleton : public XrModel {
        public:
            static std::shared_ptr<XrModel> LoadOpenVrSkeleton(std::string skeletonAction);
            static std::string ModelName(std::string skeletonAction);

        private:
            // OpenVrSkeletons can only be created using OpenVrSkeleton::LoadOpenVrSkeleton()
            OpenVrSkeleton(const string &name, shared_ptr<tinygltf::Model> model) : XrModel(name, model){};
        };

    } // namespace xr
} // namespace sp