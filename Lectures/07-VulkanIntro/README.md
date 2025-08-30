# 07. Asset Pipeline & Serialization

## 학습 목표
- 효율적인 에셋 파이프라인 설계
- Binary Serialization 시스템 구현
- Reflection을 활용한 자동 직렬화
- Runtime Asset Hot-Reloading 구현

## 강의 내용

### 7.1 에셋 파이프라인 아키텍처
- Source Assets vs Runtime Assets
- 에셋 변환과 최적화 과정
- 의존성 관리와 증분 빌드
- 플랫폼별 최적화

### 7.2 에셋 타입 시스템
```cpp
// 에셋 기본 클래스
class Asset {
private:
    AssetID id;
    std::string name;
    mutable std::atomic<uint32_t> refCount{0};
    
public:
    Asset(AssetID id, std::string name) : id(id), name(std::move(name)) {}
    virtual ~Asset() = default;
    
    AssetID getId() const { return id; }
    const std::string& getName() const { return name; }
    
    void addRef() const { refCount.fetch_add(1); }
    void release() const {
        if (refCount.fetch_sub(1) == 1) {
            delete this;
        }
    }
    
    uint32_t getRefCount() const { return refCount.load(); }
    
    virtual AssetType getType() const = 0;
    virtual bool serialize(BinaryStream& stream) const = 0;
    virtual bool deserialize(BinaryStream& stream) = 0;
};

// 구체적인 에셋 타입들
class Texture : public Asset {
private:
    uint32_t width, height;
    TextureFormat format;
    std::vector<uint8_t> data;
    
public:
    AssetType getType() const override { return AssetType::Texture; }
    bool serialize(BinaryStream& stream) const override;
    bool deserialize(BinaryStream& stream) override;
    
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    TextureFormat getFormat() const { return format; }
    const std::vector<uint8_t>& getData() const { return data; }
};

class Mesh : public Asset {
private:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    BoundingBox bounds;
    
public:
    AssetType getType() const override { return AssetType::Mesh; }
    bool serialize(BinaryStream& stream) const override;
    bool deserialize(BinaryStream& stream) override;
};
```

### 7.3 Binary Serialization 시스템
```cpp
// Binary Stream 구현
class BinaryStream {
private:
    std::vector<uint8_t> buffer;
    size_t position = 0;
    bool writing = true;
    
public:
    explicit BinaryStream(bool write = true) : writing(write) {}
    
    // 기본 타입 직렬화
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    void serialize(const T& value) {
        if (writing) {
            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
            buffer.insert(buffer.end(), ptr, ptr + sizeof(T));
        } else {
            if (position + sizeof(T) <= buffer.size()) {
                T* ptr = reinterpret_cast<T*>(&value);
                std::memcpy(ptr, buffer.data() + position, sizeof(T));
                position += sizeof(T);
            }
        }
    }
    
    // 문자열 직렬화
    void serialize(const std::string& str) {
        uint32_t length = static_cast<uint32_t>(str.size());
        serialize(length);
        
        if (writing) {
            buffer.insert(buffer.end(), str.begin(), str.end());
        } else {
            if (position + length <= buffer.size()) {
                const_cast<std::string&>(str).assign(
                    reinterpret_cast<const char*>(buffer.data() + position), length
                );
                position += length;
            }
        }
    }
    
    // 벡터 직렬화
    template<typename T>
    void serialize(const std::vector<T>& vec) {
        uint32_t size = static_cast<uint32_t>(vec.size());
        serialize(size);
        
        if (writing) {
            for (const auto& item : vec) {
                serialize(item);
            }
        } else {
            const_cast<std::vector<T>&>(vec).resize(size);
            for (auto& item : const_cast<std::vector<T>&>(vec)) {
                serialize(item);
            }
        }
    }
    
    const std::vector<uint8_t>& getBuffer() const { return buffer; }
    void setBuffer(std::vector<uint8_t> data) { 
        buffer = std::move(data); 
        position = 0; 
        writing = false; 
    }
};
```

