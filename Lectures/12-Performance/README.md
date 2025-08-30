# 11. Shader System & Compilation

## 학습 목표
- 현대적인 셰이더 컴파일 파이프라인 구축
- SPIR-V 기반 크로스 플랫폼 셰이더 시스템
- Shader Reflection과 자동 Pipeline 생성
- Hot-reloading과 실시간 셰이더 편집 지원

## 강의 내용

### 11.1 셰이더 언어와 컴파일 과정
- **HLSL**: DirectX 표준, 강력한 도구 지원
- **GLSL**: OpenGL 표준, 오픈소스 생태계
- **MSL**: Metal Shading Language, Apple 최적화
- **SPIR-V**: 중간 표현, 크로스 플랫폼 타겟

### 11.2 셰이더 컴파일 시스템 설계
```cpp
// 셰이더 스테이지 정의
enum class ShaderStage {
    Vertex,
    TessellationControl,
    TessellationEvaluation,
    Geometry,
    Fragment,
    Compute,
    // Mesh shading (modern)
    Task,
    Mesh,
    // Ray tracing
    RayGeneration,
    Miss,
    ClosestHit,
    AnyHit,
    Intersection
};

// 셰이더 소스 정보
struct ShaderSource {
    std::string source;
    std::string entryPoint = "main";
    ShaderStage stage;
    std::string filename;
    std::vector<std::string> includePaths;
    std::unordered_map<std::string, std::string> defines;
};

// 컴파일 결과
struct ShaderCompilationResult {
    std::vector<uint32_t> spirvBytecode;
    bool success = false;
    std::string errorMessage;
    std::vector<std::string> warnings;
    
    // Reflection 정보
    ShaderReflection reflection;
    
    // 디버그 정보
    std::string disassembly;
    std::vector<uint32_t> debugInfo;
};
```

### 11.3 SPIR-V 컴파일러 통합
```cpp
// shaderc를 활용한 HLSL/GLSL -> SPIR-V 컴파일
class ShaderCompiler {
private:
    shaderc::Compiler compiler;
    shaderc::CompileOptions baseOptions;
    
public:
    ShaderCompiler() {
        setupBaseOptions();
    }
    
    ShaderCompilationResult compile(const ShaderSource& source) {
        ShaderCompilationResult result{};
        
        // 컴파일 옵션 설정
        shaderc::CompileOptions options = baseOptions;
        
        // Define 설정
        for (const auto& [name, value] : source.defines) {
            options.AddMacroDefinition(name, value);
        }
        
        // Include path 설정
        for (const auto& includePath : source.includePaths) {
            options.AddIncludeSearchPath(includePath);
        }
        
        // 최적화 레벨 설정
        options.SetOptimizationLevel(shaderc_optimization_level_performance);
        
        // 대상 환경 설정
        options.SetTargetEnvironment(shaderc_target_env_vulkan, 
                                    shaderc_env_version_vulkan_1_3);
        options.SetTargetSpirv(shaderc_spirv_version_1_6);
        
        // 컴파일 실행
        shaderc_shader_kind shaderKind = convertStage(source.stage);
        auto compilationResult = compiler.CompileGlslToSpv(
            source.source, shaderKind, source.filename.c_str(),
            source.entryPoint.c_str(), options);
        
        // 결과 처리
        if (compilationResult.GetCompilationStatus() == shaderc_compilation_status_success) {
            result.success = true;
            result.spirvBytecode = {compilationResult.cbegin(), compilationResult.cend()};
            
            // Reflection 수행
            result.reflection = performReflection(result.spirvBytecode);
            
            // 디스어셈블리 생성 (디버그용)
            generateDisassembly(result);
        } else {
            result.success = false;
            result.errorMessage = compilationResult.GetErrorMessage();
        }
        
        return result;
    }
    
private:
    void setupBaseOptions() {
        baseOptions.SetWarningsAsErrors();
        baseOptions.SetGenerateDebugInfo();
        baseOptions.SetOptimizationLevel(shaderc_optimization_level_performance);
        
        // 커스텀 include resolver 설정
        baseOptions.SetIncludeCallbacks(
            [](void* userData, const char* requestedPath, int type,
               const char* requestingPath, size_t includeDepth) -> shaderc_include_result* {
                return handleInclude(requestedPath, type, requestingPath, includeDepth);
            },
            [](void* userData, shaderc_include_result* result) {
                releaseInclude(result);
            },
            nullptr
        );
    }
    
    shaderc_shader_kind convertStage(ShaderStage stage) {
        switch (stage) {
            case ShaderStage::Vertex: return shaderc_vertex_shader;
            case ShaderStage::Fragment: return shaderc_fragment_shader;
            case ShaderStage::Compute: return shaderc_compute_shader;
            case ShaderStage::Geometry: return shaderc_geometry_shader;
            case ShaderStage::TessellationControl: return shaderc_tess_control_shader;
            case ShaderStage::TessellationEvaluation: return shaderc_tess_evaluation_shader;
            case ShaderStage::Task: return shaderc_task_shader;
            case ShaderStage::Mesh: return shaderc_mesh_shader;
            default: return shaderc_vertex_shader;
        }
    }
};
```

