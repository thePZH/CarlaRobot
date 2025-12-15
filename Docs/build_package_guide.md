# CARLA 打包流程完整指南

本文档详细说明 CARLA 项目的完整打包流程，包括自定义的 CMake 变量、函数和打包步骤。

## 目录

1. [概述](#概述)
2. [前置条件](#前置条件)
3. [CMake 自定义函数和宏](#cmake-自定义函数和宏)
4. [CMake 关键变量](#cmake-关键变量)
5. [打包流程详解](#打包流程详解)
6. [打包配置选项](#打包配置选项)
7. [打包命令](#打包命令)
8. [打包输出结构](#打包输出结构)
9. [常见问题](#常见问题)

---

## 概述

CARLA 使用 CMake 构建系统，并通过 Unreal Engine 的 UAT (Unreal Automation Tool) 进行打包。打包过程包括：

1. 编译 LibCarla 和 Python API
2. 编译 Unreal Engine 项目
3. 使用 UAT 进行 Cook、Stage 和 Archive
4. 创建版本文件
5. 复制额外文件
6. 压缩打包

---

## 前置条件

### 必需环境

- **CMake**: 版本 >= 3.27.2
- **Unreal Engine**: CARLA 定制版本的 Unreal Engine
- **Python**: 用于 Python API 构建
- **编译器**: 
  - Linux: GCC/Clang
  - Windows: Visual Studio

### 环境变量

```bash
# 设置 Unreal Engine 路径
export CARLA_UNREAL_ENGINE_PATH=/path/to/UnrealEngine
```

---

## CMake 自定义函数和宏

CARLA 定义了一系列自定义的 CMake 函数和宏，位于 `CMake/Util.cmake`。

### 消息函数

#### `carla_message(MESSAGE)`
输出普通状态消息，前缀为 "CARLA: "。

```cmake
carla_message("Building CARLA...")
```

#### `carla_message_verbose(MESSAGE)`
仅在 `VERBOSE_CONFIGURE` 开启时输出消息。

```cmake
carla_message_verbose("Detailed build info...")
```

#### `carla_warning(MESSAGE)`
输出警告消息。

```cmake
carla_warning("This is a warning")
```

#### `carla_error(MESSAGE)`
输出错误消息并终止配置。

```cmake
carla_error("Fatal error occurred")
```

### 选项宏

#### `carla_option(NAME DESCRIPTION DEFAULT_VALUE)`
定义布尔选项，并自动记录到文档中。

```cmake
carla_option(
  BUILD_CARLA_CLIENT
  "Build the CARLA client."
  ON
)
```

**参数：**
- `NAME`: 选项名称
- `DESCRIPTION`: 选项描述
- `DEFAULT_VALUE`: 默认值 (ON/OFF)

#### `carla_string_option(NAME DESCRIPTION DEFAULT_VALUE)`
定义字符串选项，并自动记录到文档中。

```cmake
carla_string_option(
  CARLA_UNREAL_ENGINE_PATH
  "Path to the CARLA fork of Unreal Engine."
  "${CARLA_UNREAL_ENGINE_PATH_INFERRED}"
)
```

**参数：**
- `NAME`: 选项名称
- `DESCRIPTION`: 选项描述
- `DEFAULT_VALUE`: 默认字符串值

### 目标函数

#### `carla_add_library(NAME DESCRIPTION ...)`
添加库目标，并记录到文档中。

```cmake
carla_add_library(
  carla-server
  "CARLA server library"
  ...
)
```

#### `carla_add_executable(NAME DESCRIPTION ...)`
添加可执行文件目标，并记录到文档中。

```cmake
carla_add_executable(
  carla-server
  "CARLA server executable"
  ...
)
```

#### `carla_add_custom_target(NAME DESCRIPTION ...)`
添加自定义目标，并记录到文档中。

```cmake
carla_add_custom_target(
  carla-unreal-package
  "Create a CARLA package"
  ...
)
```

### 工具函数

#### `carla_two_step_configure_file(DESTINATION SOURCE)`
两步配置文件：先进行 configure-time 变量替换，再进行 generate-time 生成器表达式替换。

```cmake
carla_two_step_configure_file(
  ${OUTPUT_FILE}
  ${TEMPLATE_FILE}
)
```

#### `carla_get_option_docs(OUT_VAR)`
获取所有选项的文档字符串。

```cmake
carla_get_option_docs(OPTION_DOCS)
message(STATUS "${OPTION_DOCS}")
```

#### `carla_get_target_docs(OUT_VAR)`
获取所有目标的文档字符串。

```cmake
carla_get_target_docs(TARGET_DOCS)
message(STATUS "${TARGET_DOCS}")
```

### 打包函数

#### `add_carla_ue_package_target(PACKAGE_CONFIGURATION UE_BUILD_CONFIGURATION)`
创建 Unreal Engine 打包目标。这是打包流程的核心函数。

**参数：**
- `PACKAGE_CONFIGURATION`: 打包配置名称（如 "Shipping", "Debug"），可为空字符串使用默认配置
- `UE_BUILD_CONFIGURATION`: Unreal Engine 构建配置（Shipping/Debug/DebugGame/Development/Test）

**功能：**
1. 设置打包名称：`Carla-${CARLA_VERSION}-${UE_SYSTEM_NAME}-${UE_BUILD_CONFIGURATION}`
2. 创建构建目标，执行 UAT BuildCookRun
3. 添加后处理步骤：
   - 创建版本文件
   - 移除 Unreal 额外文件
   - 复制 CARLA 额外文件
   - 压缩打包

**示例：**
```cmake
# 创建 Shipping 配置的打包目标
add_carla_ue_package_target("Shipping" "Shipping")

# 创建默认配置的打包目标
add_carla_ue_package_target("" ${CARLA_UNREAL_PACKAGE_BUILD_TYPE})
```

---

## CMake 关键变量

### 版本变量

```cmake
CARLA_VERSION_MAJOR          # 主版本号 (默认: 0)
CARLA_VERSION_MINOR          # 次版本号 (默认: 10)
CARLA_VERSION_PATCH          # 补丁版本号 (默认: 0)
CARLA_VERSION                # 完整版本号 (格式: MAJOR.MINOR.PATCH)
```

### 路径变量

```cmake
CARLA_WORKSPACE_PATH         # CARLA 工作空间根目录
CARLA_BUILD_PATH             # CMake 构建目录 (CMAKE_BINARY_DIR)
CARLA_PACKAGE_PATH           # 打包输出目录 (${CARLA_BUILD_PATH}/Package)
CARLA_UNREAL_ENGINE_PATH     # Unreal Engine 安装路径
CARLA_UNREAL_PLUGINS_PATH    # Unreal 插件路径
CARLA_UE_PROJECT_PATH        # Unreal 项目文件路径 (.uproject)
```

### 打包相关变量

```cmake
CARLA_PACKAGE_NAME           # 打包名称 (格式: Carla-VERSION-SYSTEM-CONFIG)
CARLA_CURRENT_PACKAGE_PATH   # 当前打包路径
CARLA_PACKAGE_ARCHIVE_PATH   # 打包归档路径
CARLA_PACKAGE_STAGING_PATH   # 打包暂存路径
CARLA_UNREAL_PACKAGE_BUILD_TYPE  # 默认打包构建类型
```

### 系统变量

```cmake
UE_SYSTEM_NAME               # 系统名称 (Linux/Windows)
EXE_EXT                      # 可执行文件扩展名 (.exe 或空)
```

---

## 打包流程详解

打包流程由 `add_carla_ue_package_target` 函数定义，包含以下步骤：

### 步骤 1: 编译 Unreal 项目

使用 Unreal Build Tool (UBT) 编译项目：

```cmake
${CARLA_UE_BUILD_COMMAND_PREFIX}
CarlaUnreal
${UE_SYSTEM_NAME}
${UE_BUILD_CONFIGURATION}
-project=${CARLA_UE_PROJECT_PATH}
-game
-buildscw
```

**参数说明：**
- `-game`: 构建游戏可执行文件
- `-buildscw`: 构建 ShaderCompileWorker

### 步骤 2: UAT BuildCookRun

使用 Unreal Automation Tool (UAT) 进行 Cook、Stage 和 Archive：

```cmake
${CARLA_UE_UAT_COMMAND_PREFIX}
BuildCookRun
-project=${CARLA_UE_PROJECT_PATH}
-nocompileeditor
-nop4
-cook              # Cook 内容
-stage             # Stage 文件
-archive           # 归档到指定目录
-package           # 打包
-iterate
-clientconfig=${UE_BUILD_CONFIGURATION}
-TargetPlatform=${UE_SYSTEM_NAME}
-Platform=${UE_SYSTEM_NAME}
-prereqs
-build
-stagingdirectory=${CARLA_PACKAGE_STAGING_PATH}
-archivedirectory=${CARLA_PACKAGE_ARCHIVE_PATH}
```

**关键参数：**
- `-cook`: 将内容资源转换为平台特定格式
- `-stage`: 将文件复制到暂存目录
- `-archive`: 将暂存文件归档到输出目录
- `-clientconfig`: 客户端构建配置
- `-stagingdirectory`: 暂存目录路径
- `-archivedirectory`: 归档输出目录

### 步骤 3: 创建版本文件

执行 `Unreal/Package/CreateCarlaVersionFile.cmake`，创建 `VERSION` 文件，包含：

- CARLA git hash
- Content git hash
- Unreal Engine git hash

**文件位置：** `${CARLA_PACKAGE_ARCHIVE_PATH}/VERSION`

### 步骤 4: 移除 Unreal 额外文件

执行 `Unreal/Package/RemoveUnrealPackageExtraFiles.cmake`，移除：

- `Manifest_NonUFSFiles_Linux.txt`
- `Manifest_UFSFiles_Linux.txt`
- `Manifest_DebugFiles_Linux.txt`

### 步骤 5: 复制 CARLA 额外文件

执行 `Unreal/Package/CopyCarlaAdditionalFiles.cmake`，复制以下文件：

**文档文件：**
- `LICENSE` → `${CARLA_PACKAGE_ARCHIVE_PATH}/LICENSE`
- `CHANGELOG.md` → `${CARLA_PACKAGE_ARCHIVE_PATH}/CHANGELOG`
- `Docs/release_readme.md` → `${CARLA_PACKAGE_ARCHIVE_PATH}/README`

**工具：**
- `RecastBuilder` → `${CARLA_PACKAGE_ARCHIVE_PATH}/Tools/RecastBuilder`

**Python API：**
- Python wheel 文件 → `${CARLA_PACKAGE_ARCHIVE_PATH}/PythonAPI/carla/dist/`
- Python API 文档 → `${CARLA_PACKAGE_ARCHIVE_PATH}/PythonAPI/python_api.md`
- Python agents → `${CARLA_PACKAGE_ARCHIVE_PATH}/PythonAPI/carla/agents/`
- Python 示例 → `${CARLA_PACKAGE_ARCHIVE_PATH}/PythonAPI/examples/`
- Python ROS2 示例 → `${CARLA_PACKAGE_ARCHIVE_PATH}/PythonAPI/examples/ros2/`
- Python 工具 → `${CARLA_PACKAGE_ARCHIVE_PATH}/PythonAPI/util/`

### 步骤 6: 压缩打包

执行 `Unreal/Package/Compress.cmake`，创建压缩包：

**Linux:**
```bash
tar -cvfz ${CARLA_PACKAGE_NAME}.tar.gz ${CARLA_PACKAGE_FILES}
```

**Windows:**
```bash
tar -cvf ${CARLA_PACKAGE_NAME}.zip --format=zip ${CARLA_PACKAGE_FILES}
```

**输出位置：** `${CARLA_PACKAGE_PATH}/${CARLA_PACKAGE_NAME}.tar.gz` 或 `.zip`

---

## 打包配置选项

### 主要构建选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `BUILD_CARLA_CLIENT` | bool | ON | 构建 CARLA 客户端 |
| `BUILD_CARLA_SERVER` | bool | ON | 构建 CARLA 服务器 |
| `BUILD_PYTHON_API` | bool | ON | 构建 Python API |
| `BUILD_CARLA_UNREAL` | bool | 自动 | 构建 Unreal 项目（需要 Unreal Engine 路径） |
| `ENABLE_ROS2` | bool | OFF | 启用 ROS2 支持 |
| `ENABLE_ROS2_DEMO` | bool | OFF | 启用 ROS2 演示 |

### Unreal Engine 相关选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `CARLA_UNREAL_ENGINE_PATH` | string | 环境变量 | Unreal Engine 安装路径 |
| `CARLA_UNREAL_BUILD_TYPE` | string | "Development" | Unreal Editor 构建类型 |
| `CARLA_UNREAL_PACKAGE_BUILD_TYPE` | string | 自动 | 打包构建类型（根据 CMAKE_BUILD_TYPE 自动选择） |
| `CARLA_UNREAL_RHI` | string | 平台相关 | 渲染硬件接口 (Linux: vulkan, Windows: d3d12) |
| `CARLA_UNREAL_LOG_WINDOW` | bool | ON | 是否打开日志窗口 |

### 依赖版本选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `CARLA_BOOST_VERSION` | string | "1.84.0" | Boost 版本 |
| `CARLA_EIGEN_VERSION` | string | "3.4.0" | Eigen 版本 |
| `CARLA_GTEST_VERSION` | string | "1.14.0" | Google Test 版本 |
| `CARLA_RECAST_TAG` | string | "carla" | Recast Navigation git tag |
| `CARLA_RPCLIB_TAG` | string | "carla" | rpclib git tag |

### 其他选项

| 选项 | 类型 | 默认值 | 说明 |
|------|------|--------|------|
| `VERBOSE_CONFIGURE` | bool | OFF | 详细配置输出 |
| `ENABLE_ALL_WARNINGS` | bool | OFF | 启用所有编译警告 |
| `ENABLE_WARNINGS_TO_ERRORS` | bool | OFF | 将警告视为错误 |
| `ENABLE_RTTI` | bool | ON | 启用 C++ RTTI |
| `ENABLE_EXCEPTIONS` | bool | ON | 启用 C++ 异常 |

---

## 打包命令

### 基本打包流程

#### 1. 配置 CMake

```bash
mkdir -p build
cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCARLA_UNREAL_ENGINE_PATH=/path/to/UnrealEngine
```

#### 2. 编译项目

```bash
# 编译所有目标（包括 LibCarla、Python API 等）
cmake --build . --config Release

# 或者使用 make (Linux)
make -j$(nproc)
```

#### 3. 执行打包

```bash
# 打包 Shipping 配置
cmake --build . --target carla-unreal-package-shipping

# 打包默认配置（根据 CARLA_UNREAL_PACKAGE_BUILD_TYPE）
cmake --build . --target carla-unreal-package

# 打包其他配置
cmake --build . --target carla-unreal-package-debug
cmake --build . --target carla-unreal-package-development
```

### 可用的打包目标

打包目标命名格式：`carla-unreal-package[-CONFIG]`

| 目标名称 | 配置 | 说明 |
|----------|------|------|
| `carla-unreal-package` | 默认 | 使用 `CARLA_UNREAL_PACKAGE_BUILD_TYPE` |
| `carla-unreal-package-shipping` | Shipping | 发布版本（优化、无调试信息） |
| `carla-unreal-package-debug` | Debug | 调试版本（包含调试信息） |
| `carla-unreal-package-debuggame` | DebugGame | 游戏调试版本 |
| `carla-unreal-package-development` | Development | 开发版本 |
| `carla-unreal-package-test` | Test | 测试版本 |

### 快捷目标

每个打包配置还有一个简短的别名：

- `package` → `carla-unreal-package`
- `package-shipping` → `carla-unreal-package-shipping`
- `package-debug` → `carla-unreal-package-debug`
- 等等...

---

## 打包输出结构

### 打包目录结构

打包完成后，输出目录结构如下：

```
Build/Package/
├── Carla-0.10.0-Linux-Shipping/          # 打包根目录
│   ├── CarlaUE5.sh                      # 启动脚本 (Linux)
│   ├── CarlaUE5                          # 可执行文件
│   ├── VERSION                           # 版本信息文件
│   ├── LICENSE                           # 许可证
│   ├── CHANGELOG                         # 更新日志
│   ├── README                            # 说明文档
│   ├── Tools/
│   │   └── RecastBuilder                 # 导航网格构建工具
│   ├── PythonAPI/
│   │   ├── carla/
│   │   │   ├── dist/
│   │   │   │   └── carla-*.whl           # Python wheel 包
│   │   │   ├── agents/                   # 智能体代码
│   │   │   └── ...
│   │   ├── examples/                     # Python 示例
│   │   └── util/                         # Python 工具
│   └── [Unreal Engine 打包内容]
└── Carla-0.10.0-Linux-Shipping.tar.gz    # 压缩包
```

### VERSION 文件内容

```
Carla git hash:         <CARLA_GIT_HASH>
Content git hash:       <CONTENT_GIT_HASH>
UnrealEngine git hash:  <UNREAL_ENGINE_GIT_HASH>
```

---

## 常见问题

### 1. 打包名称格式

打包名称格式为：`Carla-${CARLA_VERSION}-${UE_SYSTEM_NAME}-${UE_BUILD_CONFIGURATION}`

**示例：**
- Linux Shipping: `Carla-0.10.0-Linux-Shipping`
- Windows Debug: `Carla-0.10.0-Win64-Debug`

### 2. 修改打包名称

要修改打包名称，需要修改 `Unreal/CMakeLists.txt` 中的 `CARLA_PACKAGE_NAME` 设置：

```cmake
set (
  CARLA_PACKAGE_NAME
  YourCustomName-${CARLA_VERSION}-${UE_SYSTEM_NAME}-${UE_BUILD_CONFIGURATION}
)
```

### 3. 打包失败：缺少 Unreal Engine

**错误：**
```
Could not add UE project to build since CARLA_UNREAL_ENGINE_PATH is not set
```

**解决：**
```bash
export CARLA_UNREAL_ENGINE_PATH=/path/to/UnrealEngine
# 或在 CMake 配置时指定
cmake .. -DCARLA_UNREAL_ENGINE_PATH=/path/to/UnrealEngine
```

### 4. 打包时间过长

打包过程包括：
- 编译 C++ 代码
- Cook 内容资源（可能很耗时）
- 复制文件

**优化建议：**
- 使用多核编译：`make -j$(nproc)`
- 确保有足够的磁盘空间
- 使用 SSD 存储

### 5. Python API 未包含在打包中

确保在配置时启用了 `BUILD_PYTHON_API`：

```bash
cmake .. -DBUILD_PYTHON_API=ON
```

### 6. 查看详细构建信息

启用详细输出：

```bash
cmake .. -DVERBOSE_CONFIGURE=ON
cmake --build . --verbose
```

### 7. 清理构建

```bash
# 清理构建目录
rm -rf build

# 或使用 CMake 清理目标
cmake --build . --target clean
```

---

## 相关文件

### CMake 文件

- `CMakeLists.txt` - 主 CMake 配置文件
- `CMake/Util.cmake` - 工具函数和宏
- `CMake/Options.cmake` - 配置选项定义
- `CMake/Common.cmake` - 通用配置
- `CMake/Dependencies.cmake` - 依赖管理
- `Unreal/CMakeLists.txt` - Unreal 项目配置

### 打包脚本

- `Unreal/Package/Compress.cmake` - 压缩脚本
- `Unreal/Package/CreateCarlaVersionFile.cmake` - 版本文件创建
- `Unreal/Package/CopyCarlaAdditionalFiles.cmake` - 文件复制
- `Unreal/Package/RemoveUnrealPackageExtraFiles.cmake` - 文件清理

### 文档

- `Docs/build_system.md` - 构建系统文档
- `Docs/build_linux_ue5.md` - Linux 构建指南
- `Docs/build_windows_ue5.md` - Windows 构建指南

---

## 总结

CARLA 的打包流程是一个多步骤的过程，涉及：

1. **CMake 配置** - 设置构建选项和路径
2. **编译** - 构建 LibCarla、Python API 和 Unreal 项目
3. **UAT 打包** - 使用 Unreal Automation Tool 进行 Cook、Stage 和 Archive
4. **后处理** - 创建版本文件、复制额外文件、清理
5. **压缩** - 创建最终的压缩包

通过理解这些步骤和相关的 CMake 函数、变量，可以更好地定制和调试打包流程。

---

**最后更新：** 2024年
**CARLA 版本：** 0.10.0


