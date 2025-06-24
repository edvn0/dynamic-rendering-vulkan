#include <new>
#include <tracy/Tracy.hpp>

void*
operator new(decltype(sizeof(0)) n) noexcept(false)
{
  void* ptr = std::malloc(n);
  if (!ptr)
    throw std::bad_alloc();
  TracyAlloc(ptr, n);
  return ptr;
}

void
operator delete(void* p) throw()
{
  TracyFree(p);
  std::free(p);
}

void*
operator new[](decltype(sizeof(0)) n) noexcept(false)
{
  void* ptr = std::malloc(n);
  if (!ptr)
    throw std::bad_alloc();
  TracyAlloc(ptr, n);
  return ptr;
}

void
operator delete[](void* p) throw()
{
  TracyFree(p);
  std::free(p);
}
