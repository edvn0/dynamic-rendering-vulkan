#include "app_layer.hpp"
#include <dynamic_rendering/dynamic_rendering.hpp>

int
main(int argc, char** argv)
{
  DynamicRendering::App app("My Dynamic App");

  app.add_layer<AppLayer>();

  return app.run(argc, argv).value();
}
