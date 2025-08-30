# 10. Ray Tracing Integration

## 학습 목표
- 하드웨어 레이트레이싱의 기본 개념 이해
- Acceleration Structure (BLAS/TLAS) 구축
- RT Reflections와 Shadows 구현
- 하이브리드 렌더링 파이프라인 설계

## 강의 내용

### 10.1 레이트레이싱 기초 이론
- **Ray-Triangle Intersection**: 기본적인 교차점 계산
- **Bounding Volume Hierarchy (BVH)**: 가속 구조
- **Monte Carlo Integration**: 확률적 샘플링
- **Importance Sampling**: 효율적인 샘플링 기법

### 10.2 하드웨어 레이트레이싱 아키텍처
```cpp
// RT 파이프라인 상태 정의
struct RayTracingPipelineDesc {
    struct ShaderGroup {
        std::unique_ptr<Shader> rayGenShader;
        std::unique_ptr<Shader> missShader;
        std::unique_ptr<Shader> closestHitShader;
        std::unique_ptr<Shader> anyHitShader;
        std::unique_ptr<Shader> intersectionShader;
    };
    
    std::vector<ShaderGroup> shaderGroups;
    uint32_t maxRayRecursionDepth = 1;
    uint32_t maxPayloadSize = 32;
    uint32_t maxAttributeSize = 8;
};

// 레이트레이싱 파이프라인 클래스
class RayTracingPipeline {
private:
    RenderDevice* device;
    std::unique_ptr<Pipeline> rtPipeline;
    std::unique_ptr<Buffer> shaderBindingTable;
    
    struct ShaderRecord {
        uint8_t shaderGroupHandle[32]; // 드라이버별로 다름
        // 셰이더별 로컬 데이터
    };
    
public:
    RayTracingPipeline(RenderDevice* device) : device(device) {}
    
    bool initialize(const RayTracingPipelineDesc& desc) {
        // RT 파이프라인 생성
        rtPipeline = device->createRayTracingPipeline(desc);
        if (!rtPipeline) {
            return false;
        }
        
        // Shader Binding Table 생성
        createShaderBindingTable(desc);
        
        return true;
    }
    
    void traceRays(CommandList* commandList, uint32_t width, uint32_t height, uint32_t depth = 1) {
        commandList->bindRayTracingPipeline(rtPipeline.get());
        commandList->traceRays(shaderBindingTable.get(), width, height, depth);
    }
    
private:
    void createShaderBindingTable(const RayTracingPipelineDesc& desc) {
        // 각 셰이더 그룹의 핸들을 가져와서 SBT 구성
        size_t handleSize = device->getShaderGroupHandleSize();
        size_t recordSize = handleSize + sizeof(uint32_t); // 핸들 + 로컬 데이터
        
        size_t tableSize = desc.shaderGroups.size() * recordSize;
        
        BufferDesc bufferDesc{};
        bufferDesc.size = tableSize;
        bufferDesc.usage = BufferUsage::ShaderBindingTable;
        bufferDesc.memoryType = MemoryType::HostVisible;
        
        shaderBindingTable = device->createBuffer(bufferDesc);
        
        // SBT 데이터 채우기
        uint8_t* mappedData = static_cast<uint8_t*>(shaderBindingTable->map());
        
        for (size_t i = 0; i < desc.shaderGroups.size(); ++i) {
            uint8_t* record = mappedData + i * recordSize;
            
            // 셰이더 그룹 핸들 복사
            device->getShaderGroupHandle(rtPipeline.get(), i, record);
            
            // 로컬 데이터 (필요시 추가)
            uint32_t* localData = reinterpret_cast<uint32_t*>(record + handleSize);
            *localData = static_cast<uint32_t>(i); // 예시 데이터
        }
        
        shaderBindingTable->unmap();
    }
};
```

