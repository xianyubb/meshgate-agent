# meshgate-agent

LeviLamina native command bridge for MeshGate.

This mod registers:

```text
/meshgate <target>
```

When a player runs the command, the mod reads the player's XUID and sends this request to MeshGate's local control API:

```http
POST /handoff
Authorization: Bearer <bearerToken>
Content-Type: application/json

{"xuid":"<player-xuid>","targetServer":"<target>"}
```

## Build

```powershell
xmake f -y -p windows -a x64 -m release
xmake -y
```

The packaged mod is generated at:

```text
bin/meshgate-agent/
```

## Install

Copy `bin/meshgate-agent` to the LeviLamina server's `plugins/meshgate-agent` directory.

On first load, the mod creates:

```text
plugins/meshgate-agent/config/config.json
```

Set `bearerToken` to the same value as MeshGate's `bearer_token`.

Example:

```json
{
  "apiHost": "127.0.0.1",
  "apiPort": 31920,
  "bearerToken": "replace-with-the-same-token-as-meshgate",
  "command": "meshgate"
}
```

`target` must match one of the `[[servers]].id` values in `meshgate.example.toml`, for example:

```text
/meshgate survival-02
```
