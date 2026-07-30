# meshgate-agent

meshgate-agent 是 MeshGate 的 LeviLamina 原生插件。它在后端 BDS 内注册跨服命令，并把玩家的跨服请求发送给 MeshGate 的本地控制接口。

默认命令：

```text
/meshgate <target>
```

其中 `<target>` 必须是 MeshGate 配置文件里 `[[servers]].id` 的值，例如：

```text
/meshgate survival-02
```

## 工作方式

玩家在 BDS 内执行命令后，插件会读取玩家身份信息，并向 MeshGate 发送请求：

```http
POST /handoff
Authorization: Bearer <bearerToken>
Content-Type: application/json

{"xuid":"<player-xuid>","targetServer":"<target>"}
```

MeshGate 收到请求后负责处理实际跨服流程。插件本身不直接连接目标 BDS，也不负责转发 Bedrock 数据包。

## 环境要求

- Windows x64
- Minecraft Bedrock Dedicated Server 1.26.20.04
- LeviLamina
- xmake
- Visual Studio Build Tools 或可用的 clang-cl 工具链
- Git

后端 BDS 需要关闭在线验证：

```properties
online-mode=false
```

建议同时设置：

```properties
client-side-chunk-generation-enabled=false
```

## 构建方法

在 `meshgate-agent` 仓库目录执行：

```powershell
xmake f -y -p windows -a x64 -m release
xmake -y
```

构建产物会生成在：

```text
bin/meshgate-agent/
```

如果需要清理后重新构建：

```powershell
xmake clean
xmake -y
```

## 安装方法

把构建输出目录复制到 LeviLamina 服务端插件目录：

```text
bin/meshgate-agent/
```

复制到：

```text
plugins/meshgate-agent/
```

最终目录结构类似：

```text
plugins/
  meshgate-agent/
    manifest.json
    meshgate-agent.dll
    README.md
    config.example.json
```

首次加载后，插件会创建配置文件：

```text
plugins/meshgate-agent/config/config.json
```

## 配置方法

示例配置：

```json
{
  "apiHost": "127.0.0.1",
  "apiPort": 31920,
  "bearerToken": "replace-with-the-same-token-as-meshgate",
  "command": "meshgate"
}
```

字段说明：

- `apiHost`：MeshGate 控制接口地址，通常为 `127.0.0.1`。
- `apiPort`：MeshGate 控制接口端口，需要和 MeshGate 的 `api_port` 一致。
- `bearerToken`：控制接口认证令牌，需要和 MeshGate 的 `bearer_token` 完全一致。
- `command`：游戏内命令名，默认是 `meshgate`。

MeshGate 主程序配置中的对应字段：

```toml
api_host = "127.0.0.1"
api_port = 31920
bearer_token = "replace-with-a-random-token-of-at-least-32-characters"
```

## 使用方法

1. 启动 MeshGate。
2. 启动安装了 `meshgate-agent` 的后端 BDS。
3. 玩家通过 MeshGate 入口进入服务器。
4. 在游戏内执行：

```text
/meshgate survival-02
```

如果目标服务器不存在、令牌错误、玩家身份为空或目标服务器就是当前服务器，插件或 MeshGate 会拒绝本次请求。

## 已使用的库和工具

- [LiteLDev/LeviLamina](https://github.com/LiteLDev/LeviLamina)：BDS 原生插件框架。
- [LiteLDev/xmake-repo](https://github.com/LiteLDev/xmake-repo)：LeviLamina xmake 包仓库。
- LeviBuildScript：LeviLamina 插件构建和打包规则。
- xmake：项目构建系统。
- Winsock2 / `ws2_32`：Windows HTTP/TCP 请求支持。
- C++20 标准库。

## 鸣谢

特别感谢：

- LiteLDev / LeviLamina 社区
- MeshGate 主程序参与者

## 许可证

本项目使用 MIT License 开源，详情见 [LICENSE](LICENSE)。

项目使用到的第三方库和工具仍遵循其各自的许可证；发布插件包或整合分发时，请同时保留相关依赖项目的许可证声明。
