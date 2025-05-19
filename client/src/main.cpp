#include "app_layer.hpp"
#include <dynamic_rendering/dynamic_rendering.hpp>

int
main(int argc, char** argv)
{
  auto args = DynamicRendering::parse_command_line_args(argc, argv);
  if (!args) {
    std::cerr << "Failed to parse command line arguments: "
              << args.error().message() << std::endl;
    return 1;
  }

  DynamicRendering::App app(*args);

  auto* layer = app.add_layer<AppLayer>();
  app.add_ray_pick_listener(
    dynamic_cast<DynamicRendering::IRayPickListener*>(layer));

  return app.run().value();
}
