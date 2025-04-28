#pragma once

#include <span>
#include <vulkan/vulkan.h>

#include "core/forward.hpp"

#include "pipeline/blueprint_configuration.hpp"

struct IPipelineFactory
{
  virtual ~IPipelineFactory() = default;
  virtual auto create_pipeline(const PipelineBlueprint&,
                               const PipelineLayoutInfo&) const
    -> std::unique_ptr<CompiledPipeline> = 0;
};