### 7.4 Reflection System 구현
```cpp
// Reflection을 위한 메타데이터
#define REFLECT_CLASS(ClassName) \
    static const char* getClassName() { return #ClassName; } \
    static ReflectionInfo* getReflectionInfo(); \
    template<typename Visitor> void visit(Visitor&& visitor)

#define REFLECT_MEMBER(member) \
    visitor(#member, member)

// Reflection 정보 저장
struct FieldInfo {
    std::string name;
    size_t offset;
    TypeInfo typeInfo;
};

struct ReflectionInfo {
    std::string className;
    std::vector<FieldInfo> fields;
    
    template<typename T>
    void addField(const std::string& name, size_t offset) {
        fields.push_back({name, offset, TypeInfo::get<T>()});
    }
};

// 사용 예시
struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f};
    
    REFLECT_CLASS(Transform);
    
    template<typename Visitor>
    void visit(Visitor&& visitor) {
        REFLECT_MEMBER(position);
        REFLECT_MEMBER(rotation);
        REFLECT_MEMBER(scale);
    }
};

// 자동 직렬화
template<typename T>
requires requires(T t) { t.visit(std::declval<auto>()); }
void autoSerialize(BinaryStream& stream, const T& object) {
    object.visit([&stream](const std::string& name, const auto& member) {
        stream.serialize(member);
    });
}
```

### 7.5 에셋 매니저 구현
```cpp
class AssetManager {
private:
    std::unordered_map<AssetID, std::unique_ptr<Asset>> assets;
    std::unordered_map<std::string, AssetID> nameToId;
    std::unordered_map<AssetID, std::string> idToPath;
    
    FileWatcher fileWatcher;
    std::thread loadingThread;
    std::queue<AssetID> loadingQueue;
    std::mutex queueMutex;
    
public:
    template<typename T>
    std::shared_ptr<T> load(const std::string& path) {
        AssetID id = generateIdFromPath(path);
        
        if (auto it = assets.find(id); it != assets.end()) {
            return std::static_pointer_cast<T>(it->second);
        }
        
        return loadAsset<T>(path, id);
    }
    
    template<typename T>
    std::shared_ptr<T> loadAsync(const std::string& path, 
                                std::function<void(std::shared_ptr<T>)> callback) {
        // 비동기 로딩 구현
        AssetID id = generateIdFromPath(path);
        
        std::lock_guard<std::mutex> lock(queueMutex);
        loadingQueue.push(id);
        
        return nullptr; // placeholder
    }
    
    void enableHotReloading(bool enable) {
        if (enable) {
            fileWatcher.start();
            // 모든 에셋 파일 감시 등록
            for (const auto& [id, path] : idToPath) {
                fileWatcher.watchFile(path, [this, id](const Path& changedPath) {
                    reloadAsset(id);
                });
            }
        } else {
            fileWatcher.stop();
        }
    }
    
private:
    template<typename T>
    std::shared_ptr<T> loadAsset(const std::string& path, AssetID id) {
        // 파일 읽기
        auto fileData = FileSystem::readFile(path);
        
        // 에셋 생성 및 역직렬화
        auto asset = std::make_unique<T>(id, path);
        
        BinaryStream stream(false);
        stream.setBuffer(std::move(fileData));
        
        if (!asset->deserialize(stream)) {
            return nullptr;
        }
        
        assets[id] = std::move(asset);
        nameToId[path] = id;
        idToPath[id] = path;
        
        return std::static_pointer_cast<T>(assets[id]);
    }
    
    void reloadAsset(AssetID id) {
        if (auto it = idToPath.find(id); it != idToPath.end()) {
            // 기존 에셋 제거하고 재로딩
            assets.erase(id);
            
            const std::string& path = it->second;
            AssetType type = determineAssetType(path);
            
            switch (type) {
                case AssetType::Texture:
                    loadAsset<Texture>(path, id);
                    break;
                case AssetType::Mesh:
                    loadAsset<Mesh>(path, id);
                    break;
                // 기타 타입들...
            }
        }
    }
};
```

