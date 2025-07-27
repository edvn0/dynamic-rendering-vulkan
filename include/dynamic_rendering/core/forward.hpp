#pragma once

namespace Core {
class Instance;
}

namespace DynamicRendering {
class App;
struct ViewportBounds;
}

class Event;
class Device;
class Window;
class CommandBuffer;
class GPUBuffer;
class Image;
class ImageArray;
class Shader;
struct CompiledPipeline;
class GUISystem;
class Swapchain;
class EditorCamera;
class Camera;
struct ILayer;
class AssetFileWatcher;
class AssetReloader;
class VertexBuffer;
class IndexBuffer;
class Material;
class StaticMesh;
struct InitialisationParameters;

struct IFullscreenTechnique;

class Renderer;
class BlueprintRegistry;
struct PipelineBlueprint;
class DescriptorSetManager;

class Allocator;

class GPUBinding;
class BufferBinding;
class ImageBinding;