### 10.3 Acceleration Structure 구현
```cpp
// Bottom Level Acceleration Structure (BLAS)
class BLAS {
private:
    std::unique_ptr<Buffer> blasBuffer;
    std::unique_ptr<Buffer> scratchBuffer;
    
public:
    struct GeometryDesc {
        Buffer* vertexBuffer;
        uint32_t vertexStride;
        uint32_t vertexCount;
        Format vertexFormat;
        
        Buffer* indexBuffer;
        uint32_t indexCount;
        IndexType indexType;
        
        glm::mat4 transform; // 선택적 변환
    };
    
    bool build(RenderDevice* device, CommandList* commandList, 
               const std::vector<GeometryDesc>& geometries) {
        
        // BLAS 크기 계산
        ASBuildSizes buildSizes = device->getASBuildSizes(geometries, false);
        
        // BLAS 버퍼 생성
        BufferDesc blasDesc{};
        blasDesc.size = buildSizes.accelerationStructureSize;
        blasDesc.usage = BufferUsage::AccelerationStructure;
        blasDesc.memoryType = MemoryType::DeviceLocal;
        blasBuffer = device->createBuffer(blasDesc);
        
        // 스크래치 버퍼 생성
        BufferDesc scratchDesc{};
        scratchDesc.size = buildSizes.buildScratchSize;
        scratchDesc.usage = BufferUsage::Storage;
        scratchDesc.memoryType = MemoryType::DeviceLocal;
        scratchBuffer = device->createBuffer(scratchDesc);
        
        // BLAS 빌드 명령 기록
        ASBuildInfo buildInfo{};
        buildInfo.type = ASType::BottomLevel;
        buildInfo.flags = ASBuildFlags::PreferFastTrace;
        buildInfo.geometries = geometries;
        buildInfo.destinationAS = blasBuffer.get();
        buildInfo.scratchBuffer = scratchBuffer.get();
        
        commandList->buildAccelerationStructure(buildInfo);
        
        // 빌드 완료를 위한 배리어
        MemoryBarrier barrier{};
        barrier.srcAccessMask = AccessFlags::AccelerationStructureWrite;
        barrier.dstAccessMask = AccessFlags::AccelerationStructureRead;
        commandList->barrier(barrier);
        
        return true;
    }
    
    Buffer* getBuffer() const { return blasBuffer.get(); }
    uint64_t getDeviceAddress() const { return device->getBufferDeviceAddress(blasBuffer.get()); }
};

// Top Level Acceleration Structure (TLAS)
class TLAS {
private:
    std::unique_ptr<Buffer> tlasBuffer;
    std::unique_ptr<Buffer> instanceBuffer;
    std::unique_ptr<Buffer> scratchBuffer;
    
public:
    struct Instance {
        glm::mat3x4 transform;  // 3x4 변환 매트릭스
        uint32_t instanceId : 24;
        uint32_t mask : 8;
        uint32_t instanceShaderBindingTableRecordOffset : 24;
        uint32_t flags : 8;
        uint64_t accelerationStructureReference; // BLAS 주소
    };
    
    bool build(RenderDevice* device, CommandList* commandList,
               const std::vector<Instance>& instances) {
        
        // 인스턴스 버퍼 생성 및 업데이트
        BufferDesc instanceDesc{};
        instanceDesc.size = instances.size() * sizeof(Instance);
        instanceDesc.usage = BufferUsage::AccelerationStructureBuildInput;
        instanceDesc.memoryType = MemoryType::HostVisible;
        instanceBuffer = device->createBuffer(instanceDesc);
        instanceBuffer->update(instances.data(), instanceDesc.size);
        
        // TLAS 크기 계산
        ASBuildSizes buildSizes = device->getASBuildSizes(instances, true);
        
        // TLAS 버퍼 생성
        BufferDesc tlasDesc{};
        tlasDesc.size = buildSizes.accelerationStructureSize;
        tlasDesc.usage = BufferUsage::AccelerationStructure;
        tlasDesc.memoryType = MemoryType::DeviceLocal;
        tlasBuffer = device->createBuffer(tlasDesc);
        
        // 스크래치 버퍼 생성
        BufferDesc scratchDesc{};
        scratchDesc.size = buildSizes.buildScratchSize;
        scratchDesc.usage = BufferUsage::Storage;
        scratchDesc.memoryType = MemoryType::DeviceLocal;
        scratchBuffer = device->createBuffer(scratchDesc);
        
        // TLAS 빌드 명령 기록
        ASBuildInfo buildInfo{};
        buildInfo.type = ASType::TopLevel;
        buildInfo.flags = ASBuildFlags::PreferFastTrace;
        buildInfo.instanceBuffer = instanceBuffer.get();
        buildInfo.instanceCount = static_cast<uint32_t>(instances.size());
        buildInfo.destinationAS = tlasBuffer.get();
        buildInfo.scratchBuffer = scratchBuffer.get();
        
        commandList->buildAccelerationStructure(buildInfo);
        
        // 빌드 완료를 위한 배리어
        MemoryBarrier barrier{};
        barrier.srcAccessMask = AccessFlags::AccelerationStructureWrite;
        barrier.dstAccessMask = AccessFlags::AccelerationStructureRead;
        commandList->barrier(barrier);
        
        return true;
    }
    
    Buffer* getBuffer() const { return tlasBuffer.get(); }
};
```