### 7.6 에셋 빌드 파이프라인
```cpp
// 에셋 변환기 인터페이스
class AssetConverter {
public:
    virtual ~AssetConverter() = default;
    virtual bool canConvert(const std::string& extension) const = 0;
    virtual bool convert(const Path& sourcePath, const Path& outputPath) = 0;
    virtual std::vector<std::string> getDependencies(const Path& sourcePath) const = 0;
};

// 텍스처 변환기 예시
class TextureConverter : public AssetConverter {
public:
    bool canConvert(const std::string& extension) const override {
        return extension == ".png" || extension == ".jpg" || extension == ".tga";
    }
    
    bool convert(const Path& sourcePath, const Path& outputPath) override {
        // 이미지 로딩 (stb_image 등 사용)
        int width, height, channels;
        unsigned char* data = stbi_load(sourcePath.toString().c_str(), 
                                      &width, &height, &channels, 4);
        
        if (!data) return false;
        
        // DXT 압축 등 적용
        auto compressedData = compressTexture(data, width, height, TextureFormat::DXT5);
        
        // Binary 형태로 저장
        BinaryStream stream(true);
        stream.serialize(width);
        stream.serialize(height);
        stream.serialize(TextureFormat::DXT5);
        stream.serialize(compressedData);
        
        FileSystem::writeFile(outputPath, stream.getBuffer());
        
        stbi_image_free(data);
        return true;
    }
};

// 빌드 시스템
class AssetBuildSystem {
private:
    std::vector<std::unique_ptr<AssetConverter>> converters;
    std::unordered_map<std::string, std::filesystem::file_time_type> buildCache;
    
public:
    void addConverter(std::unique_ptr<AssetConverter> converter) {
        converters.push_back(std::move(converter));
    }
    
    void buildAssets(const Path& sourceDir, const Path& outputDir) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceDir.toString())) {
            if (!entry.is_regular_file()) continue;
            
            const auto sourcePath = Path(entry.path().string());
            const auto extension = sourcePath.getExtension();
            
            // 변환기 찾기
            AssetConverter* converter = nullptr;
            for (const auto& conv : converters) {
                if (conv->canConvert(extension)) {
                    converter = conv.get();
                    break;
                }
            }
            
            if (!converter) continue;
            
            // 변경 확인 (증분 빌드)
            auto lastWrite = std::filesystem::last_write_time(entry.path());
            if (auto it = buildCache.find(sourcePath.toString()); 
                it != buildCache.end() && it->second >= lastWrite) {
                continue; // 이미 최신 상태
            }
            
            // 출력 경로 계산
            auto relativePath = std::filesystem::relative(entry.path(), sourceDir.toString());
            auto outputPath = outputDir / relativePath.string();
            outputPath = Path(outputPath.toString() + ".asset");
            
            // 변환 실행
            if (converter->convert(sourcePath, outputPath)) {
                buildCache[sourcePath.toString()] = lastWrite;
            }
        }
    }
};
```

## 실습 과제

### Phase 1: 기본 직렬화
1. **BinaryStream 클래스 구현**
2. **기본 에셋 타입 정의** (Texture, Mesh)
3. **간단한 로드/저장 테스트**

### Phase 2: Reflection 시스템
1. **메타데이터 생성 매크로**
2. **자동 직렬화 구현**
3. **타입 안전성 검증**

### Phase 3: 에셋 파이프라인
1. **AssetManager 구현**
2. **Hot-reloading 시스템**
3. **빌드 파이프라인 구축**

## 성능 최적화
- 메모리 풀을 통한 할당 최적화
- 비동기 로딩과 스트리밍
- 압축과 캐싱 전략
- 의존성 기반 로딩 순서

## 참고 자료
- "Real-Time Rendering" Asset Management 챕터
- "Game Engine Gems" Serialization 관련 글들
- FlatBuffers, Protocol Buffers 연구
- Unreal Engine Asset System 분석