#include "scene/entity.hpp"
#include "scene/components.hpp"
#include "scene/scene.hpp"

Entity::Entity(entt::entity han, Scene* s)
  : handle(han)
  , scene(s)
{
}

auto
Entity::valid() const -> bool
{
  return scene->registry.valid(handle);
}

auto
Entity::set_parent(Entity parent) -> void
{
  if (!valid() || !parent.valid() || handle == parent.handle)
    return;

  auto& registry = scene->get_registry();

  if (!has_component<Component::Hierarchy>()) {
    add_component<Component::Hierarchy>();
  }
  if (!parent.has_component<Component::Hierarchy>()) {
    parent.add_component<Component::Hierarchy>();
  }

  auto& my_hierarchy = get_component<Component::Hierarchy>();
  auto& parent_hierarchy = parent.get_component<Component::Hierarchy>();

  if (my_hierarchy.parent != entt::null &&
      my_hierarchy.parent != parent.handle) {
    auto& old_parent_hierarchy =
      registry.get<Component::Hierarchy>(my_hierarchy.parent);
    std::erase(old_parent_hierarchy.children, handle);
  }

  // Set new parent
  my_hierarchy.parent = parent.handle;

  // Avoid double insertion
  if (std::find(parent_hierarchy.children.begin(),
                parent_hierarchy.children.end(),
                handle) == parent_hierarchy.children.end()) {
    parent_hierarchy.children.push_back(handle);
  }
}