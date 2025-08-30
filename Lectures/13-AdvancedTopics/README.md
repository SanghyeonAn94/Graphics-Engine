# 12. Basic Rendering Pipeline

## 학습 목표
- Forward vs Deferred 렌더링 파이프라인 구현
- 기본 조명 모델과 셰이딩 기법 적용
- Depth Testing과 Culling 최적화
- 텍스처 관리와 샘플링 시스템 구축

## 강의 내용

### 12.1 렌더링 파이프라인 아키텍처
- **Forward Rendering**: 단순하고 투명도 처리에 유리
- **Deferred Rendering**: 다수의 광원 처리에 최적화
- **Forward+ / Tiled Forward**: 현대적 하이브리드 접근법
- **Clustered Deferred**: 3D 타일링을 통한 확장성

### 12.2 Forward 렌더링 구현
```cpp
// Forward 렌더링 패스
class ForwardRenderPass {
private:
    struct PerFrameData {
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
        glm::mat4 viewProjectionMatrix;
        glm::vec3 cameraPosition;
        uint32_t lightCount;
    };
    
    struct LightData {
        glm::vec3 position;
        float radius;
        glm::vec3 color;
        float intensity;
        glm::vec3 direction; // directional/spot lights
        float spotAngle;
    };
    
    std::unique_ptr<Buffer> perFrameBuffer;
    std::unique_ptr<Buffer> lightBuffer;
    std::unique_ptr<Pipeline> forwardPipeline;
    
public:
    void initialize(RenderDevice* device) {
        // Uniform 버퍼 생성
        BufferDesc bufferDesc{};
        bufferDesc.size = sizeof(PerFrameData);
        bufferDesc.usage = BufferUsage::Uniform;
        bufferDesc.memoryType = MemoryType::HostVisible;
        perFrameBuffer = device->createBuffer(bufferDesc);
        
        bufferDesc.size = sizeof(LightData) * MAX_LIGHTS;
        lightBuffer = device->createBuffer(bufferDesc);
        
        // 파이프라인 생성
        createForwardPipeline(device);
    }
    
    void render(RenderContext* context, const Camera& camera, 
               const std::vector<RenderableObject>& objects,
               const std::vector<Light>& lights) {
        
        // 카메라 데이터 업데이트
        updatePerFrameData(camera, lights);
        
        // 파이프라인 바인딩
        context->bindPipeline(forwardPipeline.get());
        context->bindUniformBuffer(0, perFrameBuffer.get());
        context->bindUniformBuffer(1, lightBuffer.get());
        
        // 객체별 렌더링
        for (const auto& object : objects) {
            // 모델 매트릭스 업데이트
            struct PerObjectData {
                glm::mat4 modelMatrix;
                glm::mat4 normalMatrix;
            } objectData;
            
            objectData.modelMatrix = object.transform.getMatrix();
            objectData.normalMatrix = glm::transpose(glm::inverse(objectData.modelMatrix));
            
            // Push constants로 전송
            context->pushConstants(&objectData, sizeof(objectData));
            
            // 머티리얼 바인딩
            bindMaterial(context, object.material);
            
            // 메시 렌더링
            context->drawIndexed(object.mesh->getIndexCount());
        }
    }
    
private:
    void updatePerFrameData(const Camera& camera, const std::vector<Light>& lights) {
        PerFrameData frameData{};
        frameData.viewMatrix = camera.getViewMatrix();
        frameData.projectionMatrix = camera.getProjectionMatrix();
        frameData.viewProjectionMatrix = frameData.projectionMatrix * frameData.viewMatrix;
        frameData.cameraPosition = camera.getPosition();
        frameData.lightCount = std::min(static_cast<uint32_t>(lights.size()), MAX_LIGHTS);
        
        perFrameBuffer->update(&frameData, sizeof(frameData));
        
        // 라이트 데이터 업데이트
        std::vector<LightData> lightData;
        for (size_t i = 0; i < frameData.lightCount; ++i) {
            LightData light{};
            light.position = lights[i].position;
            light.color = lights[i].color;
            light.intensity = lights[i].intensity;
            light.radius = lights[i].radius;
            
            lightData.push_back(light);
        }
        
        lightBuffer->update(lightData.data(), lightData.size() * sizeof(LightData));
    }
};
```

