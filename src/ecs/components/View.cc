#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_access.hpp>

#include "ecs/components/View.hh"
#include "ecs/components/Transform.hh"

namespace ecs
{
	void ValidateView(Entity viewEntity)
	{
		if (!viewEntity.Valid())
		{
			throw std::runtime_error("view entity is not valid because the entity has been deleted");
		}
		if (!viewEntity.Has<View>())
		{
			throw std::runtime_error("view entity is not valid because it has no View component");
		}
		if (!viewEntity.Get<ecs::View>()->vrEye && !viewEntity.Has<Transform>())
		{
			throw std::runtime_error("view entity is not valid because it has no Transform component");
		}
	}

	Handle<View> UpdateViewCache(Entity entity, float fov)
	{
		ValidateView(entity);

		auto view = entity.Get<View>();
		view->aspect = (float)view->extents.x / (float)view->extents.y;

		if (!view->vrEye)
		{
			view->projMat = glm::perspective(fov > 0.0 ? fov : view->fov, view->aspect, view->clip[0], view->clip[1]);

			auto transform = entity.Get<Transform>();
			view->invViewMat = transform->GetGlobalTransform();
		}

		view->invProjMat = glm::inverse(view->projMat);
		view->viewMat = glm::inverse(view->invViewMat);

		return view;
	}
}
