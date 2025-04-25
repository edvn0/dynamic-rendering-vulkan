#include "compiled_pipeline.hpp"

#include <device.hpp>

#include "shader.hpp"

CompiledPipeline::~CompiledPipeline()
{
  if (layout)
    vkDestroyPipelineLayout(device->get_device(), layout, nullptr);
  if (pipeline)
    vkDestroyPipeline(device->get_device(), pipeline, nullptr);

  shader.reset();
}