### 10.4 RT Reflection 구현
```cpp
// 레이트레이스 리플렉션 패스
class RTReflectionPass {
private:
    std::unique_ptr<RayTracingPipeline> rtPipeline;
    std::unique_ptr<Texture> reflectionTexture;
    std::unique_ptr<Buffer> rayGenConstantBuffer;
    
    struct RayGenConstants {
        glm::mat4 viewInverse;
        glm::mat4 projInverse;
        glm::vec4 lightDirection;
        uint32_t frameNumber;
    };
    
public:
    bool initialize(RenderDevice* device, uint32_t width, uint32_t height) {
        // 리플렉션 결과를 저장할 텍스처 생성
        TextureDesc reflectionDesc{};
        reflectionDesc.width = width;
        reflectionDesc.height = height;
        reflectionDesc.format = Format::R16G16B16A16_SFLOAT;
        reflectionDesc.usage = TextureUsage::UnorderedAccess | TextureUsage::ShaderResource;
        reflectionTexture = device->createTexture(reflectionDesc);
        
        // Ray Generation 상수 버퍼
        BufferDesc bufferDesc{};
        bufferDesc.size = sizeof(RayGenConstants);
        bufferDesc.usage = BufferUsage::Uniform;
        bufferDesc.memoryType = MemoryType::HostVisible;
        rayGenConstantBuffer = device->createBuffer(bufferDesc);
        
        // RT 파이프라인 생성
        return createRTPipeline(device);
    }
    
    void render(CommandList* commandList, const Camera& camera, TLAS* tlas, 
                Texture* gBufferNormal, Texture* gBufferDepth, uint32_t frameNumber) {
        
        // 상수 업데이트
        updateConstants(camera, frameNumber);
        
        // 텍스처 상태 전환
        commandList->transitionTexture(reflectionTexture.get(), 
                                      ResourceState::ShaderResource, 
                                      ResourceState::UnorderedAccess);
        
        // 디스크립터 바인딩
        commandList->bindRayTracingDescriptorSet(0, createDescriptorSet(tlas, gBufferNormal, gBufferDepth));
        
        // 레이트레이싱 실행
        rtPipeline->traceRays(commandList, reflectionTexture->getWidth(), 
                             reflectionTexture->getHeight());
        
        // 결과 텍스처를 읽기 상태로 전환
        commandList->transitionTexture(reflectionTexture.get(),
                                      ResourceState::UnorderedAccess,
                                      ResourceState::ShaderResource);
    }
    
    Texture* getReflectionTexture() const { return reflectionTexture.get(); }
    
private:
    bool createRTPipeline(RenderDevice* device) {
        RayTracingPipelineDesc desc{};
        
        // Ray Generation Shader
        auto rayGenShader = device->createShader({
            .source = loadShaderSource("reflection_raygen.hlsl"),
            .stage = ShaderStage::RayGeneration,
            .entryPoint = "RayGenMain"
        });
        
        // Miss Shader
        auto missShader = device->createShader({
            .source = loadShaderSource("reflection_miss.hlsl"),
            .stage = ShaderStage::Miss,
            .entryPoint = "MissMain"
        });
        
        // Closest Hit Shader
        auto closestHitShader = device->createShader({
            .source = loadShaderSource("reflection_closesthit.hlsl"),
            .stage = ShaderStage::ClosestHit,
            .entryPoint = "ClosestHitMain"
        });
        
        // 셰이더 그룹 구성
        RayTracingPipelineDesc::ShaderGroup group{};
        group.rayGenShader = std::move(rayGenShader);
        group.missShader = std::move(missShader);
        group.closestHitShader = std::move(closestHitShader);
        
        desc.shaderGroups.push_back(std::move(group));
        desc.maxRayRecursionDepth = 1;
        desc.maxPayloadSize = 16; // float4
        
        rtPipeline = std::make_unique<RayTracingPipeline>(device);
        return rtPipeline->initialize(desc);
    }
    
    void updateConstants(const Camera& camera, uint32_t frameNumber) {
        RayGenConstants constants{};
        constants.viewInverse = glm::inverse(camera.getViewMatrix());
        constants.projInverse = glm::inverse(camera.getProjectionMatrix());
        constants.frameNumber = frameNumber;
        
        rayGenConstantBuffer->update(&constants, sizeof(constants));
    }
};
```

