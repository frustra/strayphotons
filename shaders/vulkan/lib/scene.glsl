/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

struct MeshPrimitive {
    uint firstIndex, vertexOffset;
    uint indexCount, vertexCount;
    uint jointsVertexOffset;
    uint16_t baseColorTexID, metallicRoughnessTexID;
};

struct MeshModel {
    uint primitiveOffset;
    uint primitiveCount;
    uint indexOffset;
    uint vertexOffset;
};

struct RenderableEntity {
    mat4 modelToWorld;
    uint modelIndex;
    uint visibilityMask;
    uint vertexOffset;
    uint jointPosesOffset;
    uint opticID;
    float emissiveScale;
    int baseColorOverrideID;
    int metallicRoughnessOverrideID;
	uvec4 decalIDs;
};

struct SceneVertex {
    vec4 positionAndNormalX;
    vec2 normalYZ;
    vec2 uv;
};
