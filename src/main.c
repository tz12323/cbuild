#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<errno.h>
#include<stdint.h>
#include<ctype.h>  
#include<stdbool.h>

#if defined(__linux__)
    #define PLATFORM_LINUX 1
#endif

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
#include <direct.h>
#include <windows.h>
#include <sys/stat.h>
#define MKDIR(path) _mkdir(path)
#define CHDIR(path) _chdir(path)
#define PATH_SEP '\\'
#define EXE_EXT ".exe"
#define STATIC_LIB_EXT ".lib"
#define SHARED_LIB_EXT ".dll"
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#define CHDIR(path) chdir(path)
#define PATH_SEP '/'
#define EXE_EXT ""
#if defined(__APPLE__)
    #define PLATFORM_MACOS 1
#define STATIC_LIB_EXT ".a"
#define SHARED_LIB_EXT ".dylib"
#else
#define STATIC_LIB_EXT ".a"
#define SHARED_LIB_EXT ".so"
#endif
#endif

#define MAX_PATH_LEN 1024
#define BUFFER_SIZE 1024
#define MAX_DEPS 20  // 最大依赖数

// 显示平台信息
void print_platform_info() {
    printf("运行平台: ");
#if defined(PLATFORM_WINDOWS)
    printf("Windows\n");
#elif defined(PLATFORM_MACOS)
    printf("macOS\n");
#elif defined(PLATFORM_LINUX)
    printf("Linux\n");
#else
    printf("未知平台\n");
#endif
}

// 输出参数
void print_usage(const char* program_name) {
    print_platform_info();
    printf("\n用法: %s [选项] <项目名>\n\n", program_name);
    printf("选项:\n");
    printf("  new <项目名>                创建新项目\n");
    printf("    -e, --executable         创建可执行项目（默认）\n");
    printf("    -s, --static             创建静态库项目\n");
    printf("    -d, --shared             创建动态库项目\n");
    printf("    -D, --dep <依赖>         添加项目依赖\n");
    printf("    -h, --help               显示此帮助信息\n\n");
    printf("  build                      构建项目\n");
    printf("    -d, --debug              使用Debug模式构建\n");
    printf("    -r, --release            使用Release模式构建\n");
    printf("    -p, --prefix             指定安装目录\n");
    printf("    -c, --configure-only     选择是否构建\n");
    printf("    -b, --build-dir          设置构建目录\n");
    printf("  init                       根据CMake.toml创建新项目\n");
    printf("示例:\n");
    printf("  %s new myapp -e -D fmt -D sdl2\n", program_name);
    printf("  %s new mylib -s -D boost\n", program_name);
    
#if defined(PLATFORM_WINDOWS)
    printf("\n注意: Windows平台需要预先安装CMake和编译器\n");
    printf("      推荐使用MSVC或MinGW\n");
#elif defined(PLATFORM_MACOS)
    printf("\n注意: macOS平台需要安装Xcode命令行工具:\n");
    printf("      xcode-select --install\n");
#else
    printf("\n注意: Linux平台需要安装build-essential和cmake:\n");
    printf("      sudo apt-get install build-essential cmake\n");
#endif
}

int create_cmake_toml(const char* project_name, const char* project_type, char deps[][MAX_PATH_LEN], int num_deps) {
    FILE* toml_file = fopen("CMake.toml", "w");
    if (!toml_file) {
        perror("创建CMake.toml失败");
        return 0;
    }
    
    fprintf(toml_file, "# CMake项目配置文件\n");
    fprintf(toml_file, "[project]\n");
    fprintf(toml_file, "name = \"%s\"\n", project_name);
    fprintf(toml_file, "type = \"%s\"\n", project_type);
    fprintf(toml_file, "version = \"1.0.0\"\n\n");
    
    fprintf(toml_file, "# 依赖配置\n");
    fprintf(toml_file, "[dependencies]\n");
    
    // 添加命令行指定的依赖项
    for (int i = 0; i < num_deps; i++) {
        fprintf(toml_file, "%s = \"latest\"\n", deps[i]);
    }
    
    // 添加示例依赖项（作为注释）
    if (num_deps == 0) {
        fprintf(toml_file, "# fmt = \"9.1.0\"\n");
        fprintf(toml_file, "# boost = \"1.83.0\"\n");
        fprintf(toml_file, "# sdl2 = \"2.28.5\"\n");
        fprintf(toml_file, "# glfw = \"3.3.8\"\n");
        fprintf(toml_file, "# json = { name = \"nlohmann_json\", version = \"3.11.2\" }\n");
    }
    
    fclose(toml_file);
    return 1;
}

// 移除字符串首尾的空白字符和引号
void trim_string(char *str) {
    if (!str || !*str) return;
    
    char *start = str;
    char *end = start + strlen(str);
    
    // 去除前面的空白和引号
    while (isspace((unsigned char)*start) || *start == '"' || *start == '\'') start++;
    
    // 去除后面的空白和引号
    while (end > start && (isspace((unsigned char)*(end-1)) || *(end-1) == '"' || *(end-1) == '\'')) end--;
    
    // 移动字符
    memmove(str, start, end - start);
    str[end - start] = '\0';
}