### 10.5 RT 셰이더 구현 (HLSL)
```hlsl
// reflection_raygen.hlsl
RaytracingAccelerationStructure SceneBVH : register(t0);
RWTexture2D<float4> ReflectionOutput : register(u0);
Texture2D<float4> GBufferNormal : register(t1);
Texture2D<float> GBufferDepth : register(t2);

cbuffer RayGenConstants : register(b0) {
    float4x4 ViewInverse;
    float4x4 ProjInverse;
    float4 LightDirection;
    uint FrameNumber;
};

struct RayPayload {
    float3 color;
    float hitDistance;
};

[shader("raygeneration")]
void RayGenMain() {
    uint3 launchIndex = DispatchRaysIndex();
    uint3 launchDim = DispatchRaysDimensions();
    
    float2 pixelCenter = float2(launchIndex.xy) + 0.5;
    float2 inUV = pixelCenter / float2(launchDim.xy);
    float2 d = inUV * 2.0 - 1.0;
    
    // G-Buffer에서 월드 공간 위치와 노말 복원
    float depth = GBufferDepth.SampleLevel(sampler, inUV, 0);
    float4 worldPos = mul(ProjInverse, float4(d, depth, 1.0));
    worldPos /= worldPos.w;
    worldPos = mul(ViewInverse, worldPos);
    
    float3 normal = GBufferNormal.SampleLevel(sampler, inUV, 0).xyz * 2.0 - 1.0;
    normal = normalize(mul((float3x3)ViewInverse, normal));
    
    // 카메라 위치
    float3 cameraPos = ViewInverse[3].xyz;
    float3 viewDir = normalize(worldPos.xyz - cameraPos);
    
    // 리플렉션 벡터 계산
    float3 reflectionDir = reflect(viewDir, normal);
    
    // 리플렉션 레이 설정
    RayDesc ray;
    ray.Origin = worldPos.xyz + normal * 0.001; // 자기 교차 방지
    ray.Direction = reflectionDir;
    ray.TMin = 0.001;
    ray.TMax = 100.0;
    
    RayPayload payload;
    payload.color = float3(0, 0, 0);
    payload.hitDistance = -1.0;
    
    // 레이트레이싱 실행
    TraceRay(SceneBVH, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 
             0xFF, 0, 1, 0, ray, payload);
    
    // 결과 저장
    ReflectionOutput[launchIndex.xy] = float4(payload.color, payload.hitDistance);
}

// reflection_closesthit.hlsl
#include "raytracing_common.hlsl"

[shader("closesthit")]
void ClosestHitMain(inout RayPayload payload, in BuiltInTriangleIntersectionAttributes attr) {
    // 히트 포인트에서의 머티리얼 정보 가져오기
    uint meshId = InstanceID();
    uint triangleId = PrimitiveIndex();
    
    // 버텍스 데이터 보간
    float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, 
                                attr.barycentrics.x, attr.barycentrics.y);
    
    // 월드 공간 위치와 노말 계산
    float3 worldPos = WorldRayOrigin() + RayTCurrent() * WorldRayDirection();
    float3 normal = getInterpolatedNormal(meshId, triangleId, barycentrics);
    
    // 간단한 조명 계산
    float3 lightDir = normalize(-LightDirection.xyz);
    float NdotL = max(dot(normal, lightDir), 0.0);
    
    // 머티리얼 색상 가져오기
    float3 materialColor = getMaterialColor(meshId, triangleId, barycentrics);
    
    payload.color = materialColor * NdotL;
    payload.hitDistance = RayTCurrent();
}

// reflection_miss.hlsl
[shader("miss")]
void MissMain(inout RayPayload payload) {
    // 스카이박스 또는 환경 색상
    payload.color = float3(0.1, 0.2, 0.4); // 간단한 하늘색
    payload.hitDistance = -1.0;
}
```

