#pragma once

#include "graphics/Graphics.hh"

#include <ecs/Ecs.hh>
#include <glm/glm.hpp>

namespace ecs {
	class View {
	public:
		enum ViewType {
			VIEW_TYPE_PANCAKE,
			VIEW_TYPE_XR,
			VIEW_TYPE_LIGHT,
		};

		View() {}
		View(glm::ivec2 extents) : extents(extents) {}

		// When setting these parameters, View needs to recompute some internal params
		void SetProjMat(glm::mat4 proj);
		void SetProjMat(float _fov, glm::vec2 _clip, glm::ivec2 _extents);
		void SetInvViewMat(glm::mat4 invView);

		glm::ivec2 GetExtents();
		glm::vec2 GetClip();
		float GetFov();

		glm::mat4 GetProjMat();
		glm::mat4 GetInvProjMat();
		glm::mat4 GetViewMat();
		glm::mat4 GetInvViewMat();

		// Optional parameters;
		glm::ivec2 offset = {0, 0};
		// TODO(any): Maybe remove color clear once we have interior spaces
		GLbitfield clearMode = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT;
		glm::vec4 clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
		bool stencil = false;
		bool blend = false;
		float skyIlluminance = 0.0f;
		float scale = 1.0f;

		// For XR Views
		ViewType viewType = VIEW_TYPE_PANCAKE;

		// private:
		// Required parameters.
		glm::ivec2 extents;
		glm::vec2 clip; // {near, far}
		float fov;

		// Updated automatically.
		float aspect;
		glm::mat4 projMat, invProjMat;
		glm::mat4 viewMat, invViewMat;
	};

	static Component<View> ComponentView("view");

	template<>
	bool Component<View>::LoadEntity(Entity &dst, picojson::value &src);

	void ValidateView(ecs::Entity viewEntity);
	ecs::Handle<ecs::View> UpdateViewCache(ecs::Entity entity, float fov = 0.0);
} // namespace ecs
