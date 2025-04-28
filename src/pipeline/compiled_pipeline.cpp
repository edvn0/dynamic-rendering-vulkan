#include "pipeline/compiled_pipeline.hpp"

#include "core/device.hpp"
#include "pipeline/shader.hpp"

CompiledPipeline::~CompiledPipeline()
{
  if (layout)
    vkDestroyPipelineLayout(device->get_device(), layout, nullptr);
  if (pipeline)
    vkDestroyPipeline(device->get_device(), pipeline, nullptr);

  shader.reset();
}