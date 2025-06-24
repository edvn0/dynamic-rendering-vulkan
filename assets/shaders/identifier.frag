#version 460

layout(location = 0) flat in uint frag_color;

layout(location = 0) out uint out_color;

void main()
{
    out_color = frag_color;
}
