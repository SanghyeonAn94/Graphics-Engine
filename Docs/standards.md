# Next-Generation Graphics Engine Standards

This document outlines the coding standards and architectural principles for building a cutting-edge, high-performance graphics engine.

## CRITICAL: Core Architecture Principles

### Hexagonal Architecture + Modern Patterns
**This project MUST strictly follow Hexagonal Architecture with modern engine-specific patterns.**

#### Core Architectural Layers
1. **Domain Layer**: Pure business logic with zero external dependencies
2. **Ports**: Interface definitions for external communication
3. **Adapters**: External system implementations (rendering APIs, I/O, platform)
4. **Application**: Orchestration and use case coordination

#### Advanced Directory Structure
```
Engine/
├── Domain/                    # Pure business logic
│   ├── Entities/             # Core game objects (ECS components)
│   ├── Systems/              # ECS systems (logic processors)
│   ├── UseCases/             # High-level operations
│   └── Ports/                # Interface definitions
├── Infrastructure/           # External adapters
│   ├── Rendering/           # Graphics API adapters
│   │   ├── Vulkan/          # Vulkan implementation
│   │   ├── DirectX12/       # DirectX 12 implementation
│   │   ├── Metal/           # Metal implementation
│   │   └── Common/          # Shared rendering utilities
│   ├── Platform/            # OS-specific adapters
│   ├── Physics/             # Physics engine adapters
│   ├── Audio/               # Audio system adapters
│   └── FileSystem/          # File I/O and asset loading
├── Core/                    # Engine core systems
│   ├── ECS/                 # Entity Component System
│   ├── Memory/              # Custom allocators
│   ├── Threading/           # Job system and task scheduler
│   ├── Profiling/           # Performance profiling tools
│   └── Reflection/          # Runtime type information
└── Application/             # Application coordination layer
```

## Modern C++ Standards (C++23/26)

### Language Requirements
- **Minimum**: C++23 with C++26 features where available
- **Modules**: Use C++20 modules for all new code
- **Concepts**: Type constraints for all template code
- **Ranges**: STL ranges for all container operations
- **Coroutines**: For async operations and scripting

### Advanced C++ Features
```cpp
// Modules
module Engine.Rendering;
import Engine.Core;
import std;

// Concepts for type safety
template<RenderableEntity T>
requires requires(T t, const RenderContext& ctx) {
    t.render(ctx);
    { t.getBounds() } -> std::convertible_to<BoundingBox>;
}
class RenderSystem {
public:
    void render(std::ranges::range auto&& entities, const RenderContext& context) {
        std::ranges::for_each(entities | std::views::filter([](const auto& e) { 
            return e.isVisible(); 
        }), [&](const auto& entity) {
            entity.render(context);
        });
    }
};

// Reflection and serialization
template<typename T>
concept Serializable = requires(T t, BinaryStream& stream) {
    t.serialize(stream);
    T::deserialize(stream);
};
```

## Entity Component System (ECS) Architecture

### Data-Oriented Design Principles
- **Components**: Pure data structures (no behavior)
- **Systems**: Pure logic processors (no data storage)
- **Entities**: Lightweight IDs linking components
- **Archetypes**: Memory-efficient component grouping

### ECS Implementation
```cpp
// Component example - pure data
struct TransformComponent {
    glm::mat4 worldMatrix{1.0f};
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    bool isDirty = true;
};

// System example - pure logic
class TransformSystem : public SystemBase {
public:
    void update(ECSWorld& world, float deltaTime) override {
        auto query = world.createQuery<TransformComponent, MovementComponent>();
        
        query.forEach([deltaTime](Entity entity, TransformComponent& transform, 
                                 const MovementComponent& movement) {
            if (movement.velocity != glm::vec3{0}) {
                transform.position += movement.velocity * deltaTime;
                transform.isDirty = true;
            }
        });
    }
};
```

## Advanced Engine Systems