// 智能解析CMake.toml文件
int parse_cmake_toml(char* project_name, char* project_type, char deps[][MAX_PATH_LEN], int* num_deps) {
    FILE* toml_file = fopen("CMake.toml", "r");
    if (!toml_file) return 0;
    
    char line[BUFFER_SIZE];
    int in_project = 0;
    int in_dependencies = 0;
    *num_deps = 0;
    
    // 设置默认值
    strcpy(project_type, "executable");

    while (fgets(line, sizeof(line), toml_file)) {
        // 移除行尾换行符
        line[strcspn(line, "\r\n")] = '\0';
        
        // 跳过空行
        if (strlen(line) == 0) continue;
        
        // 检测区块
        if (strstr(line, "[project]")) {
            in_project = 1;
            in_dependencies = 0;
            continue;
        } 
        else if (strstr(line, "[dependencies]")) {
            in_project = 0;
            in_dependencies = 1;
            continue;
        } 
        else if (line[0] == '[') {
            in_project = 0;
            in_dependencies = 0;
            continue;
        }
        
        // 跳过注释行
        if (line[0] == '#') continue;
        
        // 解析项目信息
        if (in_project) {
            char* key = strtok(line, "=");
            char* value = strtok(NULL, "\0");
            if (!key || !value) continue;
            
            // 去除键和值两侧的空格
            trim_string(key);
            trim_string(value);
            
            // 去除值两侧的引号
            char* v = value;
            while (*v == '"' || *v == '\'') v++;
            char* end = v + strlen(v) - 1;
            while (end > v && (*end == '"' || *end == '\'')) end--;
            *(end+1) = '\0';
            
            if (strstr(key, "name")) {
                strncpy(project_name, v, MAX_PATH_LEN);
            } else if (strstr(key, "type")) {
                strncpy(project_type, v, 15);
            }
        }
        
        // 解析依赖项
        if (in_dependencies) {
            char dep_name[MAX_PATH_LEN] = {0};
            char* name_start = line;
            
            // 跳过开头空格
            while (isspace(*name_start)) name_start++;
            
            // 解析简单依赖格式: name = "version"
            if (*num_deps < MAX_DEPS) {
                char* equal_sign = strchr(name_start, '=');
                char* open_brace = strchr(name_start, '{');
                
                // 简单格式: key = value
                if (equal_sign && (!open_brace || equal_sign < open_brace)) {
                    *equal_sign = '\0';
                    trim_string(name_start);
                    strncpy(deps[*num_deps], name_start, MAX_PATH_LEN);
                    (*num_deps)++;
                }
                // 复杂格式: key = { ... }
                else if (open_brace) {
                    char* name_end = open_brace;
                    while (name_end > name_start && isspace(*(name_end-1))) name_end--;
                    *name_end = '\0';
                    trim_string(name_start);
                    strncpy(deps[*num_deps], name_start, MAX_PATH_LEN);
                    (*num_deps)++;
                }
                // 简单名称格式: name
                else {
                    char* name_end = name_start;
                    while (*name_end && !isspace(*name_end) && *name_end != '#') name_end++;
                    *name_end = '\0';
                    if (strlen(name_start) > 0) {
                        strncpy(deps[*num_deps], name_start, MAX_PATH_LEN);
                        (*num_deps)++;
                    }
                }
            }
        }
    }
    
    fclose(toml_file);
    return strlen(project_name) > 0; // 返回是否成功解析了项目名称
}

