template<typename T>
auto
Assets::Handle<T>::get() -> T*
{
  return Assets::Manager::the().get(*this);
}

template<typename T>
auto
Assets::Handle<T>::get() const -> const T*
{
  return Assets::Manager::the().get(*this);
}
