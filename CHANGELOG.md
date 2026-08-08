# Changelog

## [Unreleased] - 2026-08-09

### meshgate-agent

- 从 MeshGate Login 的 `DeviceModel` 或 `DeviceId` carrier 读取玩家 XUID。
- 在 BDS Login 校验和创建玩家阶段恢复 XUID、UUID、PlayFabId、Xbox Live 名称和公钥信息。
- 将恢复后的玩家身份标记为完整认证类型，供 BDS 正确建立玩家身份。
- 增加 Login 身份 carrier、字段结构和注入阶段的诊断日志。

### Verification

- 使用 xmake 完成 Windows x64 Release 构建。