// 创建CMakeLists.txt文件（带依赖项处理）
int create_cmakelists(const char* project_name, const char* project_type, char deps[][MAX_PATH_LEN], int num_deps) {
    FILE* cmake_file = fopen("CMakeLists.txt", "w");
    if (!cmake_file) {
        perror("创建CMakeLists.txt失败");
        return 0;
    }
    
    fprintf(cmake_file, "cmake_minimum_required(VERSION 3.10)\n");
    fprintf(cmake_file, "project(%s LANGUAGES CXX)\n\n", project_name);
    fprintf(cmake_file, "set(CMAKE_CXX_STANDARD 11)\n");
    fprintf(cmake_file, "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n");
    fprintf(cmake_file, "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n\n");
    
    // 添加PkgConfig支持
    if (num_deps > 0) {
        fprintf(cmake_file, "# 启用pkg-config\n");
        fprintf(cmake_file, "find_package(PkgConfig REQUIRED)\n");
    }
    
    // 处理依赖项
    for (int i = 0; i < num_deps; i++) {
        fprintf(cmake_file, "pkg_check_modules(%s REQUIRED %s)\n", deps[i], deps[i]);
    }
    
    if (num_deps > 0) {
        fprintf(cmake_file, "\n# 设置包含目录\n");
        fprintf(cmake_file, "include_directories(${PKG_CONFIG_INCLUDE_DIRS})\n");
        fprintf(cmake_file, "link_directories(${PKG_CONFIG_LIBRARY_DIRS})\n");
        fprintf(cmake_file, "add_definitions(${PKG_CONFIG_CFLAGS_OTHER})\n\n");
    }
    else if (strcmp(project_type, "library") != 0) {
        fprintf(cmake_file, "include_directories(include)\n\n");
    }
    
    if (strcmp(project_type, "executable") == 0) {
        fprintf(cmake_file, "set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)\n");
        fprintf(cmake_file, "add_executable(%s\n", project_name);
        fprintf(cmake_file, "    src/main.cpp\n");
        fprintf(cmake_file, ")\n");
        fprintf(cmake_file, "target_include_directories(%s PRIVATE ${CMAKE_SOURCE_DIR}/include)\n",project_name);
    } 
    else if (strcmp(project_type, "static") == 0) {
        fprintf(cmake_file, "set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/static)\n");
        fprintf(cmake_file, "add_library(%s STATIC\n", project_name);
        fprintf(cmake_file, "    src/%s.cpp\n", project_name);
        fprintf(cmake_file, ")\n");
        fprintf(cmake_file, "target_include_directories(%s PRIVATE ${CMAKE_SOURCE_DIR}/include)\n",project_name);
    } 
    else if (strcmp(project_type, "shared") == 0) {
        fprintf(cmake_file, "set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib/shared)\n");
        fprintf(cmake_file, "add_library(%s SHARED\n", project_name);
        fprintf(cmake_file, "    src/%s.cpp\n", project_name);
        fprintf(cmake_file, ")\n");
        fprintf(cmake_file, "target_include_directories(%s PRIVATE ${CMAKE_SOURCE_DIR}/include)\n",project_name);
        
        // 安装规则（跨平台）
        fprintf(cmake_file, "\n# 安装规则\n");
        fprintf(cmake_file, "install(TARGETS %s\n", project_name);
        fprintf(cmake_file, "    RUNTIME DESTINATION bin\n");
        fprintf(cmake_file, "    LIBRARY DESTINATION lib\n");
        fprintf(cmake_file, "    ARCHIVE DESTINATION lib\n");
        fprintf(cmake_file, ")\n");
        fprintf(cmake_file, "install(FILES include/%s.h DESTINATION include)\n", project_name);
    }
    
    // 链接依赖库
    if (num_deps > 0) {
        fprintf(cmake_file, "\n# 链接依赖库\n");
        fprintf(cmake_file, "target_link_libraries(%s PRIVATE\n", project_name);
        for (int i = 0; i < num_deps; i++) {
            fprintf(cmake_file, "    ${%s_LIBRARIES}\n", deps[i]);
        }
        fprintf(cmake_file, ")\n");
    }
    
    fclose(cmake_file);
    return 1;
}

int create_directory(const char* path) {
    if (MKDIR(path) == -1) {
        if (errno != EEXIST) {
            perror("创建目录失败");
            return 0;
        }
    }
    return 1;
}

// 创建初始的main.cpp文件
int create_main_cpp_file() {
    FILE* main_file = fopen("src/main.cpp", "w");
    if (!main_file) {
        perror("创建main.cpp失败");
        return 0;
    }
    
    fprintf(main_file, "#include <iostream>\n\n");
    fprintf(main_file, "int main() {\n");
    fprintf(main_file, "    std::cout << \"Hello, World!\" << std::endl;\n");
    fprintf(main_file, "    return 0;\n");
    fprintf(main_file, "}\n");
    fclose(main_file);
    return 1;
}

// 创建库源文件和头文件
int create_library_files(const char* project_name) {
    // 创建源文件
    char src_filename[MAX_PATH_LEN];
    snprintf(src_filename, MAX_PATH_LEN, "src/%s.cpp", project_name);
    FILE* src_file = fopen(src_filename, "w");
    if (!src_file) {
        perror("创建库源文件失败");
        return 0;
    }
    
    fprintf(src_file, "#include \"%s.h\"\n\n", project_name);
    fprintf(src_file, "int %s_function() {\n", project_name);
    fprintf(src_file, "    return 0;\n");
    fprintf(src_file, "}\n");
    fclose(src_file);
    
    // 创建头文件
    char header_filename[MAX_PATH_LEN];
    snprintf(header_filename, MAX_PATH_LEN, "include/%s.h", project_name);
    FILE* header_file = fopen(header_filename, "w");
    if (!header_file) {
        perror("创建头文件失败");
        return 0;
    }
    
    // 防止头文件重复包含的保护宏
    char guard[MAX_PATH_LEN];
    snprintf(guard, MAX_PATH_LEN, "%s_H", project_name);
    for (char *p = guard; *p; p++) {
        if (*p >= 'a' && *p <= 'z') *p -= 'a' - 'A';
        else if (*p == '-' || *p == '.') *p = '_';
    }
    
    fprintf(header_file, "#ifndef %s\n", guard);
    fprintf(header_file, "#define %s\n\n", guard);
    fprintf(header_file, "int %s_function();\n\n", project_name);
    fprintf(header_file, "#endif // %s\n", guard);
    
    fclose(header_file);
    return 1;
}


