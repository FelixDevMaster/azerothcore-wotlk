# mod-warden-ni

Detects the [ni-v3](https://github.com/darhanger/ni-v3) (Nevermore Internal) loader when a player
activates a profile (for example "che paladin"). The character may log in. When they toggle the
script on, the server announces it to GMs, waits 5 seconds, then kicks. **No account ban.**

## Why `_G.ni` is not enough

ni-v3 keeps the `ni` table as a local upvalue from the C++ injector (`local ni = ...`) and lists
Anti Warden as a feature. `_G.ni` is usually nil, so Lua checks like `return not not ni` never fire.

Activating a profile prints `Primary started` (or Secondary/Generic) from
`addon/Core/components/main_window/init.lua`. That print is what we watch for.

Warden payload strings are a **uint8 length (max 255 bytes)**. A longer payload is truncated and
silently does nothing.

## How it works

1. ~2s after login, Warden injects three short payloads (ids 9000–9002): hook `ChatFrame1.AddMessage`,
   scan chat history, and a best-effort `_G.ni` probe.
2. When the player enables the rotation, `print("Primary started")` is hooked and reported with
   prefix `_NI` (swallowed, never reaches the guild).
3. GMs and the worldserver log: *player X activated ni loader*.
4. After `NiWarden.KickDelaySeconds` (default 5) the session is kicked.

SQL checks 800–802 scan the chat frame for those same strings (kick, no ban). 797–799 still probe
the `ni` table in case a build leaks it.

Requires `Warden.Enabled = 1`, `AddonChannel = 1`, a Windows client, and a **rebuild** of
`mod-warden-ni`.

On worldserver startup you should see `Ni Warden: enabled`. After login:
`Queued ni watcher for <name>`. If those lines are missing, the module is not compiled in or
Warden is off.

## Config

See `conf/warden_ni.conf.dist`. Copy or merge into `worldserver.conf`.

| Option | Default | Meaning |
| --- | --- | --- |
| `NiWarden.Enable` | 1 | Master switch |
| `NiWarden.KickDelaySeconds` | 5 | Delay before kick (`0` = immediate) |
| `NiWarden.AnnounceGMs` | 1 | System message to online GMs |
| `NiWarden.NotifyPlayer` | 1 | Warn the player before the kick |
| `NiWarden.RequeueSeconds` | 60 | Re-inject the watcher (covers `/reload ui`) |

## SQL

Apply pending world + characters updates (or let the updater run them), then restart worldserver.
