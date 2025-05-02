#pragma once

#include <cstdint>
#include <string>

struct PipelineError
{
  std::string message;

  enum class Code : std::uint8_t
  {
    pipeline_layout_creation_failed,
    pipeline_creation_failed,
    unknown_error,
  };
  Code code{ Code::unknown_error };
};