### 12.3 Deferred 렌더링 구현
```cpp
// G-Buffer 레이아웃 정의
struct GBufferData {
    glm::vec3 albedo;        // RGB
    float metallic;          // A
    
    glm::vec3 normal;        // RGB (world space)
    float roughness;         // A
    
    glm::vec3 worldPosition; // RGB
    float ao;                // A (ambient occlusion)
    
    uint32_t materialId;     // 머티리얼 ID
    uint32_t objectId;       // 객체 ID (picking용)
};

class DeferredRenderPass {
private:
    // G-Buffer 텍스처들
    std::unique_ptr<Texture> albedoTexture;      // RGBA8
    std::unique_ptr<Texture> normalTexture;      // RGBA16F
    std::unique_ptr<Texture> positionTexture;    // RGBA32F
    std::unique_ptr<Texture> depthTexture;       // D32F
    
    std::unique_ptr<Pipeline> geometryPipeline;  // G-Buffer 채우기
    std::unique_ptr<Pipeline> lightingPipeline;  // 조명 계산
    
    std::unique_ptr<Mesh> fullscreenQuad;        // 전체화면 쿼드
    
public:
    void initialize(RenderDevice* device, uint32_t width, uint32_t height) {
        createGBufferTextures(device, width, height);
        createPipelines(device);
        createFullscreenQuad(device);
    }
    
    void render(RenderContext* context, const Camera& camera,
               const std::vector<RenderableObject>& objects,
               const std::vector<Light>& lights) {
        
        // 1단계: Geometry Pass - G-Buffer 채우기
        geometryPass(context, camera, objects);
        
        // 2단계: Lighting Pass - 조명 계산
        lightingPass(context, camera, lights);
    }
    
private:
    void geometryPass(RenderContext* context, const Camera& camera,
                     const std::vector<RenderableObject>& objects) {
        
        // G-Buffer를 렌더 타겟으로 설정
        std::vector<Texture*> renderTargets = {
            albedoTexture.get(),
            normalTexture.get(),
            positionTexture.get()
        };
        
        context->beginRenderPass(renderTargets, depthTexture.get());
        
        // G-Buffer 클리어
        context->clearRenderTargets();
        
        // 지오메트리 파이프라인 바인딩
        context->bindPipeline(geometryPipeline.get());
        
        // 카메라 데이터 바인딩
        updateCameraData(camera);
        
        // 객체들 렌더링
        for (const auto& object : objects) {
            // 모델 매트릭스와 머티리얼 정보 전송
            struct GeometryPushConstants {
                glm::mat4 modelMatrix;
                glm::mat4 normalMatrix;
                uint32_t materialId;
            } pushConstants;
            
            pushConstants.modelMatrix = object.transform.getMatrix();
            pushConstants.normalMatrix = glm::transpose(glm::inverse(pushConstants.modelMatrix));
            pushConstants.materialId = object.materialId;
            
            context->pushConstants(&pushConstants, sizeof(pushConstants));
            
            // 머티리얼 텍스처 바인딩
            bindMaterialTextures(context, object.material);
            
            // 메시 렌더링
            context->drawMesh(object.mesh);
        }
        
        context->endRenderPass();
    }
    
    void lightingPass(RenderContext* context, const Camera& camera,
                     const std::vector<Light>& lights) {
        
        // 백 버퍼로 렌더링
        context->beginRenderPass();
        
        // 라이팅 파이프라인 바인딩
        context->bindPipeline(lightingPipeline.get());
        
        // G-Buffer 텍스처들을 입력으로 바인딩
        context->bindTexture(0, albedoTexture.get());
        context->bindTexture(1, normalTexture.get());
        context->bindTexture(2, positionTexture.get());
        context->bindTexture(3, depthTexture.get());
        
        // 조명 데이터 업데이트
        updateLightData(lights);
        
        // 전체화면 쿼드 렌더링
        context->drawMesh(fullscreenQuad.get());
        
        context->endRenderPass();
    }
};
```

