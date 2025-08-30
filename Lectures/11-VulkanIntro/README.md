# 10. Vulkan Deep Dive

## 학습 목표
- Vulkan API의 핵심 개념과 철학 이해
- 현대적인 Vulkan 1.3 기능 활용
- Command Buffer와 Synchronization 마스터
- Bindless 렌더링과 Dynamic Rendering 구현

## 강의 내용

### 10.1 Vulkan의 철학과 설계
- 명시적 제어 vs 자동화
- CPU/GPU 병렬성 극대화
- 드라이버 오버헤드 최소화
- 멀티스레딩 친화적 설계

### 10.2 Vulkan 초기화 과정
```cpp
// Modern Vulkan 1.3 initialization
class VulkanDevice {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue graphicsQueue;
    
public:
    bool initialize() {
        createInstance();
        selectPhysicalDevice();
        createLogicalDevice();
        setupQueues();
        return true;
    }
};
```

### 10.3 Command Buffer와 Recording
```cpp
// Command buffer recording pattern
void VulkanRenderer::recordCommands() {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    
    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    // Dynamic Rendering (Vulkan 1.3)
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    
    vkCmdBeginRendering(commandBuffer, &renderingInfo);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    vkCmdEndRendering(commandBuffer);
    
    vkEndCommandBuffer(commandBuffer);
}
```

### 10.4 Pipeline State Objects
- Graphics Pipeline 생성과 관리
- Shader Stages와 Vertex Input
- Render State 설정
- Pipeline Cache 활용

### 10.5 Descriptor Sets와 Bindless
```cpp
// Bindless descriptor management
class BindlessDescriptorManager {
private:
    VkDescriptorPool pool;
    VkDescriptorSet bindlessSet;
    std::vector<VkDescriptorImageInfo> textureDescriptors;
    
public:
    uint32_t addTexture(VkImageView imageView, VkSampler sampler) {
        uint32_t index = textureDescriptors.size();
        textureDescriptors.push_back({sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        updateDescriptorSet(index);
        return index;
    }
};
```

### 10.6 Memory Management
- VMA (Vulkan Memory Allocator) 활용
- Buffer와 Image 생성 패턴
- Staging Buffer를 통한 데이터 전송

### 10.7 Synchronization Primitives
```cpp
// Timeline Semaphore (Vulkan 1.2+)
class TimelineSemaphore {
    VkSemaphore semaphore;
    uint64_t currentValue = 0;
    
public:
    void signal(uint64_t value);
    void wait(uint64_t value);
    uint64_t getCurrentValue();
};
```

## 실습 과제

### Phase 1: 기본 Vulkan 설정
1. **Instance와 Device 생성**
   - Validation layers 활성화
   - 필요한 extensions 요청
   - Physical device 선택 로직

2. **Swapchain 생성**
   - Surface format 선택
   - Present mode 설정
   - Image count 결정

### Phase 2: 첫 번째 렌더링
1. **Render Pass 없는 렌더링 (Dynamic Rendering)**
2. **Vertex Buffer 생성과 바인딩**
3. **간단한 Vertex/Fragment Shader**

### Phase 3: 고급 기능
1. **Bindless Textures 구현**
2. **Multi-draw Indirect**
3. **GPU Timestamps와 성능 측정**

## 성능 고려사항
- Command Buffer 재사용 패턴
- Descriptor Set 할당 전략
- Pipeline State 변경 최소화
- GPU/CPU 동기화 최적화

## 디버깅과 프로파일링
- Validation Layers 활용
- RenderDoc 통합
- GPU 메모리 사용량 모니터링
- Timeline 프로파일링

## 참고 자료
- Vulkan Specification 1.3
- Sascha Willems Vulkan Examples
- GPUOpen Vulkan Samples
- "Learning Vulkan" (Parminder Singh)