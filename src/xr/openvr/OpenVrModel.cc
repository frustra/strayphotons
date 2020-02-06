#include "xr/openvr/OpenVrModel.hh"
#include "Common.hh"
#include "core/Logging.hh"
#include <stdexcept>
#include <thread>

using namespace sp;
using namespace xr;


OpenVrModel::OpenVrModel(vr::RenderModel_t *vrModel, vr::RenderModel_TextureMap_t *vrTex) :
	XrModel("openvr-model")
{
	static BasicMaterial defaultMat;
	roughnessTex = defaultMat.roughnessTex;
	metallicTex = defaultMat.metallicTex;
	heightTex = defaultMat.heightTex;

	baseColorTex.Create()
	.Filter(GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR).Wrap(GL_REPEAT, GL_REPEAT)
	.Size(vrTex->unWidth, vrTex->unHeight).Storage(PF_RGBA8).Image2D(vrTex->rubTextureMapData);

	GLModel::Primitive prim;
	prim.parent = &sourcePrim;
	prim.baseColorTex = &baseColorTex;
	prim.roughnessTex = &roughnessTex;
	prim.metallicTex = &metallicTex;
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