### Multi-Threading & Job System
```cpp
// Job-based parallelism
template<typename Callable>
concept JobFunction = requires(Callable c) {
    { c() } -> std::convertible_to<void>;
};

class JobSystem {
public:
    template<JobFunction F>
    auto submit(F&& job) -> std::future<decltype(job())> {
        return m_threadPool.submit(std::forward<F>(job));
    }
    
    void parallelFor(size_t count, auto&& func) {
        constexpr size_t minPerThread = 1000;
        const size_t numThreads = std::min(count / minPerThread, 
                                          std::thread::hardware_concurrency());
        // Implementation...
    }
};
```

### GPU-Driven Rendering
```cpp
// Bindless resources and GPU culling
class GPUDrivenRenderer {
private:
    struct GPUDrawCommand {
        uint32_t indexCount;
        uint32_t instanceCount;
        uint32_t firstIndex;
        uint32_t vertexOffset;
        uint32_t firstInstance;
    };
    
    struct ObjectData {
        glm::mat4 modelMatrix;
        uint32_t materialIndex;
        BoundingSphere bounds;
    };
    
public:
    void render(const CameraComponent& camera) {
        // GPU frustum culling compute shader
        dispatchCullingPass(camera);
        
        // Multi-draw indirect from GPU buffer
        executeIndirectDraws();
    }
};
```

### Ray Tracing Integration
```cpp
class RayTracingPipeline {
public:
    void buildAccelerationStructure(const std::vector<Mesh>& meshes) {
        // Build BLAS for each mesh
        for (const auto& mesh : meshes) {
            m_bottomLevelAS.emplace_back(createBLAS(mesh));
        }
        
        // Build TLAS for scene
        m_topLevelAS = createTLAS(m_bottomLevelAS);
    }
    
    void traceRays(const RayGenerationParams& params) {
        bindRayTracingPipeline();
        dispatchRays(params.width, params.height, 1);
    }
};
```

## Performance & Optimization Standards

### Cache-Friendly Data Structures
```cpp
// Structure of Arrays (SoA) for better cache performance
template<typename... Components>
class ComponentArray {
private:
    std::tuple<std::vector<Components>...> m_arrays;
    std::vector<EntityId> m_entities;
    
public:
    template<typename Component>
    auto& get(size_t index) {
        return std::get<std::vector<Component>>(m_arrays)[index];
    }
    
    void forEach(auto&& func) {
        for (size_t i = 0; i < m_entities.size(); ++i) {
            func(m_entities[i], get<Components>(i)...);
        }
    }
};
```

### Custom Memory Allocators
```cpp
template<size_t BlockSize = 4096>
class StackAllocator {
private:
    alignas(std::max_align_t) std::byte m_memory[BlockSize];
    size_t m_offset = 0;
    
public:
    template<typename T>
    [[nodiscard]] T* allocate(size_t count = 1) {
        const size_t size = sizeof(T) * count;
        const size_t alignment = alignof(T);
        
        m_offset = alignUp(m_offset, alignment);
        
        if (m_offset + size > BlockSize) {
            throw std::bad_alloc{};
        }
        
        auto* ptr = reinterpret_cast<T*>(m_memory + m_offset);
        m_offset += size;
        return ptr;
    }
    
    void reset() { m_offset = 0; }
};
```

## Hot-Reloading System
```cpp
class HotReloadManager {
private:
    std::unordered_map<std::string, FileWatcher> m_watchers;
    std::unordered_map<std::string, std::function<void()>> m_reloadCallbacks;
    
public:
    void watchFile(const std::string& path, std::function<void()> callback) {
        m_watchers[path] = FileWatcher{path, [this, callback](const std::string&) {
            callback();
        }};
        m_reloadCallbacks[path] = callback;
    }
    
    // Shader hot-reloading
    void watchShader(const std::string& shaderPath) {
        watchFile(shaderPath, [this, shaderPath]() {
            recompileShader(shaderPath);
            notifyRenderSystemsOfShaderChange(shaderPath);
        });
    }
};
```

---

## Table of Contents

