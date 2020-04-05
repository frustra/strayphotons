#include "xr/openvr/OpenVrModel.hh"
#include "Common.hh"
#include "core/Logging.hh"
#include <stdexcept>
#include <thread>

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

std::shared_ptr<XrModel> OpenVrModel::LoadOpenVRModel(vr::TrackedDeviceIndex_t deviceIndex)
{
	// TODO: yikes, this is a big string. Declare it on the heap maybe?
	char tempVrProperty[vr::k_unMaxPropertyStringSize];

	if (deviceIndex == vr::k_unTrackedDeviceIndexInvalid)
	{
		throw std::runtime_error("Failed to get tracked device index for TrackedObjectHandle");
	}

	vr::VRSystem()->GetStringTrackedDeviceProperty(deviceIndex, vr::Prop_RenderModelName_String, tempVrProperty, vr::k_unMaxPropertyStringSize);

	Logf("Loading VR render model %s", tempVrProperty);
	vr::RenderModel_t *vrModel;
	vr::RenderModel_TextureMap_t *vrTex;
	vr::EVRRenderModelError merr;

	while (true)
	{
		merr = vr::VRRenderModels()->LoadRenderModel_Async(tempVrProperty, &vrModel);
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

	std::shared_ptr<XrModel> xrModel = make_shared<OpenVrModel>(std::string(tempVrProperty), vrModel, vrTex);

	vr::VRRenderModels()->FreeTexture(vrTex);
	vr::VRRenderModels()->FreeRenderModel(vrModel);

	return xrModel;
}