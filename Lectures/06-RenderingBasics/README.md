# 06. Platform Abstraction Layer

## 학습 목표
- Cross-platform 호환성을 위한 추상화 레이어 설계
- 윈도우 시스템과 입력 처리 통합
- 파일 시스템 추상화와 경로 관리
- Hot-reloading 시스템 구현

## 강의 내용

### 6.1 Platform Abstraction 철학
- "Write once, run anywhere" vs 플랫폼별 최적화
- 추상화 레벨 결정
- 성능과 호환성 트레이드오프
- 플랫폼별 특성 활용

### 6.2 윈도우 시스템 추상화
```cpp
// 플랫폼 독립적 윈도우 인터페이스
class Window {
public:
    virtual ~Window() = default;
    
    virtual bool create(const WindowDesc& desc) = 0;
    virtual void destroy() = 0;
    virtual void update() = 0;
    
    virtual void setTitle(const std::string& title) = 0;
    virtual void setSize(uint32_t width, uint32_t height) = 0;
    virtual void setPosition(int32_t x, int32_t y) = 0;
    
    virtual bool shouldClose() const = 0;
    virtual void* getNativeHandle() const = 0;
    
    // Events
    std::function<void(uint32_t, uint32_t)> onResize;
    std::function<void(int32_t, int32_t)> onMouseMove;
    std::function<void(int32_t, bool)> onKeyPress;
};

// Windows 구현
class Win32Window : public Window {
private:
    HWND hwnd = nullptr;
    
public:
    bool create(const WindowDesc& desc) override;
    static LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Linux 구현  
class X11Window : public Window {
private:
    Display* display = nullptr;
    ::Window window = 0;
    
public:
    bool create(const WindowDesc& desc) override;
    void processEvents();
};
```

### 6.3 입력 시스템 설계
```cpp
// 입력 추상화
enum class KeyCode {
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Space, Enter, Escape, Tab,
    Arrow_Left, Arrow_Right, Arrow_Up, Arrow_Down,
    // 게임패드 버튼들
    Gamepad_A, Gamepad_B, Gamepad_X, Gamepad_Y
};

enum class MouseButton {
    Left, Right, Middle, X1, X2
};

class InputSystem {
private:
    std::array<bool, 256> currentKeys{};
    std::array<bool, 256> previousKeys{};
    std::array<bool, 8> currentMouseButtons{};
    std::array<bool, 8> previousMouseButtons{};
    
    glm::vec2 mousePosition{0.0f};
    glm::vec2 mouseDelta{0.0f};
    float scrollDelta = 0.0f;
    
public:
    void update();
    
    bool isKeyDown(KeyCode key) const;
    bool isKeyPressed(KeyCode key) const;
    bool isKeyReleased(KeyCode key) const;
    
    bool isMouseButtonDown(MouseButton button) const;
    bool isMouseButtonPressed(MouseButton button) const;
    bool isMouseButtonReleased(MouseButton button) const;
    
    glm::vec2 getMousePosition() const { return mousePosition; }
    glm::vec2 getMouseDelta() const { return mouseDelta; }
    float getScrollDelta() const { return scrollDelta; }
    
    // 게임패드 지원
    struct GamepadState {
        bool connected = false;
        std::array<bool, 16> buttons{};
        std::array<float, 6> axes{}; // Left stick, Right stick, Triggers
    };
    
    GamepadState getGamepadState(uint32_t index) const;
};
```

### 6.4 파일 시스템 추상화
```cpp
// C++17 filesystem을 기반으로 한 확장
namespace FileSystem {
    class Path {
    private:
        std::filesystem::path path_;
        
    public:
        Path() = default;
        Path(const std::string& path) : path_(path) {}
        Path(const char* path) : path_(path) {}
        
        std::string toString() const { return path_.string(); }
        std::string getExtension() const { return path_.extension().string(); }
        std::string getFilename() const { return path_.filename().string(); }
        Path getParent() const { return Path(path_.parent_path().string()); }
        
        bool exists() const { return std::filesystem::exists(path_); }
        bool isFile() const { return std::filesystem::is_regular_file(path_); }
        bool isDirectory() const { return std::filesystem::is_directory(path_); }
        
        Path operator/(const std::string& other) const {
            return Path((path_ / other).string());
        }
    };
    
    // 가상 파일 시스템 지원
    class VirtualFileSystem {
    private:
        std::unordered_map<std::string, Path> mountPoints;
        
    public:
        void mount(const std::string& virtualPath, const Path& realPath);
        void unmount(const std::string& virtualPath);
        
        Path resolve(const Path& virtualPath) const;
        
        std::vector<uint8_t> readFile(const Path& path);
        bool writeFile(const Path& path, const std::vector<uint8_t>& data);
        
        std::vector<Path> listDirectory(const Path& path);
    };
    
    // 플랫폼별 경로 처리
    class PathUtils {
    public:
        static Path getExecutableDirectory();
        static Path getDocumentsDirectory();
        static Path getUserDataDirectory(const std::string& appName);
        static Path getTempDirectory();
        
        static std::string toNativePath(const Path& path);
        static Path fromNativePath(const std::string& nativePath);
    };
}
```

