#include "scene/entity.hpp"
#include "scene/scene.hpp"

Entity::Entity(entt::entity han, Scene* s)
  : handle(han)
  , scene(s)
{
}
