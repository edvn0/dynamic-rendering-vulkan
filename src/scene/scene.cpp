#include "scene/scene.hpp"

#include "scene/components.hpp"

Scene::Scene(const std::string_view n)
  : scene_name(n) {};

Entity
Scene::create_entity(std::string_view n)
{
  std::string final_name = n.empty() ? "Entity" : std::string(n);
  if (!tag_registry.is_unique(final_name))
    final_name = tag_registry.generate_unique_name(final_name);

  const entt::entity handle = registry.create();
  tag_registry.register_tag(final_name, handle);

  registry.emplace<Component::Tag>(handle, final_name);
  registry.emplace<Component::Transform>(handle);

  return Entity(handle, this);
}

auto
Scene::create_entt_entity() -> entt::entity
{
  return registry.create();
}