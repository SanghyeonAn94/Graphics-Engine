# 09. Graphics API Abstraction

## 학습 목표
- 여러 그래픽스 API를 통합하는 추상화 레이어 설계
- Command Buffer 패턴과 렌더링 아키텍처 구현
- Resource Management와 생명주기 관리
- 동기화와 성능 최적화 전략 수립

## 강의 내용

### 9.1 그래픽스 API 비교 분석
- **Vulkan**: 최대 성능, 명시적 제어, 복잡성 높음
- **DirectX 12**: Windows 플랫폼 특화, Xbox 지원
- **Metal**: Apple 생태계 최적화
- **OpenGL**: 레거시 지원, 단순함
- **WebGPU**: 웹 플랫폼, 미래 표준

### 9.2 추상화 레이어 설계
```cpp
// 그래픽스 디바이스 인터페이스
class RenderDevice {
public:
    virtual ~RenderDevice() = default;
    
    // 초기화
    virtual bool initialize(const DeviceCreateInfo& createInfo) = 0;
    virtual void shutdown() = 0;
    
    // 리소스 생성
    virtual std::unique_ptr<Buffer> createBuffer(const BufferDesc& desc) = 0;
    virtual std::unique_ptr<Texture> createTexture(const TextureDesc& desc) = 0;
    virtual std::unique_ptr<Shader> createShader(const ShaderDesc& desc) = 0;
    virtual std::unique_ptr<Pipeline> createPipeline(const PipelineDesc& desc) = 0;
    
    // 커맨드 인터페이스
    virtual std::unique_ptr<CommandList> createCommandList() = 0;
    virtual void executeCommandList(CommandList* commandList) = 0;
    virtual void waitForIdle() = 0;
    
    // 프레임 관리
    virtual void beginFrame() = 0;
    virtual void endFrame() = 0;
    virtual void present() = 0;
    
    virtual RenderAPI getAPI() const = 0;
};

// 리소스 기본 클래스
class Resource {
protected:
    std::string name;
    uint32_t refCount = 1;
    
public:
    Resource(std::string name) : name(std::move(name)) {}
    virtual ~Resource() = default;
    
    const std::string& getName() const { return name; }
    
    void addRef() { ++refCount; }
    void release() { 
        if (--refCount == 0) {
            delete this;
        }
    }
    
    uint32_t getRefCount() const { return refCount; }
};
```

### 9.3 Buffer 관리 시스템
```cpp
// 버퍼 타입과 사용법 정의
enum class BufferUsage {
    Vertex = 1 << 0,
    Index = 1 << 1,
    Uniform = 1 << 2,
    Storage = 1 << 3,
    Indirect = 1 << 4,
    TransferSrc = 1 << 5,
    TransferDst = 1 << 6
};

enum class MemoryType {
    DeviceLocal,    // GPU 전용 메모리
    HostVisible,    // CPU에서 접근 가능
    HostCached,     // CPU 캐시된 메모리
    HostCoherent    // 일관성 보장된 메모리
};

struct BufferDesc {
    size_t size;
    BufferUsage usage;
    MemoryType memoryType;
    std::string debugName;
};

// 추상 버퍼 클래스
class Buffer : public Resource {
protected:
    BufferDesc desc;
    
public:
    Buffer(const BufferDesc& desc) : Resource(desc.debugName), desc(desc) {}
    
    virtual void* map() = 0;
    virtual void unmap() = 0;
    virtual void update(const void* data, size_t size, size_t offset = 0) = 0;
    
    size_t getSize() const { return desc.size; }
    BufferUsage getUsage() const { return desc.usage; }
    MemoryType getMemoryType() const { return desc.memoryType; }
    
    virtual void* getNativeHandle() const = 0;
};

// Vulkan 구현 예시
class VulkanBuffer : public Buffer {
private:
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = nullptr;
    VmaAllocator allocator;
    void* mappedPtr = nullptr;
    
public:
    VulkanBuffer(const BufferDesc& desc, VmaAllocator alloc) 
        : Buffer(desc), allocator(alloc) {
        
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.size;
        bufferInfo.usage = convertUsage(desc.usage);
        
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = convertMemoryType(desc.memoryType);
        
        vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, 
                       &buffer, &allocation, nullptr);
    }
    
    ~VulkanBuffer() {
        if (mappedPtr) {
            vmaUnmapMemory(allocator, allocation);
        }
        vmaDestroyBuffer(allocator, buffer, allocation);
    }
    
    void* map() override {
        if (!mappedPtr) {
            vmaMapMemory(allocator, allocation, &mappedPtr);
        }
        return mappedPtr;
    }
    
    void unmap() override {
        if (mappedPtr) {
            vmaUnmapMemory(allocator, allocation);
            mappedPtr = nullptr;
        }
    }
    
    void update(const void* data, size_t size, size_t offset) override {
        void* mapped = map();
        if (mapped) {
            std::memcpy(static_cast<char*>(mapped) + offset, data, size);
            unmap();
        }
    }
    
    void* getNativeHandle() const override { return buffer; }
    VkBuffer getVkBuffer() const { return buffer; }
};
```

