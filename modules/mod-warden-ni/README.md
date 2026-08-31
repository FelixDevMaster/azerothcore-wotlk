# mod-warden-ni

Detects [ni-v3](https://github.com/darhanger/ni-v3) when a player activates a rotation such as
**che paladin**. The character may log in. On activation the **entire realm** is told, then the
player is kicked. **No account ban.**

The GitHub repo `FelixDevMaster/che-paladin` was not reachable (404). Detection uses the ni-v3
loader contract plus the profile name `che paladin`:

- Toggling the primary profile prints `Primary started`
  (`addon/Core/components/main_window/init.lua`).
- The rotation folder/name `che paladin` is scanned in chat.

`_G.ni` is usually hidden (local upvalue + Anti Warden). Warden payloads must be **≤ 255 bytes**.

## How it works

1. ~2s after login, Warden injects payloads 9000–9002 (ChatFrame hook, history scan, `_G.ni` probe).
2. Enabling che paladin reports `_NI` to the server (swallowed, not sent to the guild).
3. Everyone online sees a realm announcement (server message + chat):
   `[Warden] <name> ha activado el script che paladin (ni loader). Ha sido expulsado.`
4. The player is kicked immediately (`NiWarden.KickDelaySeconds = 0`). No ban.

SQL checks 800–803 cover the same chat strings if the payload is blocked.

## Config

| Option | Default | Meaning |
| --- | --- | --- |
| `NiWarden.Enable` | 1 | Master switch |
| `NiWarden.KickDelaySeconds` | 0 | 0 = kick at once |
| `NiWarden.AnnounceWorld` | 1 | Announce to the whole realm |
| `NiWarden.AnnounceGMs` | 0 | Extra GM-only message |
| `NiWarden.NotifyPlayer` | 1 | Whisper/notify the cheater |
| `NiWarden.RequeueSeconds` | 60 | Re-inject after `/reload ui` |

Requires `Warden.Enabled = 1`, `AddonChannel = 1`, rebuild of this module, pending SQL, restart.
Startup log: `Ni Warden: enabled`. After login: `Queued ni watcher for <name>`.
