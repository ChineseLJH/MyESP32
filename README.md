# ESP32 项目展示

这里收集的是我做过的 ESP32 / ESP-IDF 项目、实验和练习代码，重点展示具体功能和实现内容，而不是单纯的空壳工程。每个目录都是一个独立项目，可以单独编译、烧录和调试。

## 项目一览

### [Android_Remote_Control_ESP32S3](Android_Remote_Control_ESP32S3)

ESP32-S3 的安卓远程控制项目，核心内容是设备联网、远程交互和控制逻辑。适合展示手机端和 ESP32 端之间的通信设计。

### [S3-WebUART-Bridge](S3-WebUART-Bridge)

WebSocket 和 UART 桥接示例，重点是把浏览器或网络侧的消息转发到串口设备。这个项目更偏向网络协议、实时数据转发和调试串口联动。

### [ESP_Architect_Labs](ESP_Architect_Labs)

ESP-IDF 架构实验合集，里面包含多个主题练习，比如构建系统、组件化、存储布局、多核、中断和调试等。这个目录更适合作为技术学习和实验记录。

### [ESP32-C3-hello_world](ESP32-C3-hello_world)

ESP32-C3 的基础示例工程，用于验证环境、工具链和最小运行流程。

### [esp32s3_hello_world](esp32s3_hello_world)

ESP32-S3 的基础示例工程，适合作为 S3 平台的入门参考和环境检查项目。

## 这个仓库能看到什么

- ESP32-S3 应用开发。
- WebSocket、UART、网络通信相关实践。
- ESP-IDF 构建、组件化和工程组织方式。
- 存储、并发、调试等系统级实验内容。

## 快速浏览建议

如果你想先看“项目做了什么”，建议直接进入对应目录查看 `main/` 和该项目自己的 `README.md`。如果你想看工程整体结构，可以先看根目录下的各项目索引，再按兴趣进入具体项目。