### 11.4 SPIR-V Reflection 시스템
```cpp
// SPIR-V Cross를 활용한 Reflection
#include <spirv_cross.hpp>

struct ShaderReflection {
    // 입력/출력
    struct VertexInput {
        std::string name;
        uint32_t location;
        uint32_t binding;
        Format format;
        uint32_t offset;
    };
    
    struct DescriptorBinding {
        std::string name;
        uint32_t set;
        uint32_t binding;
        DescriptorType type;
        uint32_t arraySize;
        ShaderStage stages;
    };
    
    struct PushConstantRange {
        std::string name;
        uint32_t offset;
        uint32_t size;
        ShaderStage stages;
    };
    
    std::vector<VertexInput> vertexInputs;
    std::vector<DescriptorBinding> descriptorBindings;
    std::vector<PushConstantRange> pushConstants;
    
    // 컴퓨트 셰이더 정보
    glm::uvec3 workGroupSize{1, 1, 1};
    
    // 특수화 상수
    struct SpecializationConstant {
        std::string name;
        uint32_t id;
        size_t size;
    };
    std::vector<SpecializationConstant> specializationConstants;
};

class SPIRVReflector {
public:
    static ShaderReflection reflect(const std::vector<uint32_t>& spirvBytecode) {
        ShaderReflection reflection{};
        
        spirv_cross::Compiler compiler(spirvBytecode);
        spirv_cross::ShaderResources resources = compiler.get_shader_resources();
        
        // Vertex inputs 분석
        for (const auto& input : resources.stage_inputs) {
            ShaderReflection::VertexInput vertexInput{};
            vertexInput.name = input.name;
            vertexInput.location = compiler.get_decoration(input.id, spv::DecorationLocation);
            
            const auto& type = compiler.get_type(input.type_id);
            vertexInput.format = convertSPIRVTypeToFormat(type);
            
            reflection.vertexInputs.push_back(vertexInput);
        }
        
        // Uniform buffers 분석
        analyzeDescriptorBindings(compiler, resources.uniform_buffers, 
                                DescriptorType::UniformBuffer, reflection);
        
        // Storage buffers 분석
        analyzeDescriptorBindings(compiler, resources.storage_buffers,
                                DescriptorType::StorageBuffer, reflection);
        
        // Samplers 분석
        analyzeDescriptorBindings(compiler, resources.sampled_images,
                                DescriptorType::CombinedImageSampler, reflection);
        
        // Storage images 분석
        analyzeDescriptorBindings(compiler, resources.storage_images,
                                DescriptorType::StorageImage, reflection);
        
        // Push constants 분석
        for (const auto& pushConstant : resources.push_constant_buffers) {
            ShaderReflection::PushConstantRange range{};
            range.name = pushConstant.name;
            
            const auto& type = compiler.get_type(pushConstant.type_id);
            range.size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));
            
            // 오프셋은 보통 0 (단일 push constant block)
            range.offset = 0;
            
            reflection.pushConstants.push_back(range);
        }
        
        // 워크 그룹 사이즈 (컴퓨트 셰이더)
        if (compiler.get_execution_model() == spv::ExecutionModelGLCompute) {
            auto workGroupSize = compiler.get_work_group_size_specialization_constants();
            if (workGroupSize.x == 0) {
                // 특수화 상수가 아닌 경우 직접 값 가져오기
                reflection.workGroupSize = glm::uvec3(
                    compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 0),
                    compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 1),
                    compiler.get_execution_mode_argument(spv::ExecutionModeLocalSize, 2)
                );
            }
        }
        
        return reflection;
    }
    
private:
    static void analyzeDescriptorBindings(spirv_cross::Compiler& compiler,
                                         const std::vector<spirv_cross::Resource>& resources,
                                         DescriptorType type,
                                         ShaderReflection& reflection) {
        for (const auto& resource : resources) {
            ShaderReflection::DescriptorBinding binding{};
            binding.name = resource.name;
            binding.set = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
            binding.binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
            binding.type = type;
            
            const auto& resourceType = compiler.get_type(resource.type_id);
            binding.arraySize = resourceType.array.empty() ? 1 : resourceType.array[0];
            
            reflection.descriptorBindings.push_back(binding);
        }
    }
    
    static Format convertSPIRVTypeToFormat(const spirv_cross::SPIRType& type) {
        if (type.basetype == spirv_cross::SPIRType::Float) {
            switch (type.vecsize) {
                case 1: return Format::R32_SFLOAT;
                case 2: return Format::R32G32_SFLOAT;
                case 3: return Format::R32G32B32_SFLOAT;
                case 4: return Format::R32G32B32A32_SFLOAT;
            }
        } else if (type.basetype == spirv_cross::SPIRType::Int) {
            switch (type.vecsize) {
                case 1: return Format::R32_SINT;
                case 2: return Format::R32G32_SINT;
                case 3: return Format::R32G32B32_SINT;
                case 4: return Format::R32G32B32A32_SINT;
            }
        }
        
        return Format::UNDEFINED;
    }
};
```