### 10.6 하이브리드 렌더링 파이프라인
```cpp
// 래스터라이제이션과 레이트레이싱 결합
class HybridRenderer {
private:
    // 전통적인 래스터라이제이션 패스들
    std::unique_ptr<ForwardRenderPass> forwardPass;
    std::unique_ptr<DeferredRenderPass> deferredPass;
    
    // 레이트레이싱 패스들
    std::unique_ptr<RTReflectionPass> rtReflectionPass;
    std::unique_ptr<RTShadowPass> rtShadowPass;
    std::unique_ptr<RTGlobalIlluminationPass> rtGIPass;
    
    // 후처리
    std::unique_ptr<DenoisePass> denoisePass;
    std::unique_ptr<TemporalAccumulationPass> temporalPass;
    
public:
    void render(RenderContext* context, const Scene& scene, const Camera& camera) {
        // 1단계: G-Buffer 패스 (래스터라이제이션)
        deferredPass->renderGBuffer(context, scene, camera);
        
        // 2단계: RT 리플렉션
        if (scene.hasReflectiveSurfaces()) {
            rtReflectionPass->render(context->getCommandList(), camera, 
                                   scene.getTLAS(), 
                                   deferredPass->getNormalTexture(),
                                   deferredPass->getDepthTexture(),
                                   getCurrentFrameNumber());
        }
        
        // 3단계: RT 섀도우 (선택적)
        if (scene.hasDynamicShadows()) {
            rtShadowPass->render(context->getCommandList(), camera, scene);
        }
        
        // 4단계: RT 글로벌 일루미네이션 (선택적)
        if (isGIEnabled()) {
            rtGIPass->render(context->getCommandList(), camera, scene);
        }
        
        // 5단계: 디노이징
        if (rtReflectionPass->getReflectionTexture()) {
            denoisePass->denoise(context->getCommandList(),
                               rtReflectionPass->getReflectionTexture(),
                               deferredPass->getNormalTexture(),
                               deferredPass->getDepthTexture());
        }
        
        // 6단계: 템포럴 누적
        temporalPass->accumulate(context->getCommandList(),
                               denoisePass->getDenoisedTexture(),
                               camera.getViewProjectionMatrix(),
                               camera.getPreviousViewProjectionMatrix());
        
        // 7단계: 최종 조명 패스
        deferredPass->renderLighting(context, camera, scene,
                                   temporalPass->getAccumulatedTexture());
    }
    
private:
    uint32_t getCurrentFrameNumber() const {
        static uint32_t frameNumber = 0;
        return frameNumber++;
    }
    
    bool isGIEnabled() const {
        // 성능에 따라 GI 활성화 여부 결정
        return getRenderQuality() >= RenderQuality::Ultra;
    }
};
```

## 실습 과제

### Phase 1: 기본 RT 구조
1. **BLAS/TLAS 구축 시스템**
2. **간단한 RT 파이프라인**
3. **기본 레이-트라이앵글 교차**

### Phase 2: RT 이펙트
1. **RT 리플렉션 구현**
2. **RT 섀도우 시스템**
3. **디노이징 기초**

### Phase 3: 고급 기능
1. **템포럴 누적**
2. **RT 글로벌 일루미네이션**
3. **성능 최적화**

## 성능 최적화
- **레이 일관성 (Ray Coherence)**: 유사한 레이들을 그룹화
- **레이 재사용**: 여러 픽셀에서 동일한 레이 활용
- **적응형 샘플링**: 노이즈에 따른 샘플 수 조정
- **하이브리드 접근법**: 래스터와 RT의 적절한 조합

## 참고 자료
- "Ray Tracing in One Weekend" 시리즈
- NVIDIA RTX Best Practices Guide
- "Real-Time Ray Tracing" (Peter Shirley)
- DirectX Raytracing (DXR) 문서