### 9.4 Command Buffer 패턴
```cpp
// 렌더링 명령들을 기록하는 인터페이스
class CommandList {
public:
    virtual ~CommandList() = default;
    
    // 상태 관리
    virtual void begin() = 0;
    virtual void end() = 0;
    virtual void reset() = 0;
    
    // 렌더 패스
    virtual void beginRenderPass(const RenderPassBeginInfo& beginInfo) = 0;
    virtual void endRenderPass() = 0;
    
    // 파이프라인 바인딩
    virtual void bindPipeline(Pipeline* pipeline) = 0;
    virtual void bindVertexBuffer(Buffer* buffer, size_t offset = 0) = 0;
    virtual void bindIndexBuffer(Buffer* buffer, IndexType type, size_t offset = 0) = 0;
    virtual void bindDescriptorSet(DescriptorSet* descriptorSet, uint32_t setIndex) = 0;
    
    // 드로우 콜
    virtual void draw(uint32_t vertexCount, uint32_t instanceCount = 1, 
                     uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
    virtual void drawIndexed(uint32_t indexCount, uint32_t instanceCount = 1,
                           uint32_t firstIndex = 0, int32_t vertexOffset = 0, 
                           uint32_t firstInstance = 0) = 0;
    virtual void drawIndirect(Buffer* buffer, size_t offset, uint32_t drawCount, uint32_t stride) = 0;
    
    // 컴퓨트
    virtual void dispatch(uint32_t x, uint32_t y, uint32_t z) = 0;
    virtual void dispatchIndirect(Buffer* buffer, size_t offset) = 0;
    
    // 리소스 전환
    virtual void transitionTexture(Texture* texture, ResourceState oldState, ResourceState newState) = 0;
    virtual void copyBuffer(Buffer* src, Buffer* dst, const BufferCopy& copyInfo) = 0;
    virtual void copyTexture(Texture* src, Texture* dst, const TextureCopy& copyInfo) = 0;
    
    // 동기화
    virtual void barrier(const MemoryBarrier& barrier) = 0;
    virtual void textureBarrier(const TextureBarrier& barrier) = 0;
    
    // 디버깅
    virtual void pushDebugGroup(const std::string& name) = 0;
    virtual void popDebugGroup() = 0;
    virtual void insertDebugMarker(const std::string& name) = 0;
};

// 렌더링 컨텍스트 - 고수준 인터페이스
class RenderContext {
private:
    std::unique_ptr<CommandList> commandList;
    RenderDevice* device;
    
    // 상태 추적
    Pipeline* currentPipeline = nullptr;
    std::array<Buffer*, 8> vertexBuffers{};
    Buffer* indexBuffer = nullptr;
    
public:
    RenderContext(RenderDevice* device) : device(device) {
        commandList = device->createCommandList();
    }
    
    void begin() {
        commandList->begin();
    }
    
    void end() {
        commandList->end();
    }
    
    void submit() {
        device->executeCommandList(commandList.get());
    }
    
    // 고수준 렌더링 함수들
    void drawMesh(const Mesh& mesh, const Material& material, const glm::mat4& transform) {
        // 파이프라인 바인딩
        if (currentPipeline != material.getPipeline()) {
            currentPipeline = material.getPipeline();
            commandList->bindPipeline(currentPipeline);
        }
        
        // 버퍼 바인딩
        commandList->bindVertexBuffer(mesh.getVertexBuffer());
        commandList->bindIndexBuffer(mesh.getIndexBuffer(), IndexType::UInt32);
        
        // 머티리얼 바인딩
        commandList->bindDescriptorSet(material.getDescriptorSet(), 0);
        
        // Transform 업데이트 (Push Constants 사용)
        struct PushConstants {
            glm::mat4 mvp;
        } pushConstants;
        pushConstants.mvp = transform;
        
        // 드로우
        commandList->drawIndexed(mesh.getIndexCount());
    }
};
```