uint8_t create_new_project(int argc,char*argv[]){
    char project_name[MAX_PATH_LEN] = "my_project";
    char project_type[15] = "executable";
    char deps_from_cli[MAX_DEPS][MAX_PATH_LEN];
    int num_deps_cli = 0;
    uint8_t project_name_set = 0;
    uint8_t create_project = 0;

    if(2==argc){
        create_project++;
    }
    if(2<argc && argv[2][0]!='-'){
        create_project++;
        project_name_set = 1;
        strcpy(project_name,argv[2]);
    }
    else{
        create_project++;
    }
    
    // 解析命令行参数
    for(size_t i = 2; i < argc; i++){
        if(!strcmp(argv[i], "-e") || !strcmp(argv[i], "--executable")) {
            strcpy(project_type, "executable");
        }
        else if(!strcmp(argv[i], "-s") || !strcmp(argv[i], "--static")) {
            strcpy(project_type, "static");
        }
        else if(!strcmp(argv[i], "-d") || !strcmp(argv[i], "--shared")) {
            strcpy(project_type, "shared");
        }
        else if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }
        else if((!strcmp(argv[i], "-D") || !strcmp(argv[i], "--dep")) && i+1 < argc) {
            // 获取依赖项名称
            i++;
            if(num_deps_cli < MAX_DEPS) {
                strncpy(deps_from_cli[num_deps_cli], argv[i], MAX_PATH_LEN);
                num_deps_cli++;
            } else {
                printf("警告: 已达到最大依赖项数量(%d)，忽略依赖项: %s\n", MAX_DEPS, argv[i]);
            }
        }
    }

    if(!project_name_set){
        printf("未设置项目名称,使用默认名称%s\n",project_name);
    }
    printf("项目名称为 : %s\n",project_name);
    printf("项目类型为 : %s (%s)\n",project_name, 
           strcmp(project_type, "executable") == 0 ? "可执行文件" :
           strcmp(project_type, "static") == 0 ? "静态库" : "动态库");
    
    // 显示命令行添加的依赖项
    if(num_deps_cli > 0) {
        printf("命令行添加的依赖项: ");
        for(int i = 0; i < num_deps_cli; i++) {
            printf("%s ", deps_from_cli[i]);
        }
        printf("\n");
    }
    
    if(!create_directory(project_name)){
        return EXIT_FAILURE;
    }

    if(CHDIR(project_name)!=0){
        perror("无法进入项目目录");
        return EXIT_FAILURE;
    }

    if (!create_directory("src") || 
        !create_directory("include") || 
        !create_directory("build")) {
        return EXIT_FAILURE;
    }

    // 创建CMake.toml文件（包含命令行依赖项）
    if (!create_cmake_toml(project_name, project_type, deps_from_cli, num_deps_cli)) {
        return EXIT_FAILURE;
    }
    
    // 解析CMake.toml获取依赖项（包括命令行添加的）
    char deps[MAX_DEPS][MAX_PATH_LEN];
    int num_deps = 0;
    if (!parse_cmake_toml(project_name, project_type, deps, &num_deps)) {
        printf("警告 : 未能完全解析CMake.toml,使用默认配置\n");
    }
    else if (num_deps > 0) {
        printf("检测到依赖项: ");
        for (int i = 0; i < num_deps; i++) {
            printf("%s ", deps[i]);
        }
        printf("\n");
    }

    // 创建CMakeLists.txt文件（带依赖处理）
    if(!create_cmakelists(project_name, project_type, deps, num_deps)){
        return EXIT_FAILURE;
    }
    
    if (strcmp(project_type, "executable") == 0) {
        if (!create_main_cpp_file()) {
            return EXIT_FAILURE;
        }
    } 
    else {
        if (!create_library_files(project_name)) {
            return EXIT_FAILURE;
        }
    }

    // 输出成功信息
    printf("\n项目创建成功! 结构如下:\n");
    printf("%s%c\n", project_name, PATH_SEP);
    printf("├── CMakeLists.txt\n");
    printf("├── CMake.toml\n");
    printf("├── build%c\n", PATH_SEP);
    printf("├── include%c\n", PATH_SEP);
    if (strcmp(project_type, "static") == 0 || strcmp(project_type, "shared") == 0) {
        printf("│   └── %s.h\n", project_name);
    }
    printf("└── src%c\n", PATH_SEP);
    
    if (strcmp(project_type, "executable") == 0) {
        printf("    └── main.cpp\n");
    } else {
        printf("    └── %s.cpp\n", project_name);
    }

    printf("\n构建指南:\n");
    printf("  cd %s\n", project_name);
    printf("  cd build\n");
    
    if (strcmp(project_type, "executable") == 0) {
        printf("  cmake ..\n");
        printf("  cmake --build .\n");
        printf("  .%c%s%s\n", PATH_SEP, project_name, EXE_EXT);
    } 
    else if (strcmp(project_type, "static") == 0) {
        printf("  cmake ..\n");
        printf("  cmake --build .\n");
        printf("  # 静态库文件: build%clib%cstatic%c%s%s\n", 
               PATH_SEP, PATH_SEP, PATH_SEP, project_name, STATIC_LIB_EXT);
    } 
    else {
        printf("  cmake ..\n");
        printf("  cmake --build .\n");
        printf("  # 动态库文件: build%cbin%c%s%s (Windows) 或 build%clib%cshared%c%s\n", 
               PATH_SEP, PATH_SEP, project_name, SHARED_LIB_EXT, 
               PATH_SEP, PATH_SEP, PATH_SEP, SHARED_LIB_EXT);
    }
    
    if (num_deps > 0) {
        printf("\n注意 : 本项目的依赖项需要通过系统包管理器安装\n");
#if defined(PLATFORM_WINDOWS)
        printf("      请使用 vcpkg 安装依赖项\n");
#elif defined(PLATFORM_MACOS)
        printf("      请使用 Homebrew 安装依赖项\n");
#else
        printf("      请使用 apt-get/yum 安装依赖项\n");
#endif
    }
    
    return EXIT_SUCCESS;

}

