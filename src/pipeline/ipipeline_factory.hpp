#pragma once

#include <span>
#include <vulkan/vulkan.h>

#include "blueprint_configuration.hpp"
#include "forward.hpp"

struct IPipelineFactory
{
  virtual ~IPipelineFactory() = default;
  virtual auto create_pipeline(const PipelineBlueprint&,
                               const PipelineLayoutInfo&) const
    -> std::unique_ptr<CompiledPipeline> = 0;
};