### 9.5 Resource State Tracking
```cpp
// 리소스 상태 추적 시스템
enum class ResourceState {
    Undefined,
    Common,
    VertexBuffer,
    IndexBuffer,
    UniformBuffer,
    ShaderResource,
    UnorderedAccess,
    RenderTarget,
    DepthWrite,
    DepthRead,
    TransferSrc,
    TransferDst,
    Present
};

class ResourceStateTracker {
private:
    struct ResourceInfo {
        ResourceState currentState = ResourceState::Undefined;
        ResourceState pendingState = ResourceState::Undefined;
        bool needsTransition = false;
    };
    
    std::unordered_map<Resource*, ResourceInfo> trackedResources;
    std::vector<ResourceTransition> pendingTransitions;
    
public:
    void trackResource(Resource* resource, ResourceState initialState) {
        trackedResources[resource].currentState = initialState;
    }
    
    void transitionResource(Resource* resource, ResourceState newState) {
        auto& info = trackedResources[resource];
        
        if (info.currentState != newState) {
            info.pendingState = newState;
            info.needsTransition = true;
            
            ResourceTransition transition{};
            transition.resource = resource;
            transition.stateBefore = info.currentState;
            transition.stateAfter = newState;
            
            pendingTransitions.push_back(transition);
        }
    }
    
    void flushTransitions(CommandList* commandList) {
        if (pendingTransitions.empty()) return;
        
        // 배치로 배리어 실행
        for (const auto& transition : pendingTransitions) {
            if (transition.resource->getType() == ResourceType::Texture) {
                TextureBarrier barrier{};
                barrier.texture = static_cast<Texture*>(transition.resource);
                barrier.stateBefore = transition.stateBefore;
                barrier.stateAfter = transition.stateAfter;
                commandList->textureBarrier(barrier);
            }
            
            // 상태 업데이트
            auto& info = trackedResources[transition.resource];
            info.currentState = info.pendingState;
            info.needsTransition = false;
        }
        
        pendingTransitions.clear();
    }
    
    ResourceState getResourceState(Resource* resource) const {
        if (auto it = trackedResources.find(resource); it != trackedResources.end()) {
            return it->second.currentState;
        }
        return ResourceState::Undefined;
    }
};
```

### 9.6 플랫폼별 구현 팩토리
```cpp
// 디바이스 팩토리 패턴
class RenderDeviceFactory {
public:
    static std::unique_ptr<RenderDevice> create(RenderAPI api, const DeviceCreateInfo& createInfo) {
        switch (api) {
            case RenderAPI::Vulkan:
                return std::make_unique<VulkanDevice>(createInfo);
            case RenderAPI::DirectX12:
                return std::make_unique<D3D12Device>(createInfo);
            case RenderAPI::Metal:
                return std::make_unique<MetalDevice>(createInfo);
            case RenderAPI::OpenGL:
                return std::make_unique<OpenGLDevice>(createInfo);
            default:
                return nullptr;
        }
    }
    
    static RenderAPI selectBestAPI() {
        // 플랫폼별 최적 API 선택
#ifdef _WIN32
        if (isDirectX12Supported()) {
            return RenderAPI::DirectX12;
        }
#endif
        
#ifdef __APPLE__
        return RenderAPI::Metal;
#endif
        
        if (isVulkanSupported()) {
            return RenderAPI::Vulkan;
        }
        
        return RenderAPI::OpenGL; // 폴백
    }
    
private:
    static bool isVulkanSupported() {
        // Vulkan 지원 확인 로직
        return true; // 간단화
    }
    
    static bool isDirectX12Supported() {
        // DirectX 12 지원 확인 로직
        return true; // 간단화
    }
};

// 사용 예시
class Renderer {
private:
    std::unique_ptr<RenderDevice> device;
    std::unique_ptr<RenderContext> context;
    
public:
    bool initialize() {
        // 최적 API 선택
        RenderAPI api = RenderDeviceFactory::selectBestAPI();
        
        DeviceCreateInfo createInfo{};
        createInfo.enableValidation = true;
        createInfo.enableDebugMarkers = true;
        
        // 디바이스 생성
        device = RenderDeviceFactory::create(api, createInfo);
        if (!device || !device->initialize(createInfo)) {
            return false;
        }
        
        // 렌더링 컨텍스트 생성
        context = std::make_unique<RenderContext>(device.get());
        
        return true;
    }
    
    void render() {
        device->beginFrame();
        
        context->begin();
        
        // 렌더링 작업들...
        
        context->end();
        context->submit();
        
        device->endFrame();
        device->present();
    }
};
```

## 실습 과제

### Phase 1: 기본 추상화
1. **RenderDevice 인터페이스 구현**
2. **Buffer 추상화 및 Vulkan 구현**
3. **간단한 Command List 시스템**

### Phase 2: 고급 기능
1. **Resource State Tracking**
2. **Multi-API 지원 (Vulkan + OpenGL)**
3. **성능 최적화**

### Phase 3: 통합 테스트
1. **크로스 플랫폼 렌더링 테스트**
2. **성능 벤치마크**
3. **API별 기능 차이 처리**

## 설계 고려사항
- **성능 오버헤드**: 추상화로 인한 성능 손실 최소화
- **기능 호환성**: 각 API의 고유 기능 활용 방법
- **확장성**: 새로운 API 추가의 용이성
- **디버깅**: 멀티 API 환경에서의 디버깅 전략

## 참고 자료
- "Real-Time Rendering 4th Edition" API 비교 챕터
- Vulkan, DirectX 12, Metal 공식 문서
- "Learning DirectX 12" (Jeremiah van Oosten)
- NVIDIA/AMD GPU 최적화 가이드