### 11.5 자동 Pipeline 생성
```cpp
// Reflection 정보를 기반으로 Pipeline 자동 생성
class PipelineBuilder {
private:
    RenderDevice* device;
    
public:
    PipelineBuilder(RenderDevice* device) : device(device) {}
    
    std::unique_ptr<Pipeline> buildGraphicsPipeline(
        const std::vector<ShaderCompilationResult>& shaders,
        const PipelineStateDesc& stateDesc) {
        
        GraphicsPipelineDesc pipelineDesc{};
        
        // 셰이더 스테이지 설정
        for (const auto& shader : shaders) {
            PipelineShaderStage stage{};
            stage.stage = shader.reflection.stage;
            stage.bytecode = shader.spirvBytecode;
            stage.entryPoint = "main";
            
            pipelineDesc.shaderStages.push_back(stage);
        }
        
        // Vertex Input Layout 자동 생성
        pipelineDesc.vertexInputLayout = createVertexInputLayout(shaders);
        
        // Descriptor Set Layout 자동 생성
        pipelineDesc.descriptorSetLayouts = createDescriptorSetLayouts(shaders);
        
        // Push Constants Layout 자동 생성
        pipelineDesc.pushConstantRanges = createPushConstantRanges(shaders);
        
        // 렌더 스테이트 설정
        pipelineDesc.renderState = stateDesc.renderState;
        pipelineDesc.renderPass = stateDesc.renderPass;
        
        return device->createGraphicsPipeline(pipelineDesc);
    }
    
private:
    VertexInputLayout createVertexInputLayout(const std::vector<ShaderCompilationResult>& shaders) {
        VertexInputLayout layout{};
        
        // Vertex shader에서 입력 찾기
        for (const auto& shader : shaders) {
            if (shader.reflection.stage == ShaderStage::Vertex) {
                uint32_t offset = 0;
                for (const auto& input : shader.reflection.vertexInputs) {
                    VertexAttributeDesc attribute{};
                    attribute.location = input.location;
                    attribute.binding = 0; // 단일 버퍼 가정
                    attribute.format = input.format;
                    attribute.offset = offset;
                    
                    layout.attributes.push_back(attribute);
                    offset += getFormatSize(input.format);
                }
                
                // Vertex buffer binding 설정
                VertexBindingDesc binding{};
                binding.binding = 0;
                binding.stride = offset;
                binding.inputRate = VertexInputRate::Vertex;
                
                layout.bindings.push_back(binding);
                break;
            }
        }
        
        return layout;
    }
    
    std::vector<DescriptorSetLayout> createDescriptorSetLayouts(
        const std::vector<ShaderCompilationResult>& shaders) {
        
        // 모든 셰이더의 descriptor binding 수집
        std::map<uint32_t, std::vector<ShaderReflection::DescriptorBinding>> setBindings;
        
        for (const auto& shader : shaders) {
            for (const auto& binding : shader.reflection.descriptorBindings) {
                setBindings[binding.set].push_back(binding);
            }
        }
        
        // Descriptor Set Layout 생성
        std::vector<DescriptorSetLayout> layouts;
        for (const auto& [setIndex, bindings] : setBindings) {
            DescriptorSetLayoutDesc desc{};
            
            for (const auto& binding : bindings) {
                DescriptorSetLayoutBinding layoutBinding{};
                layoutBinding.binding = binding.binding;
                layoutBinding.descriptorType = binding.type;
                layoutBinding.descriptorCount = binding.arraySize;
                layoutBinding.stageFlags = binding.stages;
                
                desc.bindings.push_back(layoutBinding);
            }
            
            layouts.push_back(device->createDescriptorSetLayout(desc));
        }
        
        return layouts;
    }
};
```