### 12.4 조명 모델 구현
```hlsl
// PBR 기반 조명 계산 (HLSL)
struct Material {
    float3 albedo;
    float metallic;
    float roughness;
    float ao;
    float3 normal;
    float3 worldPos;
};

struct Light {
    float3 position;
    float3 color;
    float intensity;
    float radius;
    int type; // 0: point, 1: directional, 2: spot
};

// Cook-Torrance BRDF 구현
float3 calculatePBRLighting(Material material, Light light, float3 viewDir) {
    float3 lightDir;
    float attenuation = 1.0;
    
    if (light.type == 0) { // Point light
        lightDir = normalize(light.position - material.worldPos);
        float distance = length(light.position - material.worldPos);
        attenuation = 1.0 / (1.0 + 0.09 * distance + 0.032 * distance * distance);
    } else if (light.type == 1) { // Directional light
        lightDir = normalize(-light.position); // position은 방향으로 사용
    }
    
    float3 halfwayDir = normalize(lightDir + viewDir);
    float3 radiance = light.color * light.intensity * attenuation;
    
    // Cook-Torrance BRDF
    float NDF = distributionGGX(material.normal, halfwayDir, material.roughness);
    float G = geometrySmith(material.normal, viewDir, lightDir, material.roughness);
    float3 F = fresnelSchlick(max(dot(halfwayDir, viewDir), 0.0), 
                             lerp(float3(0.04), material.albedo, material.metallic));
    
    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(material.normal, viewDir), 0.0) * 
                           max(dot(material.normal, lightDir), 0.0) + 0.001;
    float3 specular = numerator / denominator;
    
    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= 1.0 - material.metallic;
    
    float NdotL = max(dot(material.normal, lightDir), 0.0);
    
    return (kD * material.albedo / PI + specular) * radiance * NdotL;
}

// Normal Distribution Function
float distributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}

// Geometry Function
float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    
    return num / denom;
}

float geometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    
    return ggx1 * ggx2;
}

// Fresnel Equation
float3 fresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
```

### 12.5 Culling과 최적화
```cpp
// Frustum Culling 구현
class FrustumCuller {
private:
    struct Plane {
        glm::vec3 normal;
        float distance;
        
        float distanceToPoint(const glm::vec3& point) const {
            return glm::dot(normal, point) + distance;
        }
    };
    
    std::array<Plane, 6> frustumPlanes; // Left, Right, Bottom, Top, Near, Far
    
public:
    void updateFrustum(const Camera& camera) {
        const glm::mat4& viewProjection = camera.getViewProjectionMatrix();
        
        // 절두체 평면 추출 (Gribb/Hartmann 방법)
        // Left plane
        frustumPlanes[0].normal.x = viewProjection[0][3] + viewProjection[0][0];
        frustumPlanes[0].normal.y = viewProjection[1][3] + viewProjection[1][0];
        frustumPlanes[0].normal.z = viewProjection[2][3] + viewProjection[2][0];
        frustumPlanes[0].distance = viewProjection[3][3] + viewProjection[3][0];
        
        // Right plane
        frustumPlanes[1].normal.x = viewProjection[0][3] - viewProjection[0][0];
        frustumPlanes[1].normal.y = viewProjection[1][3] - viewProjection[1][0];
        frustumPlanes[1].normal.z = viewProjection[2][3] - viewProjection[2][0];
        frustumPlanes[1].distance = viewProjection[3][3] - viewProjection[3][0];
        
        // Bottom, Top, Near, Far planes... (비슷한 방식)
        
        // 평면 정규화
        for (auto& plane : frustumPlanes) {
            float length = glm::length(plane.normal);
            plane.normal /= length;
            plane.distance /= length;
        }
    }
    
    bool isAABBVisible(const AABB& aabb) const {
        for (const auto& plane : frustumPlanes) {
            // AABB의 positive vertex 찾기
            glm::vec3 positiveVertex = aabb.min;
            if (plane.normal.x >= 0) positiveVertex.x = aabb.max.x;
            if (plane.normal.y >= 0) positiveVertex.y = aabb.max.y;
            if (plane.normal.z >= 0) positiveVertex.z = aabb.max.z;
            
            // 평면 뒤쪽에 있으면 컬링
            if (plane.distanceToPoint(positiveVertex) < 0) {
                return false;
            }
        }
        return true;
    }
    
    std::vector<RenderableObject> cullObjects(const std::vector<RenderableObject>& objects) const {
        std::vector<RenderableObject> visibleObjects;
        
        for (const auto& object : objects) {
            if (isAABBVisible(object.boundingBox)) {
                visibleObjects.push_back(object);
            }
        }
        
        return visibleObjects;
    }
};

// Occlusion Culling (간단한 구현)
class OcclusionCuller {
private:
    std::unique_ptr<Buffer> queryBuffer;
    std::vector<uint32_t> queryResults;
    
public:
    void initialize(RenderDevice* device) {
        // Occlusion query 버퍼 생성
        BufferDesc bufferDesc{};
        bufferDesc.size = sizeof(uint32_t) * MAX_QUERIES;
        bufferDesc.usage = BufferUsage::QueryBuffer;
        queryBuffer = device->createBuffer(bufferDesc);
    }
    
    void performOcclusionTest(RenderContext* context, 
                             const std::vector<RenderableObject>& objects) {
        // 1단계: Depth-only 패스로 오클루더들 렌더링
        renderOccluders(context, objects);
        
        // 2단계: 바운딩 박스들에 대해 occlusion query 수행
        performQueries(context, objects);
        
        // 3단계: 결과 수집
        collectResults();
    }
    
private:
    void renderOccluders(RenderContext* context, const std::vector<RenderableObject>& objects) {
        // depth-only 파이프라인 사용하여 큰 객체들 먼저 렌더링
        // 컬러 쓰기 비활성화, depth 테스트만 수행
    }
    
    void performQueries(RenderContext* context, const std::vector<RenderableObject>& objects) {
        // 각 객체의 바운딩 박스에 대해 occlusion query 실행
        // GPU에서 가시성 판단
    }
    
    void collectResults() {
        // Query 결과를 CPU로 읽어와서 다음 프레임에서 사용
    }
};
```

