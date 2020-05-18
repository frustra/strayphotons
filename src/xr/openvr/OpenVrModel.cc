#include "xr/openvr/OpenVrModel.hh"
#include "xr/XrAction.hh"
#include "Common.hh"
#include "core/Logging.hh"
#include "tiny_gltf.h"

#include <stdexcept>
#include <thread>
#include <filesystem>

using namespace sp;
using namespace xr;


OpenVrModel::OpenVrModel(std::string name, vr::RenderModel_t *vrModel, vr::RenderModel_TextureMap_t *vrTex) :
	XrModel(name)
{
	static BasicMaterial defaultMat;
	metallicRoughnessTex = defaultMat.metallicRoughnessTex;
	heightTex = defaultMat.heightTex;

	baseColorTex.Create()
		.Filter(GL_NEAREST, GL_NEAREST).Wrap(GL_REPEAT, GL_REPEAT)
		.Size(vrTex->unWidth, vrTex->unHeight).Storage(PF_RGBA8).Image2D(vrTex->rubTextureMapData);

	GLModel::Primitive prim;
	prim.parent = &sourcePrim;
	prim.baseColorTex = &baseColorTex;
	prim.metallicRoughnessTex = &metallicRoughnessTex;
	prim.heightTex = &heightTex;

	static_assert(sizeof(*vr::RenderModel_t::rIndexData) == sizeof(uint16), "index data wrong size");

	vbo.SetElementsVAO(vrModel->unVertexCount, (SceneVertex *) vrModel->rVertexData);
	prim.vertexBufferHandle = vbo.VAO();

	ibo.Create().Data(vrModel->unTriangleCount * 3 * sizeof(uint16), vrModel->rIndexData);
	prim.indexBufferHandle = ibo.handle;

	sourcePrim.drawMode = GL_TRIANGLES;
	sourcePrim.indexBuffer.byteOffset = 0;
	sourcePrim.indexBuffer.components = vrModel->unTriangleCount * 3;
	sourcePrim.indexBuffer.componentType = GL_UNSIGNED_SHORT;

	glModel = make_shared<GLModel>(this);
	glModel->AddPrimitive(prim);
}

OpenVrModel::~OpenVrModel()
{
	vbo.DestroyVAO().Destroy();
}

std::shared_ptr<XrModel> OpenVrModel::LoadOpenVrModel(vr::TrackedDeviceIndex_t deviceIndex)
{
	std::shared_ptr<char[]> tempVrProperty(new char[vr::k_unMaxPropertyStringSize]);

	if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid)
	{
		throw std::runtime_error("Failed to get tracked device index for TrackedObjectHandle");
	}

	vr::VRSystem()->GetStringTrackedDeviceProperty(deviceIndex, vr::Prop_RenderModelName_String, tempVrProperty.get(), vr::k_unMaxPropertyStringSize);

	Logf("Loading VR render model %s", tempVrProperty.get());
	vr::RenderModel_t *vrModel;
	vr::RenderModel_TextureMap_t *vrTex;
	vr::EVRRenderModelError merr;

	while (true)
	{
		merr = vr::VRRenderModels()->LoadRenderModel_Async(tempVrProperty.get(), &vrModel);
		if (merr != vr::VRRenderModelError_Loading) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (merr != vr::VRRenderModelError_None)
	{
		Errorf("VR render model error: %s", vr::VRRenderModels()->GetRenderModelErrorNameFromEnum(merr));
		throw std::runtime_error("Failed to load VR render model");
	}

	while (true)
	{
		merr = vr::VRRenderModels()->LoadTexture_Async(vrModel->diffuseTextureId, &vrTex);
		if (merr != vr::VRRenderModelError_Loading) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if (merr != vr::VRRenderModelError_None)
	{
		Errorf("VR render texture error: %s", vr::VRRenderModels()->GetRenderModelErrorNameFromEnum(merr));
		throw std::runtime_error("Failed to load VR render texture");
	}

	std::shared_ptr<XrModel> xrModel = std::shared_ptr<OpenVrModel>(new OpenVrModel(std::string(tempVrProperty.get()), vrModel, vrTex));

	vr::VRRenderModels()->FreeTexture(vrTex);
	vr::VRRenderModels()->FreeRenderModel(vrModel);

	return xrModel;
}

std::shared_ptr<XrModel> OpenVrSkeleton::LoadOpenVrSkeleton(std::string skeletonAction)
{
	const char* handResource = NULL;
	if (skeletonAction == xr::LeftHandSkeletonActionName)
	{
		handResource = xr::openvr::LeftHandModelResource;
	}
	else if (skeletonAction == xr::RightHandSkeletonActionName)
	{
		handResource = xr::openvr::RightHandModelResource;
	}
	else
	{
		return false;
	}

	// Get the length of the path to the model
    size_t modelPathLen = vr::VRResources()->GetResourceFullPath(handResource, xr::openvr::HandModelResourceDir, NULL, 0);

	std::shared_ptr<char[]> modelPathStr(new char[modelPathLen]);
	vr::VRResources()->GetResourceFullPath(handResource, xr::openvr::HandModelResourceDir, modelPathStr.get(), modelPathLen);

	// GetResourceFullPath has no error checking. We need to validate the path exists
	std::filesystem::path modelPath(modelPathStr.get());
	if (!std::filesystem::exists(modelPath) || std::filesystem::is_directory(modelPath))
	{
		Errorf("OpenVR Skeleton GLTF File Path (%s) is not a file", modelPathStr.get());
		return false;
	}

	tinygltf::TinyGLTF gltfLoader;
	std::string err;
	std::string warn;
	std::shared_ptr<tinygltf::Model> gltfModel = make_shared<tinygltf::Model>();

	if (!gltfLoader.LoadBinaryFromFile(gltfModel.get(), &err, &warn, std::string(modelPathStr.get())))
	{
		Errorf("Failed to parse OpenVR Skeleton GLTF file: %s", modelPathStr.get());
		Errorf("TinyGLTF Error: %s", err.c_str());
		Errorf("TinyGLTF Warn: %s", warn.c_str());
		return false;
	}
	
	std::shared_ptr<XrModel> xrModel = std::shared_ptr<OpenVrSkeleton>(new OpenVrSkeleton(handResource, gltfModel));

	return xrModel;
}