- [Modern Graphics APIs](#modern-graphics-apis)
- [Code Style Guidelines](#code-style-guidelines)
- [Naming Conventions](#naming-conventions)
- [File Organization](#file-organization)
- [Documentation Standards](#documentation-standards)
- [Testing Framework](#testing-framework)
- [Build System](#build-system)

## Modern Graphics APIs

### Supported APIs (Priority Order)
1. **Vulkan** - Primary API for maximum performance and control
2. **DirectX 12** - Windows-specific optimizations
3. **Metal** - macOS/iOS native performance
4. **WebGPU** - Cross-platform future standard

### Vulkan Implementation Standards
```cpp
// Modern Vulkan usage with dynamic rendering
class VulkanRenderer {
private:
    VkDevice m_device;
    std::unique_ptr<DescriptorAllocator> m_descriptorAllocator;
    std::unique_ptr<CommandPool> m_commandPool;
    
public:
    void render(const RenderGraph& graph) {
        auto cmd = m_commandPool->allocateCommand();
        
        // Dynamic rendering (Vulkan 1.3)
        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {{0, 0}, m_swapchainExtent};
        
        vkCmdBeginRendering(cmd, &renderingInfo);
        
        // Bindless descriptors
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               m_pipelineLayout, 0, 1, &m_bindlessDescriptorSet, 0, nullptr);
        
        // GPU-driven rendering
        vkCmdDrawIndexedIndirect(cmd, m_indirectBuffer, 0, drawCount, sizeof(VkDrawIndexedIndirectCommand));
        
        vkCmdEndRendering(cmd);
    }
};
```

### Shader Compilation & Reflection
```cpp
class ShaderCompiler {
public:
    struct ShaderReflection {
        std::vector<DescriptorBinding> descriptorBindings;
        std::vector<PushConstantRange> pushConstants;
        std::vector<VertexAttribute> vertexInputs;
    };
    
    auto compileShader(const std::string& source, ShaderStage stage) 
        -> std::pair<std::vector<uint32_t>, ShaderReflection> {
        
        // Use shaderc for compilation
        shaderc::Compiler compiler;
        shaderc::CompileOptions options;
        
        options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        
        auto result = compiler.CompileGlslToSpv(source, convertStage(stage), "shader", options);
        
        // Reflect on SPIR-V
        auto reflection = reflectShader({result.cbegin(), result.cend()});
        
        return {{result.cbegin(), result.cend()}, reflection};
    }
};
```

## Code Style Guidelines

### Modern C++ Principles
- **Minimum C++23**: Use latest features for maximum efficiency
- **Zero-cost abstractions**: Template metaprogramming where beneficial
- **Concepts everywhere**: Type safety through compile-time constraints
- **Ranges and views**: Functional-style data processing
- **Modules**: Faster compilation and better encapsulation

### Formatting Standards
```cpp
// Use clang-format with custom engine style
// .clang-format configuration:
BasedOnStyle: LLVM
IndentWidth: 4
ColumnLimit: 120
BreakBeforeBraces: Attach
AllowShortFunctionsOnASingleLine: Inline
PointerAlignment: Left

// Example formatted code
class RenderSystem : public SystemBase {
public:
    explicit RenderSystem(RenderDevice* device) : m_device(device) {}
    
    void update(ECSWorld& world, float deltaTime) override {
        auto renderables = world.query<TransformComponent, RenderComponent>();
        
        renderables.forEach([this](Entity entity, const auto& transform, const auto& render) {
            if (render.isVisible && m_camera.isInFrustum(transform.bounds)) {
                submitDrawCommand(entity, transform, render);
            }
        });
    }
    
private:
    RenderDevice* m_device;
    std::vector<DrawCommand> m_drawCommands;
};
```

## Advanced Performance Standards

### SIMD and Vectorization
```cpp
// Use GLM's SIMD operations
#define GLM_FORCE_INTRINSICS
#include <glm/glm.hpp>
#include <glm/gtx/simd_vec4.hpp>

// Batch matrix operations
void updateTransforms(std::span<TransformComponent> transforms) {
    // Process 4 transforms at once using SIMD
    for (size_t i = 0; i + 3 < transforms.size(); i += 4) {
        glm::simdVec4 positions[4];
        // Load positions
        for (int j = 0; j < 4; ++j) {
            positions[j] = glm::simdVec4(transforms[i + j].position, 1.0f);
        }
        
        // Apply transformations using SIMD
        // ... SIMD operations
        
        // Store back
        for (int j = 0; j < 4; ++j) {
            transforms[i + j].worldMatrix = computeMatrix(positions[j]);
        }
    }
}
```

### Lock-Free Data Structures
```cpp
template<typename T, size_t Capacity>
class LockFreeRingBuffer {
private:
    alignas(64) std::atomic<size_t> m_head{0};
    alignas(64) std::atomic<size_t> m_tail{0};
    alignas(64) std::array<T, Capacity> m_buffer;
    
public:
    bool push(T&& item) {
        const auto current_tail = m_tail.load(std::memory_order_relaxed);
        const auto next_tail = (current_tail + 1) % Capacity;
        
        if (next_tail == m_head.load(std::memory_order_acquire)) {
            return false; // Buffer full
        }
        
        m_buffer[current_tail] = std::move(item);
        m_tail.store(next_tail, std::memory_order_release);
        return true;
    }
    
    bool pop(T& item) {
        const auto current_head = m_head.load(std::memory_order_relaxed);
        
        if (current_head == m_tail.load(std::memory_order_acquire)) {
            return false; // Buffer empty
        }
        
        item = std::move(m_buffer[current_head]);
        m_head.store((current_head + 1) % Capacity, std::memory_order_release);
        return true;
    }
};
```

## Naming Conventions

### Modern Naming Standards
```cpp
// Classes and Structs - PascalCase with descriptive names
class VulkanCommandBuffer {};
struct RayTracingPipelineState {};

// Concepts - PascalCase with descriptive constraints
template<typename T>
concept RenderableComponent = requires(T component) {
    component.render();
    { component.getBoundingBox() } -> std::convertible_to<BoundingBox>;
};

// Functions - camelCase with clear intent
auto createRenderTarget(uint32_t width, uint32_t height) -> std::unique_ptr<RenderTarget>;
void submitDrawCommands(std::span<const DrawCommand> commands);

// Variables - camelCase with semantic meaning
auto frameAllocator = StackAllocator<65536>{};
const auto maxConcurrentFrames = 3u;

// Member variables - m_ prefix
class Renderer {
private:
    VkDevice m_device;
    std::unique_ptr<CommandPool> m_commandPool;
    std::array<FrameData, 3> m_frameData;
};

// Constants - SCREAMING_SNAKE_CASE or constexpr
static constexpr uint32_t MAX_BINDLESS_TEXTURES = 1024;
static constexpr auto THREAD_POOL_SIZE = std::thread::hardware_concurrency();

// Enums - PascalCase with semantic prefixes
enum class RenderPassType {
    Shadow,
    GBuffer,
    Forward,
    PostProcess
};
```

### File Naming Conventions
```
// Headers - PascalCase with .hpp extension
VulkanRenderer.hpp
EntityComponentSystem.hpp
RayTracingPipeline.hpp

// Implementations - Same as header but .cpp
VulkanRenderer.cpp
EntityComponentSystem.cpp
RayTracingPipeline.cpp

// Module files - Use .cppm extension
RenderingSystem.cppm
CoreUtilities.cppm
```

## File Organization & Module System

### Modern Module-Based Organization
```cpp
// RenderingSystem.cppm - Module interface
export module Engine.Rendering;

export import Engine.Core;
import <vulkan/vulkan.h>;
import <memory>;
import <span>;

export namespace Engine::Rendering {
    class VulkanRenderer;
    class RenderGraph;
    
    template<RenderableComponent T>
    void submitForRendering(std::span<T> components);
}

// Implementation in same file or separate .cpp
module Engine.Rendering;

// Implementation details...
```

### Header Files (.hpp) - Legacy Support
```cpp
#pragma once

// Minimal includes - prefer forward declarations
#include <cstdint>
#include <memory>

// Forward declarations
namespace Engine::Core { class Entity; }
namespace Vulkan { class Device; }

// Use concepts for template constraints
template<typename T>
concept GraphicsResource = requires(T t) {
    t.getHandle();
    t.isValid();
};

namespace Engine::Rendering {
    class RenderTarget {
    public:
        explicit RenderTarget(uint32_t width, uint32_t height);
        ~RenderTarget() = default;
        
        // Move-only resource
        RenderTarget(const RenderTarget&) = delete;
        RenderTarget& operator=(const RenderTarget&) = delete;
        RenderTarget(RenderTarget&&) = default;
        RenderTarget& operator=(RenderTarget&&) = default;
        
        [[nodiscard]] auto getWidth() const noexcept -> uint32_t { return m_width; }
        [[nodiscard]] auto getHeight() const noexcept -> uint32_t { return m_height; }
        
    private:
        uint32_t m_width;
        uint32_t m_height;
        std::unique_ptr<Impl> m_impl;  // PIMPL idiom
    };
}
```

## Documentation Standards

### Self-Documenting Code Philosophy
```cpp
// Prefer clear naming over comments
auto calculateFrustumCulledObjects(const Camera& camera, 
                                  std::span<const RenderableObject> objects) 
    -> std::vector<RenderableObject>;

// Use concepts to document requirements
template<RenderableEntity T>
requires std::is_move_constructible_v<T> && 
         requires(T entity, const RenderContext& ctx) {
    entity.render(ctx);
    { entity.getBounds() } -> std::same_as<BoundingBox>;
}
void batchRender(std::span<T> entities, const RenderContext& context);
```

### Modern Documentation (C++23 Attributes)
```cpp
// Use attributes for documentation
[[nodiscard("Render targets are expensive resources")]]
auto createRenderTarget(uint32_t width, uint32_t height) -> std::unique_ptr<RenderTarget>;

[[deprecated("Use createMeshFromVertices instead")]]
auto createMesh(const std::vector<float>& vertices) -> Mesh;

// Contract programming (C++26 preview)
auto divideViewport(uint32_t width, uint32_t height, uint32_t divisions) 
    -> std::pair<uint32_t, uint32_t>
    pre r: divisions > 0
    post result: result.first * result.second <= width * height;
```

### API Documentation
```cpp
/// @brief High-performance GPU-driven rendering system
/// 
/// This system uses modern GPU features including:
/// - Bindless textures and samplers
/// - GPU-side frustum culling
/// - Indirect draw commands
/// - Mesh shaders (where available)
/// 
/// @performance O(1) CPU overhead regardless of object count
/// @thread_safety All methods are thread-safe unless noted
class GPUDrivenRenderer {
public:
    /// @brief Submit objects for GPU-driven rendering
    /// @param objects Range of renderable objects
    /// @param camera Camera for frustum culling
    /// @requires objects must remain valid until next frame
    /// @complexity O(1) CPU, O(n) GPU where n = object count
    template<std::ranges::range R>
    void submitObjects(R&& objects, const Camera& camera) noexcept;
};
```

## Advanced Performance Guidelines

### Data-Oriented Design Principles
```cpp
// Structure of Arrays (SoA) for better cache performance
struct TransformArrays {
    std::vector<glm::vec3> positions;
    std::vector<glm::quat> rotations; 
    std::vector<glm::vec3> scales;
    std::vector<glm::mat4> worldMatrices;  // Cached
    std::vector<bool> dirtyFlags;
    
    void updateMatrices() {
        // SIMD-friendly batch processing
        for (size_t i = 0; i < positions.size(); ++i) {
            if (dirtyFlags[i]) {
                worldMatrices[i] = glm::translate(glm::mat4(1.0f), positions[i]) *
                                  glm::mat4_cast(rotations[i]) *
                                  glm::scale(glm::mat4(1.0f), scales[i]);
                dirtyFlags[i] = false;
            }
        }
    }
};
```

### Branch Prediction Optimization
```cpp
// Minimize branches in hot paths
template<bool EnableDebugChecks = false>
class OptimizedRenderer {
public:
    void render(const RenderCommand& cmd) {
        // Compile-time branch elimination
        if constexpr (EnableDebugChecks) {
            validateRenderCommand(cmd);
        }
        
        // Use likely/unlikely attributes (C++20)
        if (cmd.hasTransparency()) [[unlikely]] {
            renderTransparent(cmd);
        } else [[likely]] {
            renderOpaque(cmd);
        }
    }
};
```

### Memory Pool Patterns
```cpp
// Frame-based allocation for temporary data
class FrameAllocator {
private:
    static constexpr size_t FRAME_SIZE = 64 * 1024 * 1024;  // 64MB
    alignas(64) std::byte m_memory[FRAME_SIZE];
    size_t m_offset = 0;
    
public:
    template<typename T>
    [[nodiscard]] auto allocate(size_t count = 1) -> T* {
        const size_t size = sizeof(T) * count;
        const size_t alignment = alignof(T);
        
        m_offset = (m_offset + alignment - 1) & ~(alignment - 1);
        
        if (m_offset + size > FRAME_SIZE) [[unlikely]] {
            throw std::bad_alloc{};
        }
        
        auto* ptr = reinterpret_cast<T*>(m_memory + m_offset);
        m_offset += size;
        return ptr;
    }
    
    void reset() noexcept { m_offset = 0; }
};
```

## Advanced Memory Management

### Smart Pointer Hierarchy
```cpp
// Prefer unique ownership with clear transfer semantics
class TextureManager {
private:
    std::unordered_map<std::string, std::unique_ptr<Texture>> m_textures;
    
public:
    // Transfer ownership to caller
    [[nodiscard]] auto extractTexture(const std::string& name) 
        -> std::unique_ptr<Texture> {
        auto it = m_textures.find(name);
        if (it != m_textures.end()) {
            auto texture = std::move(it->second);
            m_textures.erase(it);
            return texture;
        }
        return nullptr;
    }
    
    // Weak reference for observation
    [[nodiscard]] auto getTexture(const std::string& name) const noexcept 
        -> const Texture* {
        auto it = m_textures.find(name);
        return it != m_textures.end() ? it->second.get() : nullptr;
    }
};
```

### Custom Allocator Integration
```cpp
// Use PMR allocators for performance-critical containers
class RenderSystem {
private:
    std::pmr::monotonic_buffer_resource m_frameMemory{64 * 1024};
    std::pmr::vector<DrawCommand> m_drawCommands{&m_frameMemory};
    std::pmr::unordered_set<EntityId> m_visibleEntities{&m_frameMemory};
    
public:
    void beginFrame() {
        // Reset frame allocator
        m_frameMemory.release();
        m_drawCommands.clear();
        m_visibleEntities.clear();
    }
};
```

### GPU Memory Management
```cpp
class VulkanMemoryAllocator {
private:
    VmaAllocator m_allocator;
    
public:
    template<typename T>
    class GPUBuffer {
    private:
        VkBuffer m_buffer = VK_NULL_HANDLE;
        VmaAllocation m_allocation = nullptr;
        VmaAllocator m_allocator;
        
    public:
        explicit GPUBuffer(VmaAllocator allocator, size_t count, 
                          VkBufferUsageFlags usage) : m_allocator(allocator) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size = sizeof(T) * count;
            bufferInfo.usage = usage;
            
            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            
            vmaCreateBuffer(m_allocator, &bufferInfo, &allocInfo, 
                          &m_buffer, &m_allocation, nullptr);
        }
        
        ~GPUBuffer() {
            if (m_buffer != VK_NULL_HANDLE) {
                vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
            }
        }
        
        // Move-only semantics
        GPUBuffer(const GPUBuffer&) = delete;
        GPUBuffer& operator=(const GPUBuffer&) = delete;
        GPUBuffer(GPUBuffer&&) = default;
        GPUBuffer& operator=(GPUBuffer&&) = default;
    };
};
```

## Modern Error Handling

### Expected/Result Pattern (C++23)
```cpp
#include <expected>

enum class AssetError {
    FileNotFound,
    InvalidFormat,
    OutOfMemory,
    GPUResourceCreationFailed
};

// Modern error handling with std::expected
auto loadTexture(const std::filesystem::path& path) 
    -> std::expected<std::unique_ptr<Texture>, AssetError> {
    
    if (!std::filesystem::exists(path)) {
        return std::unexpected{AssetError::FileNotFound};
    }
    
    auto imageData = loadImageData(path);
    if (!imageData) {
        return std::unexpected{AssetError::InvalidFormat};
    }
    
    auto texture = createGPUTexture(*imageData);
    if (!texture) {
        return std::unexpected{AssetError::GPUResourceCreationFailed};
    }
    
    return texture;
}

// Usage with monadic operations
void loadAndUseTexture(const std::string& path) {
    loadTexture(path)
        .and_then([](auto&& texture) {
            return bindTextureToSlot(std::move(texture), 0);
        })
        .or_else([](AssetError error) {
            logError("Failed to load texture: {}", error);
            return getDefaultTexture();
        });
}
```

### Panic/Assert System for Development
```cpp
// Different assertion levels
#ifdef ENGINE_DEBUG
    #define ENGINE_ASSERT(condition, message) \
        do { \
            if (!(condition)) [[unlikely]] { \
                logCritical("Assertion failed: {} at {}:{}", message, __FILE__, __LINE__); \
                std::abort(); \
            } \
        } while(0)
#else
    #define ENGINE_ASSERT(condition, message) ((void)0)
#endif

// Runtime verification with recovery
#define ENGINE_VERIFY(condition, recovery) \
    do { \
        if (!(condition)) [[unlikely]] { \
            logWarning("Verification failed: {} at {}:{}", #condition, __FILE__, __LINE__); \
            recovery; \
        } \
    } while(0)

// Usage examples
void renderMesh(const Mesh& mesh) {
    ENGINE_ASSERT(mesh.isValid(), "Mesh must be valid before rendering");
    
    ENGINE_VERIFY(mesh.getVertexCount() > 0, return);
    
    // Render mesh...
}
```

## Testing Framework

### Modern Test Structure with Catch2
```cpp
#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>

// Behavior-driven testing
TEST_CASE("VulkanRenderer", "[rendering][vulkan]") {
    GIVEN("A valid Vulkan device") {
        auto device = createTestVulkanDevice();
        auto renderer = VulkanRenderer{device.get()};
        
        WHEN("Rendering a simple mesh") {
            auto mesh = createTestMesh();
            auto renderResult = renderer.render(mesh);
            
            THEN("The render succeeds") {
                REQUIRE(renderResult.has_value());
                REQUIRE(renderResult->commandsSubmitted > 0);
            }
        }
        
        WHEN("Rendering with invalid mesh") {
            Mesh invalidMesh{};
            auto renderResult = renderer.render(invalidMesh);
            
            THEN("The render fails gracefully") {
                REQUIRE(!renderResult.has_value());
                REQUIRE(renderResult.error() == RenderError::InvalidMesh);
            }
        }
    }
}

// Performance benchmarking
TEST_CASE("Transform System Performance", "[performance][ecs]") {
    auto world = createTestWorld();
    populateWithEntities(world, 100000);
    auto transformSystem = TransformSystem{};
    
    BENCHMARK("Update 100k transforms") {
        return transformSystem.update(world, 0.016f);
    };
}
```

### Property-Based Testing
```cpp
// Use Hypothesis-style testing for complex algorithms
TEST_CASE("Frustum culling properties", "[rendering][property]") {
    auto camera = Camera{};
    
    SECTION("Objects inside frustum are never culled") {
        for (int i = 0; i < 1000; ++i) {
            auto position = generateRandomPositionInsideFrustum(camera);
            auto object = RenderableObject{position};
            
            REQUIRE(camera.isInsideFrustum(object));
            REQUIRE_FALSE(shouldCull(camera, object));
        }
    }
    
    SECTION("Objects outside frustum are always culled") {
        for (int i = 0; i < 1000; ++i) {
            auto position = generateRandomPositionOutsideFrustum(camera);
            auto object = RenderableObject{position};
            
            REQUIRE_FALSE(camera.isInsideFrustum(object));
            REQUIRE(shouldCull(camera, object));
        }
    }
}
```

### GPU Testing Infrastructure
```cpp
class GPUTestFixture {
protected:
    std::unique_ptr<RenderDevice> m_device;
    std::unique_ptr<CommandPool> m_commandPool;
    
    void SetUp() {
        m_device = createHeadlessRenderDevice();  // No display required
        m_commandPool = m_device->createCommandPool();
    }
    
    void TearDown() {
        m_device->waitIdle();
        m_commandPool.reset();
        m_device.reset();
    }
    
    // Helper for GPU resource validation
    template<typename ResourceType>
    bool validateGPUResource(const ResourceType& resource) {
        // Check resource is properly created on GPU
        return resource.getHandle() != nullptr && 
               resource.isValid() && 
               m_device->isResourceValid(resource);
    }
};
```

## Build System & Toolchain

### CMake Modern Practices
```cmake
# CMakeLists.txt - Modern CMake 3.25+
cmake_minimum_required(VERSION 3.25)

project(GraphicsEngine
    VERSION 1.0.0
    DESCRIPTION "Next-generation graphics engine"
    LANGUAGES CXX
)

# C++23 with modules support
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable modern features
set(CMAKE_CXX_MODULES ON)
set(CMAKE_EXPERIMENTAL_CXX_MODULE_CMAKE_API ON)

# Library target with modules
add_library(Engine)
target_sources(Engine
    PUBLIC FILE_SET CXX_MODULES FILES
        src/Core.cppm
        src/Rendering.cppm
        src/ECS.cppm
    PRIVATE
        src/VulkanRenderer.cpp
        src/EntitySystem.cpp
)

# Link dependencies
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

target_link_libraries(Engine
    PUBLIC
        Vulkan::Vulkan
        glfw
        glm::glm
)

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(Engine PRIVATE
        -march=native
        -ffast-math
        -fno-exceptions  # Optional: if not using exceptions
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(Engine PRIVATE
        -march=native
        -ffast-math
        -flto=auto
    )
endif()
```

### Continuous Integration
```yaml
# .github/workflows/ci.yml
name: Engine CI

on: [push, pull_request]

jobs:
  build-and-test:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        compiler: [clang, gcc, msvc]
        build-type: [Debug, Release]
        
    steps:
    - uses: actions/checkout@v4
    
    - name: Setup Vulkan SDK
      uses: humbletim/setup-vulkan-sdk@v1.2.0
      
    - name: Configure CMake
      run: |
        cmake -B build \
              -DCMAKE_BUILD_TYPE=${{ matrix.build-type }} \
              -DCMAKE_CXX_COMPILER=${{ matrix.compiler }}
              
    - name: Build
      run: cmake --build build --parallel
      
    - name: Test
      run: ctest --test-dir build --parallel --output-on-failure
      
    - name: Benchmark
      if: matrix.build-type == 'Release'
      run: ./build/tests/benchmarks --reporter json > benchmark-results.json
```

### Development Tooling
```json
// .clangd configuration
{
  "CompileFlags": {
    "Add": [
      "-std=c++23",
      "-Wall", 
      "-Wextra",
      "-Wpedantic"
    ]
  },
  "InlayHints": {
    "Enabled": true,
    "ParameterNames": true,
    "DeducedTypes": true
  },
  "Hover": {
    "ShowAKA": true
  }
}
```

## Future Roadmap

### Planned Advanced Features
- **C++26 Reflection**: Automatic serialization and editor integration
- **Coroutines**: Async asset loading and scripting system
- **Networking**: Distributed rendering and cloud compute
- **Machine Learning**: AI-driven LOD and rendering optimization
- **WebAssembly**: Browser deployment target

### Performance Targets
- **60+ FPS** at 4K resolution with complex scenes
- **<1ms** frame time variance for VR compatibility  
- **<100MB** memory overhead for engine core
- **Multi-million** objects with GPU-driven culling

---

> **"Clean code is not written by following a set of rules. Clean code is written by programmers who care."** - Robert C. Martin

*This living document evolves with cutting-edge practices and community feedback. Last updated: 2025*