uint8_t init_project(int argc,char*argv[]){
    char project_name[MAX_PATH_LEN] = "my_project";
    char project_type[15] = "executable";
    char deps[MAX_DEPS][MAX_PATH_LEN];
    int num_deps = 0;

    // 尝试从CMake.toml获取项目名称
    if (parse_cmake_toml(project_name, project_type, deps, &num_deps)) {
        printf("从CMake.toml获取项目名称: %s\n", project_name);
        printf("从CMake.toml获取项目类型: %s\n", project_type);
        if (num_deps > 0) {
            printf("检测到依赖项: ");
            for (int i = 0; i < num_deps; i++) {
                printf("%s ", deps[i]);
            }
            printf("\n");
        }
    } 
    else {
        printf("无法打开CMake.toml或解析失败\n");
        return EXIT_FAILURE;
    }

    printf("\n在当前目录初始化项目: %s (%s)\n", project_name, 
           strcmp(project_type, "executable") == 0 ? "可执行文件" :
           strcmp(project_type, "static") == 0 ? "静态库" : "动态库");

    // 创建必要的子目录
    struct stat st;
    memset(&st, 0, sizeof(st)); // 修复初始化方式
    
    if (stat("src", &st) == -1 && !create_directory("src")) {
        return EXIT_FAILURE;
    }
    if (stat("include", &st) == -1 && !create_directory("include")) {
        return EXIT_FAILURE;
    }
    if (stat("build", &st) == -1 && !create_directory("build")) {
        return EXIT_FAILURE;
    }

    // 创建CMakeLists.txt文件（带依赖处理）
    if (!create_cmakelists(project_name, project_type, deps, num_deps)) {
        return EXIT_FAILURE;
    }

    // 创建源文件（如果不存在）
    if (strcmp(project_type, "executable") == 0) {
        if (stat("src/main.cpp", &st) == -1 && !create_main_cpp_file()) {
            return EXIT_FAILURE;
        }
    } 
    else {
        char src_file[MAX_PATH_LEN];
        snprintf(src_file, MAX_PATH_LEN, "src/%s.cpp", project_name);
        if (stat(src_file, &st) == -1 && !create_library_files(project_name)) {
            return EXIT_FAILURE;
        }
    }

    // 输出成功信息
    printf("\n项目初始化成功!\n");
    printf("已创建/更新以下文件:\n");
    printf("  CMakeLists.txt\n");
    if (stat("CMake.toml", &st) == -1) {
        printf("  CMake.toml (已创建)\n");
        create_cmake_toml(project_name, project_type, deps, num_deps);
    } else {
        printf("  CMake.toml (已更新)\n");
    }
    
    if (strcmp(project_type, "static") == 0 || strcmp(project_type, "shared") == 0) {
        printf("  include/%s.h\n", project_name);
        printf("  src/%s.cpp\n", project_name);
    } 
    else {
        printf("  src/main.cpp\n");
    }
    
    if (num_deps > 0) {
        printf("\n注意 : 本项目的依赖项需要通过系统包管理器安装\n");
#if defined(PLATFORM_WINDOWS)
        printf("      请使用 vcpkg 安装依赖项\n");
#elif defined(PLATFORM_MACOS)
        printf("      请使用 Homebrew 安装依赖项\n");
#else
        printf("      请使用 apt-get/yum 安装依赖项\n");
#endif
    }
    
    return EXIT_SUCCESS;
}