### 11.6 Hot-Reloading 시스템
```cpp
// 셰이더 핫 리로딩 매니저
class ShaderHotReloader {
private:
    struct ShaderFile {
        std::filesystem::path filePath;
        ShaderStage stage;
        std::filesystem::file_time_type lastWriteTime;
        std::vector<Pipeline*> dependentPipelines;
    };
    
    std::unordered_map<std::string, ShaderFile> trackedShaders;
    ShaderCompiler compiler;
    PipelineBuilder pipelineBuilder;
    
    std::thread watcherThread;
    std::atomic<bool> running{false};
    
public:
    ShaderHotReloader(RenderDevice* device) : pipelineBuilder(device) {}
    
    void startWatching() {
        running = true;
        watcherThread = std::thread([this]() { watcherLoop(); });
    }
    
    void stopWatching() {
        running = false;
        if (watcherThread.joinable()) {
            watcherThread.join();
        }
    }
    
    void trackShader(const std::string& name, const std::filesystem::path& path, 
                    ShaderStage stage, Pipeline* pipeline) {
        ShaderFile& file = trackedShaders[name];
        file.filePath = path;
        file.stage = stage;
        file.lastWriteTime = std::filesystem::last_write_time(path);
        file.dependentPipelines.push_back(pipeline);
    }
    
private:
    void watcherLoop() {
        while (running) {
            checkForChanges();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void checkForChanges() {
        for (auto& [name, shader] : trackedShaders) {
            if (!std::filesystem::exists(shader.filePath)) {
                continue;
            }
            
            auto currentWriteTime = std::filesystem::last_write_time(shader.filePath);
            if (currentWriteTime > shader.lastWriteTime) {
                reloadShader(name, shader);
                shader.lastWriteTime = currentWriteTime;
            }
        }
    }
    
    void reloadShader(const std::string& name, ShaderFile& shader) {
        // 셰이더 파일 읽기
        std::ifstream file(shader.filePath);
        std::string source((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
        
        // 컴파일
        ShaderSource shaderSource{};
        shaderSource.source = source;
        shaderSource.stage = shader.stage;
        shaderSource.filename = shader.filePath.string();
        
        auto result = compiler.compile(shaderSource);
        if (!result.success) {
            // 컴파일 에러 로그
            logError("Shader compilation failed for {}: {}", name, result.errorMessage);
            return;
        }
        
        // 의존하는 파이프라인들 재생성
        for (Pipeline* pipeline : shader.dependentPipelines) {
            // 파이프라인 재생성 로직
            rebuildPipeline(pipeline, result);
        }
        
        logInfo("Shader {} reloaded successfully", name);
    }
    
    void rebuildPipeline(Pipeline* pipeline, const ShaderCompilationResult& newShader) {
        // 기존 파이프라인 정보를 기반으로 새로운 파이프라인 생성
        // 이는 복잡한 과정이므로 실제 구현에서는 더 정교한 관리가 필요
    }
};
```

## 실습 과제

### Phase 1: 기본 컴파일러
1. **shaderc 통합**
2. **기본 HLSL/GLSL 컴파일**
3. **에러 핸들링**

### Phase 2: Reflection 시스템
1. **SPIRV-Cross 통합**
2. **Descriptor 정보 추출**
3. **자동 Pipeline 생성**

### Phase 3: 고급 기능
1. **Hot-reloading 구현**
2. **Shader 변형 시스템**
3. **성능 최적화**

## 고급 주제
- **Shader 변형 (Variants)**: 조건부 컴파일
- **Uber Shader**: 단일 셰이더로 다양한 기능 지원
- **Shader Caching**: 컴파일 결과 캐싱
- **Cross-compilation**: SPIR-V에서 다른 언어로 변환

## 참고 자료
- "Real-Time Rendering" Shader 챕터
- Vulkan Specification (SPIR-V)
- shaderc 문서
- SPIRV-Cross 사용법