### 12.6 텍스처 관리 시스템
```cpp
// 텍스처 관리자
class TextureManager {
private:
    struct TextureEntry {
        std::unique_ptr<Texture> texture;
        std::string filePath;
        uint32_t refCount;
        bool isResident; // GPU 메모리에 상주 여부
    };
    
    std::unordered_map<std::string, TextureEntry> loadedTextures;
    RenderDevice* device;
    
    // Streaming 관련
    std::queue<std::string> loadingQueue;
    std::thread streamingThread;
    std::atomic<bool> streamingActive{false};
    
public:
    TextureManager(RenderDevice* device) : device(device) {
        startStreaming();
    }
    
    std::shared_ptr<Texture> loadTexture(const std::string& filePath) {
        if (auto it = loadedTextures.find(filePath); it != loadedTextures.end()) {
            it->second.refCount++;
            return std::shared_ptr<Texture>(it->second.texture.get(), 
                                          [this, filePath](Texture*) { 
                                              releaseTexture(filePath); 
                                          });
        }
        
        // 새로운 텍스처 로딩
        auto texture = loadTextureFromFile(filePath);
        if (texture) {
            TextureEntry entry{};
            entry.texture = std::move(texture);
            entry.filePath = filePath;
            entry.refCount = 1;
            entry.isResident = true;
            
            loadedTextures[filePath] = std::move(entry);
            
            return std::shared_ptr<Texture>(loadedTextures[filePath].texture.get(),
                                          [this, filePath](Texture*) { 
                                              releaseTexture(filePath); 
                                          });
        }
        
        return nullptr;
    }
    
private:
    std::unique_ptr<Texture> loadTextureFromFile(const std::string& filePath) {
        // stb_image를 사용한 이미지 로딩
        int width, height, channels;
        unsigned char* data = stbi_load(filePath.c_str(), &width, &height, &channels, 4);
        
        if (!data) {
            return nullptr;
        }
        
        TextureDesc desc{};
        desc.width = width;
        desc.height = height;
        desc.format = Format::R8G8B8A8_UNORM;
        desc.usage = TextureUsage::ShaderResource;
        desc.generateMipmaps = true;
        
        auto texture = device->createTexture(desc);
        texture->update(data, width * height * 4);
        
        stbi_image_free(data);
        
        // 밉맵 생성
        if (desc.generateMipmaps) {
            generateMipmaps(texture.get());
        }
        
        return texture;
    }
    
    void generateMipmaps(Texture* texture) {
        // GPU에서 밉맵 생성
        auto commandList = device->createCommandList();
        commandList->begin();
        commandList->generateMipmaps(texture);
        commandList->end();
        device->executeCommandList(commandList.get());
    }
    
    void releaseTexture(const std::string& filePath) {
        if (auto it = loadedTextures.find(filePath); it != loadedTextures.end()) {
            if (--it->second.refCount == 0) {
                loadedTextures.erase(it);
            }
        }
    }
};
```

## 실습 과제

### Phase 1: Forward 렌더링
1. **기본 Forward 파이프라인 구현**
2. **Phong/Blinn-Phong 조명 모델**
3. **기본 텍스처링**

### Phase 2: Deferred 렌더링  
1. **G-Buffer 레이아웃 설계**
2. **Geometry/Lighting 패스 분리**
3. **다중 광원 처리**

### Phase 3: PBR 조명
1. **Cook-Torrance BRDF 구현**
2. **Image-Based Lighting 기초**
3. **성능 최적화**

## 성능 최적화 기법
- **Depth Pre-pass**: Z-rejection 최적화
- **Early-Z Testing**: 픽셀 셰이더 최적화
- **Instanced Rendering**: 동일 메시 배치 최적화
- **Texture Atlasing**: 드로우 콜 감소

## 참고 자료
- "Real-Time Rendering 4th Edition"
- "Physically Based Rendering" (Pharr, Jakob, Humphreys)
- "OpenGL SuperBible 7th Edition"
- GPU Gems 시리즈