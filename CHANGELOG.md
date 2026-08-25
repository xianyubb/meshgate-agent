# Changelog

## [0.1.0] - 2026-08-26

首个发行版本。

### meshgate-agent

- 从 MeshGate Login 的 `DeviceModel` 或 `DeviceId` carrier 读取玩家 XUID。
- 在 BDS Login 校验和创建玩家阶段恢复 XUID、UUID、PlayFabId、Xbox Live 名称和公钥信息。
- 将恢复后的玩家身份标记为完整认证类型，供 BDS 正确建立玩家身份。
- 增加 Login 身份 carrier、字段结构和注入阶段的诊断日志。
- 新增 GitHub Actions `Build`（push/PR 构建）与 `Release`（tag 推送自动发布）工作流。
- 修正 `tooth.json` 仓库地址为 `github.com/xianyubb/meshgate-agent`，使 lip 安装指向实际 Release 资产。

### Verification

- 使用 xmake 完成 Windows x64 Release 构建。
