# Sevnce Services 重构说明

## 概述
本次重构将CarlaServer.cpp中的lambda函数逻辑提取到独立的静态服务类中，提高代码的可维护性和可测试性。

## 创建的文件结构
```
Carla/Source/Carla/Services/
├── SevnceActorLogic.h/cpp     # Actor生成和管理相关服务
├── SevnceVehicleLogic.h/cpp   # 车辆控制相关服务
└── README.md                  # 本说明文档
```

## 重构的函数

### 1. SvcActorLogic 类
**处理函数：**
- `spawn_actor` - 生成Actor
- `spawn_actor_with_parent` - 生成带父Actor的Actor

**主要功能：**
- Actor生成逻辑
- 父子关系设置
- 特殊挂载处理（骨骼网格插槽）
- ROS2父Actor名称处理
- 大地图管理器集成

### 2. SvcVehicleLogic 类
**处理函数：**
- `apply_control_to_vehicle` - 应用车辆控制

**主要功能：**
- 车辆控制应用
- 错误处理和日志记录

## 设计原则

### 1. 静态函数设计
- 所有服务类方法都是静态函数
- 不依赖类实例状态
- 便于单元测试

### 2. 返回值设计
- 静态函数返回原始数据类型（如FCarlaActor*, ECarlaServerResponse）
- lambda函数负责用R<>包装返回值
- 保持原有的错误处理机制

### 3. 错误处理
- 静态函数内部进行详细的错误日志记录
- 返回适当的错误状态码
- lambda函数根据返回状态决定是否调用RESPOND_ERROR

## 使用方式

### 在CarlaServer.cpp中的调用示例：
```cpp
// 原来的lambda函数体
FCarlaActor* Result = SvcActorLogic::SpawnActor(Episode, Description, Transform);
if (!Result)
{
    RESPOND_ERROR("Failed to spawn actor");
}
return Episode->SerializeActor(Result);
```

## 扩展指南

### 添加新的服务类
1. 在Services文件夹中创建新的.h/.cpp文件
2. 按照命名规范：Svc[功能名]Logic
3. 实现静态函数，遵循相同的设计原则
4. 在CarlaServer.cpp中包含头文件并调用

### 命名规范
- 类名：Svc[功能名]Logic
- 函数名：动词+名词，如SpawnActor, ApplyControlToVehicle
- 变量名：使用m_前缀表示成员变量，小驼峰表示局部变量

## 优势
1. **代码分离**：业务逻辑与RPC绑定分离
2. **可测试性**：静态函数易于单元测试
3. **可维护性**：逻辑集中，便于修改和扩展
4. **可重用性**：静态函数可在其他地方调用
5. **错误处理**：统一的错误处理和日志记录