// 执行命令并检查状态
int execute_command(const char* command) {
    printf("执行命令: %s\n", command);
    
#if defined(PLATFORM_WINDOWS)
    // Windows下需要将参数传递给cmd
    char cmd[MAX_PATH_LEN * 3];
    snprintf(cmd, sizeof(cmd), "cmd /c \"%s\"", command);
    int status = system(cmd);
#else
    int status = system(command);
#endif
    
    if (status == -1) {
        perror("命令执行失败");
        return 0;
    }
    
#if !defined(PLATFORM_WINDOWS)
    if (WIFEXITED(status)) {
        int exit_status = WEXITSTATUS(status);
        if (exit_status != 0) {
            fprintf(stderr, "命令退出代码: %d\n", exit_status);
            return 0;
        }
    } 
    else {
        fprintf(stderr, "命令异常终止\n");
        return 0;
    }
#endif
    
    return 1;
}

uint8_t build_project(int argc, char* argv[]) {
    char cmake_build_type[16] = "Debug"; // 使用更安全的长度
    char make_install_prefix[MAX_PATH_LEN] = ""; // 跨平台前缀初始化
    char build_dir[MAX_PATH_LEN] = "build";
    char additional_flags[1024] = "";
    bool configure_only = false;

    // 设置默认安装路径
#if PLATFORM_WINDOWS
    strcpy(make_install_prefix, ".\\install"); // Windows默认安装路径
#else
    // Unix默认安装路径
    const char* home = getenv("HOME");
    if (home) {
        snprintf(make_install_prefix, MAX_PATH_LEN, "%s/.local", home);
    } else {
        strcpy(make_install_prefix, "/usr/local");
    }
#endif

    // 增强的参数解析
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--debug")) {
            strcpy(cmake_build_type, "Debug");
        } 
        else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--release")) {
            strcpy(cmake_build_type, "Release");
        } 
        else if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--prefix")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "错误：未指定安装目录\n");
                return EXIT_FAILURE;
            }
            if(argv[i+1][0]=='-'){
                fprintf(stderr, "错误：未指定安装目录,使用默认安装目录\n");
                continue;
            }
            strncpy(make_install_prefix, argv[++i], MAX_PATH_LEN - 1);
            make_install_prefix[MAX_PATH_LEN - 1] = '\0';
        } 
        else if (!strcmp(argv[i], "--configure-only") || !strcmp(argv[i], "-c")) {
            configure_only = true;
        } 
        else if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--build-dir")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "错误：未指定构建目录\n");
                return EXIT_FAILURE;
            }
            strncpy(build_dir, argv[++i], MAX_PATH_LEN - 1);
            build_dir[MAX_PATH_LEN - 1] = '\0';
        }
        else {
            // 收集额外的CMake参数
            if (additional_flags[0] != '\0') strcat(additional_flags, " ");
            strncat(additional_flags, argv[i], sizeof(additional_flags) - strlen(additional_flags) - 1);
        }
    }

    printf("构建模式: %s | 安装路径: %s\n", cmake_build_type, make_install_prefix);

    // 处理构建目录
    struct stat st;
    memset(&st, 0, sizeof(st));
    if (stat(build_dir, &st) == -1) {
        if (!create_directory(build_dir)) {
            fprintf(stderr, "创建构建目录失败: %s\n", build_dir);
            return EXIT_FAILURE;
        }
    }

    // 保存当前目录
    char cwd[MAX_PATH_LEN];
    if (!getcwd(cwd, sizeof(cwd))) {
        perror("无法获取当前目录");
        return EXIT_FAILURE;
    }

    // 进入构建目录
    if (CHDIR(build_dir) != 0) {
        perror("无法进入构建目录");
        fprintf(stderr, "目标目录: %s\n", build_dir);
        return EXIT_FAILURE;
    }

    char cmake_command[MAX_PATH_LEN * 3] = ""; // 三倍缓冲区确保安全
    bool need_configure = true;
    
    // 检查是否存在CMake缓存文件
    if (stat("CMakeCache.txt", &st) == 0) {
        // 尝试获取缓存的构建类型
        FILE* cache_file = fopen("CMakeCache.txt", "r");
        if (cache_file) {
            char line[256];
            char existing_type[16] = "";
            
            while (fgets(line, sizeof(line), cache_file)) {
                if (strstr(line, "CMAKE_BUILD_TYPE:STRING")) {
                    char* value = strchr(line, '=');
                    if (value) {
                        value++; // 跳过等号
                        // 提取值并去除换行符
                        size_t len = strcspn(value, "\r\n");
                        if (len < sizeof(existing_type)) {
                            strncpy(existing_type, value, len);
                            existing_type[len] = '\0';
                        }
                    }
                }
            }
            fclose(cache_file);
            
            if (strcmp(existing_type, cmake_build_type) == 0) {
                need_configure = false;
                printf("检测到现有的CMake缓存(构建类型相同),跳过配置阶段\n");
            } 
            else {
                printf("构建类型从 %s 变为 %s,需要重新配置\n", existing_type, cmake_build_type);
                need_configure = true;
            }
        }
    } 
    else {
        printf("未找到CMake缓存,需要进行配置\n");
    }

    // 配置阶段
    if (need_configure) {
        // 构建配置命令
        #if PLATFORM_WINDOWS
            // Windows路径需要特殊处理反斜杠
            char escaped_prefix[MAX_PATH_LEN * 2] = {0};
            char* pos = make_install_prefix;
            char* dest = escaped_prefix;
            while (*pos && (dest - escaped_prefix) < sizeof(escaped_prefix) - 2) {
                if (*pos == '\\') *dest++ = '\\'; // 对反斜杠进行转义
                *dest++ = *pos++;
            }
            *dest = '\0';
            
            snprintf(cmake_command, sizeof(cmake_command), 
                "cmake .. -DCMAKE_BUILD_TYPE=%s -DCMAKE_INSTALL_PREFIX=\"%s\" %s",
                cmake_build_type, escaped_prefix, additional_flags);
        #else
            snprintf(cmake_command, sizeof(cmake_command), 
                "cmake .. -DCMAKE_BUILD_TYPE=%s -DCMAKE_INSTALL_PREFIX=\"%s\" %s",
                cmake_build_type, make_install_prefix, additional_flags);
        #endif
        
        printf("配置CMake: %s\n", cmake_command);
        if (!execute_command(cmake_command)) {
            fprintf(stderr, "CMake配置失败\n");
            CHDIR(cwd); // 恢复原始目录
            return EXIT_FAILURE;
        }
    }

    // 构建阶段
    if (!configure_only) {
        char build_tool[128];
        #if PLATFORM_WINDOWS
            snprintf(build_tool, sizeof(build_tool), "cmake --build .");
        #else
            // 尝试获取核心数
            int core_count = sysconf(_SC_NPROCESSORS_ONLN);
            if (core_count > 0) {
                snprintf(build_tool, sizeof(build_tool), "cmake --build . --parallel %d", core_count);
            } else {
                strcpy(build_tool, "cmake --build .");
            }
        #endif

        printf("构建中: %s\n", build_tool);
        if (!execute_command(build_tool)) {
            fprintf(stderr, "构建失败\n");
            CHDIR(cwd); // 恢复原始目录
            return EXIT_FAILURE;
        }
    }

    // 返回原始目录
    if (CHDIR(cwd) != 0) {
        perror("返回原始目录失败");
        return EXIT_FAILURE;
    }

    printf("\n构建%s成功!\n", configure_only ? "配置" : "");
    return EXIT_SUCCESS;
}

