#ifndef _Z_SLICING_GLSL
#define _Z_SLICING_GLSL

float compute_slice_depth(uint slice, uint num_slices, float near_z, float far_z)
{
  float slice_norm = float(slice) / float(num_slices);
  return far_z * pow(near_z / far_z, slice_norm);
}

uint compute_slice_index(float view_z, uint num_slices, float near_z, float far_z)
{
  float log_ratio = log(near_z / far_z);
  float slice_float = log(view_z / far_z) / log_ratio * float(num_slices);
  return uint(clamp(slice_float, 0.0, float(num_slices - 1)));
}

#endif