### 6.5 Hot-Reloading 시스템
```cpp
// 파일 변경 감지
class FileWatcher {
private:
    struct WatchEntry {
        Path path;
        std::filesystem::file_time_type lastWriteTime;
        std::function<void(const Path&)> callback;
    };
    
    std::vector<WatchEntry> watchedFiles;
    std::thread watcherThread;
    std::atomic<bool> running{false};
    
public:
    void start();
    void stop();
    
    void watchFile(const Path& path, std::function<void(const Path&)> callback);
    void unwatchFile(const Path& path);
    
private:
    void watcherLoop();
    void checkForChanges();
};

// Hot-reload 매니저
class HotReloadManager {
private:
    FileWatcher fileWatcher;
    std::unordered_map<std::string, std::function<void()>> reloadCallbacks;
    
public:
    void initialize();
    void shutdown();
    
    void watchShader(const Path& shaderPath, std::function<void()> reloadCallback);
    void watchTexture(const Path& texturePath, std::function<void()> reloadCallback);
    void watchScript(const Path& scriptPath, std::function<void()> reloadCallback);
    
private:
    void onFileChanged(const Path& path);
};
```

### 6.6 플랫폼별 성능 최적화
```cpp
// 플랫폼별 최적화 유틸리티
namespace PlatformUtils {
    // CPU 정보 조회
    struct CPUInfo {
        std::string brand;
        uint32_t coreCount;
        uint32_t threadCount;
        bool hasSSE4;
        bool hasAVX2;
        bool hasAVX512;
    };
    
    CPUInfo getCPUInfo();
    
    // 메모리 정보
    struct MemoryInfo {
        size_t totalPhysical;
        size_t availablePhysical;
        size_t totalVirtual;
        size_t availableVirtual;
    };
    
    MemoryInfo getMemoryInfo();
    
    // 고해상도 타이머
    class HighResTimer {
    private:
        std::chrono::high_resolution_clock::time_point startTime;
        
    public:
        void start() { startTime = std::chrono::high_resolution_clock::now(); }
        
        double getElapsedSeconds() const {
            auto now = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime);
            return duration.count() / 1000000.0;
        }
    };
    
    // 플랫폼별 라이브러리 로딩
    class DynamicLibrary {
    private:
        void* handle = nullptr;
        
    public:
        bool load(const Path& libraryPath);
        void unload();
        
        template<typename T>
        T* getFunction(const std::string& name);
    };
}
```

## 실습 과제

### Phase 1: 기본 플랫폼 레이어
1. **윈도우 시스템 구현** (Windows/Linux)
2. **기본 입력 처리**
3. **파일 시스템 래퍼 제작**

### Phase 2: 고급 기능
1. **Virtual File System 구현**
2. **Hot-reloading 시스템**
3. **플랫폼별 최적화**

### Phase 3: 통합 테스트
1. **Cross-platform 빌드 테스트**
2. **성능 벤치마크**
3. **사용성 테스트**

## 플랫폼별 고려사항
- **Windows**: Win32 API, DirectX 통합, Windows 10/11 기능
- **Linux**: X11/Wayland, OpenGL/Vulkan 지원
- **macOS**: Cocoa, Metal 통합, macOS 특화 기능
- **Mobile**: 터치 입력, 센서 지원, 배터리 최적화

## 참고 자료
- "Game Engine Architecture" Platform Layer 챕터
- SDL2 소스코드 분석
- GLFW 구현 연구
- Platform-specific API 문서들