uint8_t clean_project_cache() {
    char original_dir[4096];
    
    // 保存当前目录
    if (!getcwd(original_dir, sizeof(original_dir))) {
        perror("无法获取当前工作目录");
        return EXIT_FAILURE;
    }

    // 进入 build 目录
    if (CHDIR("build") != 0) {
        // 特殊处理：build目录不存在时视为已清理
        if (errno == ENOENT) {
            printf("build目录不存在,无需清理\n");
            create_directory("build");
            return EXIT_SUCCESS;
        }
        perror("无法进入build目录");
        return EXIT_FAILURE;
    }

    // 清理操作
#if defined(PLATFORM_WINDOWS)
    // Windows: 返回上级目录后删除整个build
    CHDIR("..");
    if (!execute_command("rmdir /S /Q build 2>NUL")) {
        printf("清理缓存失败\n");
        CHDIR(original_dir);  // 失败时恢复原始目录
        return EXIT_FAILURE;
    }
#else
    // POSIX: 使用更可靠的递归删除命令
    if (!execute_command("find . -delete 2>/dev/null || { rm -rf ./* && rm -rf .[!.]*; }")) {
        printf("清理缓存失败\n");
        CHDIR(original_dir);  // 失败时恢复原始目录
        return EXIT_FAILURE;
    }
    // 返回原始目录
    if (CHDIR(original_dir) != 0) {
        perror("无法返回工作目录");
        return EXIT_FAILURE;
    }
#endif

    printf("CMake缓存清理成功\n");
    return EXIT_SUCCESS;
}

uint8_t install_project(int argc, char* argv[]) {
    char install_path[MAX_PATH_LEN] = {0}; // 初始化路径缓冲区

#if defined(PLATFORM_WINDOWS)
    // Windows 默认安装路径
    const char* default_path = ".\\install";
#else
    // Unix 系统默认安装路径（包含 macOS 和 Linux）
    const char* default_path = getenv("HOME");
    char home_path[MAX_PATH_LEN] = {0};
    if (default_path) {
        snprintf(home_path, MAX_PATH_LEN, "%s/.local", default_path);
        default_path = home_path;
    } else {
        default_path = "/usr/local"; // 后备默认值
    }
#endif

    // 处理用户输入的安装路径
    if (argc > 2) {
        strncpy(install_path, argv[2], MAX_PATH_LEN - 1);
        install_path[MAX_PATH_LEN - 1] = '\0'; // 确保终止符
    } else {
        strncpy(install_path, default_path, MAX_PATH_LEN - 1);
        install_path[MAX_PATH_LEN - 1] = '\0';
    }

    // 尝试进入 build 目录
    if (CHDIR("build") != 0) {
        perror("无法进入build目录");
        return EXIT_FAILURE;
    }

    // 构建安装命令
    char command[MAX_PATH_LEN];
    snprintf(command, MAX_PATH_LEN, "cmake --install . --prefix \"%s\"", install_path);
    
    return execute_command(command);
}



