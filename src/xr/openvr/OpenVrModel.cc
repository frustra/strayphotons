#include "xr/openvr/OpenVrModel.hh"

#include "Common.hh"
#include "core/Logging.hh"
#include "graphics/opengl/GLModel.hh"
#include "graphics/opengl/GLTexture.hh"
#include "tiny_gltf.h"
#include "xr/XrAction.hh"

#include <filesystem>
#include <stdexcept>
#include <thread>

using namespace sp;
using namespace xr;

OpenVrModel::OpenVrModel(std::string name, vr::RenderModel_t *vrModel, vr::RenderModel_TextureMap_t *vrTex)
    : XrModel(name) {
    static BasicMaterial defaultMat;
    metallicRoughnessTex = defaultMat.metallicRoughnessTex;
    heightTex = defaultMat.heightTex;

    baseColorTex.Create()
        .Filter(GL_NEAREST, GL_NEAREST)
        .Wrap(GL_REPEAT, GL_REPEAT)
        .Size(vrTex->unWidth, vrTex->unHeight)
        .Storage(PF_RGBA8)
        .Image2D(vrTex->rubTextureMapData);

    GLModel::Primitive prim;
    prim.parent = &sourcePrim;
    prim.baseColorTex = &baseColorTex;
    prim.metallicRoughnessTex = &metallicRoughnessTex;
    prim.heightTex = &heightTex;

    static_assert(sizeof(*vr::RenderModel_t::rIndexData) == sizeof(uint16), "index data wrong size");

    vbo.SetElementsVAO(vrModel->unVertexCount, (SceneVertex *)vrModel->rVertexData);
    prim.vertexBufferHandle = vbo.VAO();

    ibo.Create().Data(vrModel->unTriangleCount * 3 * sizeof(uint16), vrModel->rIndexData);
    prim.indexBufferHandle = ibo.handle;

    sourcePrim.drawMode = Model::DrawMode::Triangles;
    sourcePrim.indexBuffer.byteOffset = 0;
    sourcePrim.indexBuffer.components = vrModel->unTriangleCount * 3;
    sourcePrim.indexBuffer.componentType = GL_UNSIGNED_SHORT;

    shared_ptr<GLModel> glModel = make_shared<GLModel>(this, nullptr);
    glModel->AddPrimitive(prim);
    nativeModel = glModel;
}

OpenVrModel::~OpenVrModel() {
    vbo.DestroyVAO().Destroy();
}

std::shared_ptr<XrModel> OpenVrModel::LoadOpenVrModel(vr::TrackedDeviceIndex_t deviceIndex) {
    std::string modelName = ModelName(deviceIndex);

    Logf("Loading VR render model %s", modelName);
    vr::RenderModel_t *vrModel;
    vr::RenderModel_TextureMap_t *vrTex;
    vr::EVRRenderModelError merr;

    while (true) {
        merr = vr::VRRenderModels()->LoadRenderModel_Async(modelName.c_str(), &vrModel);
        if (merr != vr::VRRenderModelError_Loading) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (merr != vr::VRRenderModelError_None) {
        Errorf("VR render model error: %s", vr::VRRenderModels()->GetRenderModelErrorNameFromEnum(merr));
        return nullptr;
    }

    while (true) {
        merr = vr::VRRenderModels()->LoadTexture_Async(vrModel->diffuseTextureId, &vrTex);
        if (merr != vr::VRRenderModelError_Loading) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (merr != vr::VRRenderModelError_None) {
        Errorf("VR render texture error: %s", vr::VRRenderModels()->GetRenderModelErrorNameFromEnum(merr));
        return nullptr;
    }

    std::shared_ptr<XrModel> xrModel = std::shared_ptr<OpenVrModel>(new OpenVrModel(modelName, vrModel, vrTex));

    vr::VRRenderModels()->FreeTexture(vrTex);
    vr::VRRenderModels()->FreeRenderModel(vrModel);

    return xrModel;
}

std::string OpenVrModel::ModelName(vr::TrackedDeviceIndex_t deviceIndex) {
    if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid) {
        Errorf("Failed to get tracked device index for TrackedObjectHandle");
        return "";
    }

    uint32_t modelNameLength =
        vr::VRSystem()->GetStringTrackedDeviceProperty(deviceIndex, vr::Prop_RenderModelName_String, NULL, 0);
    std::shared_ptr<char[]> modelNameStr(new char[modelNameLength]);

    vr::VRSystem()->GetStringTrackedDeviceProperty(deviceIndex,
                                                   vr::Prop_RenderModelName_String,
                                                   modelNameStr.get(),
                                                   modelNameLength);

    return std::string(modelNameStr.get());
}

std::shared_ptr<XrModel> OpenVrSkeleton::LoadOpenVrSkeleton(std::string skeletonAction) {
    std::string modelPathStr = ModelName(skeletonAction);

    // ModelName has no error checking. We need to validate the path exists
    std::filesystem::path modelPath(modelPathStr);
    if (!std::filesystem::exists(modelPath) || std::filesystem::is_directory(modelPath)) {
        Errorf("OpenVR Skeleton GLTF File Path (%s) is not a file", modelPathStr);
        return nullptr;
    }

    tinygltf::TinyGLTF gltfLoader;
    std::string err;
    std::string warn;
    std::shared_ptr<tinygltf::Model> gltfModel = make_shared<tinygltf::Model>();

    if (!gltfLoader.LoadBinaryFromFile(gltfModel.get(), &err, &warn, modelPathStr)) {
        Errorf("Failed to parse OpenVR Skeleton GLTF file: %s", modelPathStr);
        Errorf("TinyGLTF Error: %s", err.c_str());
        Errorf("TinyGLTF Warn: %s", warn.c_str());
        return nullptr;
    }

    std::shared_ptr<XrModel> xrModel = std::shared_ptr<OpenVrSkeleton>(new OpenVrSkeleton(modelPathStr, gltfModel));

    return xrModel;
}

// Standardized logic for determining the model path for a particular skeleton. We use this to:
// - Find the model to load during the initial load process
// - As a the key in a map which stores already-loaded Action models in OpenVrAction
std::string OpenVrSkeleton::ModelName(std::string skeletonAction) {
    const char *handResource = NULL;
    if (skeletonAction == xr::LeftHandSkeletonActionName) {
        handResource = xr::openvr::LeftHandModelResource;
    } else if (skeletonAction == xr::RightHandSkeletonActionName) {
        handResource = xr::openvr::RightHandModelResource;
    } else {
        throw std::runtime_error("Unknown skeleton hand action");
    }

    // Get the length of the path to the model
    uint32_t modelPathLen =
        vr::VRResources()->GetResourceFullPath(handResource, xr::openvr::HandModelResourceDir, NULL, 0);

    std::shared_ptr<char[]> modelPathStr(new char[modelPathLen]);
    vr::VRResources()->GetResourceFullPath(handResource,
                                           xr::openvr::HandModelResourceDir,
                                           modelPathStr.get(),
                                           modelPathLen);

    return std::string(modelPathStr.get());
}
