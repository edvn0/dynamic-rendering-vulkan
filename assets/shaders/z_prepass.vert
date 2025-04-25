#version 460

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 transform_rotation;
layout(location = 4) in vec4 transform_translation_and_scale;
layout(location = 5) in vec4 transform_non_uniform_scale;

mat3 quaternion_to_matrix(vec4);
mat4 reconstruct_transform_matrix();

layout(set = 0, binding = 0, std140) uniform UniformBufferObject
{
    mat4 model_view_projection;
}
ubo;

void main()
{
    mat4 model_matrix = reconstruct_transform_matrix();
    vec4 world_position = model_matrix * vec4(a_position, 1.0);
    gl_Position = ubo.model_view_projection * world_position;
}

mat3 quaternion_to_matrix(vec4 q)
{
    q = normalize(q);

    float x = q.x;
    float y = q.y;
    float z = q.z;
    float w = q.w;

    float xx = x * x;
    float xy = x * y;
    float xz = x * z;
    float xw = x * w;

    float yy = y * y;
    float yz = y * z;
    float yw = y * w;

    float zz = z * z;
    float zw = z * w;

    return mat3(1.0 - 2.0 * (yy + zz), 2.0 * (xy - zw), 2.0 * (xz + yw),
                2.0 * (xy + zw), 1.0 - 2.0 * (xx + zz), 2.0 * (yz - xw),
                2.0 * (xz - yw), 2.0 * (yz + xw), 1.0 - 2.0 * (xx + yy));
}

mat4 reconstruct_transform_matrix()
{
    vec3 translation = transform_translation_and_scale.xyz;
    vec3 non_uniform_scale = transform_non_uniform_scale.xyz;
    vec4 rotation = transform_rotation;

    mat3 rotation_matrix = quaternion_to_matrix(rotation);

    mat3 transform_matrix = rotation_matrix * mat3(non_uniform_scale.x, 0.0, 0.0,
                                                   0.0, non_uniform_scale.y, 0.0,
                                                   0.0, 0.0, non_uniform_scale.z);

    return mat4(vec4(transform_matrix[0], 0.0), vec4(transform_matrix[1], 0.0),
                vec4(transform_matrix[2], 0.0), vec4(translation, 1.0));
}