// 包管理器类型检测
int detect_package_manager(char* manager, size_t size) {
    *manager = '\0';
    
#if defined(PLATFORM_WINDOWS)
    // 检测 WinGet
    if (system("where winget >nul 2>nul") == 0) {
        snprintf(manager, size, "winget");
        return 0;
    }
    // 检测 Chocolatey
    if (system("where choco >nul 2>nul") == 0) {
        snprintf(manager, size, "choco");
        return 0;
    }
    return -1;  // 未找到包管理器

#elif defined(PLATFORM_MACOS)
    // 检测 Homebrew
    if (system("command -v brew >/dev/null 2>&1") == 0) {
        snprintf(manager, size, "brew");
        return 0;
    }
    return -1;  // 未找到包管理器

#elif defined(PLATFORM_LINUX)
    // 检测 APT
    if (access("/usr/bin/apt", X_OK) == 0) {
        snprintf(manager, size, "apt");
        return 0;
    }
    // 检测 DNF (Fedora)
    if (access("/usr/bin/dnf", X_OK) == 0) {
        snprintf(manager, size, "dnf");
        return 0;
    }
    // 检测 YUM (CentOS/RHEL)
    if (access("/usr/bin/yum", X_OK) == 0) {
        snprintf(manager, size, "yum");
        return 0;
    }
    // 检测 Pacman (Arch)
    if (access("/usr/bin/pacman", X_OK) == 0) {
        snprintf(manager, size, "pacman");
        return 0;
    }
    return -1;  // 未找到包管理器

#endif
}

// 主卸载函数
int uninstall_thirdparty_library(int argc ,char* argv[]) {
    char lib_name[32];
    strcpy(lib_name,argv[2]);

    char manager[32] = {0};
    
    // 检测包管理器
    if (detect_package_manager(manager, sizeof(manager)) != 0) {
        fprintf(stderr, "错误: 未找到支持的包管理器\n");
        return -1;
    }

    printf("检测到包管理器: %s\n", manager);

    // 构建卸载命令
    char command[256] = {0};
    
#if defined(PLATFORM_WINDOWS)
    if (strcmp(manager, "winget") == 0) {
        snprintf(command, sizeof(command), "winget uninstall %s", lib_name);
    } else if (strcmp(manager, "choco") == 0) {
        snprintf(command, sizeof(command), "choco uninstall %s -y", lib_name);
    }

#elif defined(PLATFORM_MACOS)
    if (strcmp(manager, "brew") == 0) {
        snprintf(command, sizeof(command), "brew uninstall %s", lib_name);
    }

#elif defined(PLATFORM_LINUX)
    // 基于不同包管理器构建命令
    if (strcmp(manager, "apt") == 0) {
        snprintf(command, sizeof(command), "sudo apt remove -y %s", lib_name);
    } 
    else if (strcmp(manager, "dnf") == 0 || strcmp(manager, "yum") == 0) {
        snprintf(command, sizeof(command), "sudo %s remove -y %s", manager, lib_name);
    } 
    else if (strcmp(manager, "pacman") == 0) {
        snprintf(command, sizeof(command), "sudo pacman -R --noconfirm %s", lib_name);
    }
#endif

    return execute_command(command);
}

int main(int argc,char*argv[]){

#if defined(PLATFORM_WINDOWS)
    SetConsoleOutputCP(CP_UTF8);    
    SetConsoleCP(CP_UTF8);
#endif
    
    print_platform_info();

    if(argc==1){
        printf("提示: 使用 -h 查看帮助\n");
        print_usage(argv[0]);
        return EXIT_SUCCESS;
    }
    else{
        // 构建项目
        if(! strcmp("build",argv[1])){
            printf("开始构建...\n");
            return build_project(argc,argv);
        }
        
        // 根据解析的CMake.toml初始化项目
        else if(! strcmp("init",argv[1])){
            // 解析并且初始化CMake.toml
            return init_project(argc,argv);
        }

        // 清除cmake构建
        else if(! strcmp("clean",argv[1])){
            return clean_project_cache();
        }

        // 创建新的项目
        else if(! strcmp(argv[1],"new")){
            return create_new_project(argc,argv);
        }

        // 安装项目
        else if(! strcmp("install",argv[1])){
            return install_project(argc,argv) == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        else if(! strcmp("uninstall",argv[1])){
            return uninstall_thirdparty_library(argc,argv);
        }

        // 输出帮助消息
        else if(! strcmp(argv[1],"-h") || ! strcmp(argv[1],"--help")){
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }

        else{
            printf("无效参数: %s\n", argv[1]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }
    
    return EXIT_SUCCESS;
}     