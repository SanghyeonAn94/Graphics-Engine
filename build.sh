#!/bin/bash
# build.sh - 크로스 플랫폼 빌드 스크립트

set -e  # 에러 발생 시 스크립트 종료

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 함수 정의
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

# 기본 설정
BUILD_TYPE="Debug"
BUILD_DIR="build"
COMPILER=""
CLEAN_BUILD=false
RUN_TESTS=false
INSTALL=false

# 도움말 함수
show_help() {
    echo "Usage: $0 [options]"
    echo ""
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -r, --release       Build in Release mode (default: Debug)"
    echo "  -c, --clean         Clean build directory before building"
    echo "  -t, --test          Run tests after building"
    echo "  -i, --install       Install after building"
    echo "  --compiler=COMP     Use specific compiler (gcc, clang, msvc)"
    echo "  --build-dir=DIR     Use custom build directory (default: build)"
    echo ""
    echo "Examples:"
    echo "  $0                  # Debug build"
    echo "  $0 -r -t            # Release build with tests"
    echo "  $0 -c --compiler=clang  # Clean debug build with clang"
}

# 명령행 인수 파싱
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_help
            exit 0
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -t|--test)
            RUN_TESTS=true
            shift
            ;;
        -i|--install)
            INSTALL=true
            shift
            ;;
        --compiler=*)
            COMPILER="${1#*=}"
            shift
            ;;
        --build-dir=*)
            BUILD_DIR="${1#*=}"
            shift
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# 시스템 정보 검사
print_info "Detecting system information..."
OS="Unknown"
if [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="Linux"
elif [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macOS"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    OS="Windows"
fi

print_info "Operating System: $OS"
print_info "Build Type: $BUILD_TYPE"
print_info "Build Directory: $BUILD_DIR"

# 컴파일러 설정
if [[ -z "$COMPILER" ]]; then
    # 자동 컴파일러 선택
    if command -v clang++ &> /dev/null; then
        COMPILER="clang"
        print_info "Auto-selected compiler: Clang++"
    elif command -v g++ &> /dev/null; then
        COMPILER="gcc"
        print_info "Auto-selected compiler: GCC"
    else
        print_error "No suitable compiler found. Please install Clang++ or GCC."
        exit 1
    fi
else
    print_info "Using specified compiler: $COMPILER"
fi

# 컴파일러 버전 확인
case $COMPILER in
    clang)
        if ! command -v clang++ &> /dev/null; then
            print_error "Clang++ not found"
            exit 1
        fi
        CLANG_VERSION=$(clang++ --version | head -n1 | grep -o '[0-9]\+\.[0-9]\+')
        CLANG_MAJOR=$(echo $CLANG_VERSION | cut -d. -f1)
        if [[ $CLANG_MAJOR -lt 17 ]]; then
            print_warning "Clang++ $CLANG_VERSION detected. C++23 modules may not be fully supported. Recommend Clang++ 17+"
        else
            print_success "Clang++ $CLANG_VERSION detected - C++23 support available"
        fi
        ;;
    gcc)
        if ! command -v g++ &> /dev/null; then
            print_error "GCC not found"
            exit 1
        fi
        GCC_VERSION=$(g++ --version | head -n1 | grep -o '[0-9]\+\.[0-9]\+')
        GCC_MAJOR=$(echo $GCC_VERSION | cut -d. -f1)
        if [[ $GCC_MAJOR -lt 14 ]]; then
            print_warning "GCC $GCC_VERSION detected. C++23 modules may not be fully supported. Recommend GCC 14+"
        else
            print_success "GCC $GCC_VERSION detected - C++23 support available"
        fi
        ;;
esac

# CMake 버전 확인
if ! command -v cmake &> /dev/null; then
    print_error "CMake not found. Please install CMake 3.28+"
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | grep -o '[0-9]\+\.[0-9]\+')
CMAKE_MAJOR=$(echo $CMAKE_VERSION | cut -d. -f1)
CMAKE_MINOR=$(echo $CMAKE_VERSION | cut -d. -f2)

if [[ $CMAKE_MAJOR -lt 3 ]] || [[ $CMAKE_MAJOR -eq 3 && $CMAKE_MINOR -lt 28 ]]; then
    print_warning "CMake $CMAKE_VERSION detected. C++ modules support requires CMake 3.28+. Some features may not work."
else
    print_success "CMake $CMAKE_VERSION detected - C++ modules support available"
fi

# 빌드 디렉토리 정리
if [[ $CLEAN_BUILD == true ]]; then
    print_info "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# 빌드 디렉토리 생성
print_info "Creating build directory..."
mkdir -p "$BUILD_DIR"

# CMake 설정
print_info "Configuring CMake..."
CMAKE_ARGS=""

case $COMPILER in
    clang)
        CMAKE_ARGS="-DCMAKE_CXX_COMPILER=clang++"
        ;;
    gcc)
        CMAKE_ARGS="-DCMAKE_CXX_COMPILER=g++"
        ;;
esac

cd "$BUILD_DIR"

# CMake 구성 실행
if ! cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE" $CMAKE_ARGS; then
    print_error "CMake configuration failed"
    exit 1
fi

print_success "CMake configuration completed"

# 빌드 실행
print_info "Building project..."
CPU_CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
print_info "Using $CPU_CORES parallel jobs"

if ! cmake --build . --parallel $CPU_CORES; then
    print_error "Build failed"
    exit 1
fi

print_success "Build completed successfully"

# 테스트 실행
if [[ $RUN_TESTS == true ]]; then
    print_info "Running tests..."
    
    if [[ -f "./EngineTests" ]]; then
        if ! ./EngineTests; then
            print_error "Tests failed"
            exit 1
        fi
        print_success "All tests passed"
    else
        print_warning "Test executable not found"
    fi
fi

# 설치
if [[ $INSTALL == true ]]; then
    print_info "Installing..."
    if ! cmake --build . --target install; then
        print_error "Installation failed"
        exit 1
    fi
    print_success "Installation completed"
fi

# 예제 실행 제안
if [[ -f "./BasicExample" ]]; then
    print_info "Example executable built successfully"
    echo ""
    echo "To run the basic example:"
    echo "  cd $BUILD_DIR && ./BasicExample"
fi

print_success "Build process completed successfully!"