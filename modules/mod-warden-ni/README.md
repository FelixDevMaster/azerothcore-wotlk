# mod-warden-ni

Detects the [ni-v3](https://github.com/darhanger/ni-v3) (Nevermore Internal) loader when a player
activates it inside `wow.exe`. The character is allowed to connect. When the script comes up the
server announces it to GMs, waits 5 seconds, then kicks the player. **No account ban.**

## How it works

1. On login, Warden injects a small Lua watcher (custom payload id 9000).
2. The watcher looks for the `ni` global (`loaded_init` / `backend` from v3, or `rotation` / `vars`
   from older ni) on each frame — so detection happens at activation, not on the next Warden cycle.
3. The client reports over a private addon prefix (`_NI`). That message is swallowed and never
   reaches the guild.
4. GMs and the worldserver log receive: *player X activated ni loader*.
5. After `NiWarden.KickDelaySeconds` (default 5) the session is kicked.

SQL Lua checks 797–799 are a slower fallback if the payload is blocked: they log and kick (still
no ban) on the normal Warden Lua rotation.

Requires `Warden.Enabled = 1` and a Windows client.

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

Apply pending updates (or restart worldserver with updater enabled):

- `data/sql/updates/pending_db_world/` — Lua checks for ni v3 / classic ni
- `data/sql/updates/pending_db_characters/` — those checks use action **kick** (not ban)
