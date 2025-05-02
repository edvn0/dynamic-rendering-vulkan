#pragma once

#include <expected>
#include <memory>
#include <span>
#include <vulkan/vulkan.h>

#include "core/forward.hpp"

#include "pipeline/blueprint_configuration.hpp"
#include "pipeline/pipeline_error.hpp"

struct IPipelineFactory
{
  virtual ~IPipelineFactory() = default;
  virtual auto create_pipeline(const PipelineBlueprint&,
                               const PipelineLayoutInfo&) const
    -> std::expected<std::unique_ptr<CompiledPipeline>, PipelineError> = 0;
};