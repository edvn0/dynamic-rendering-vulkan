#ifndef _MATRIX_MATH_GLSL
#define _MATRIX_MATH_GLSL

#define RECONSTRUCT()                                                          \
  mat4(a_model_matrix_row0,                                                    \
       a_model_matrix_row1,                                                    \
       a_model_matrix_row2,                                                    \
       a_model_matrix_row3)

#endif