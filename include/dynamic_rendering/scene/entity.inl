#pragma once

template<typename T, typename... Args>
T&
Entity::add_component(Args&&... args)
{
  return scene->get_registry().emplace<T>(handle, std::forward<Args>(args)...);
}

template<typename T>
T&
Entity::get_component()
{
  return scene->get_registry().get<T>(handle);
}

template<typename T>
bool
Entity::has_component() const
{
  return scene->get_registry().any_of<T>(handle);
}

template<typename T>
auto
Entity::try_get() -> T*
{
  if (scene->get_registry().any_of<T>(handle)) {
    return &scene->get_registry().get<T>(handle);
  }
  return nullptr;
}

template<typename T>
auto
ReadonlyEntity::try_get() const -> const T*
{
  auto* component = entity.scene->get_registry().try_get<const T>(entity.raw());
  return component;
}
