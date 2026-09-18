# Arena / Battleground QoL

Modulo de calidad de vida para arenas y campos de batalla:

1. **Ritual automatico.** Si hay un mago, se coloca [Ritual de Refrigerio] en su posicion. Si hay un brujo, se coloca [Ritual de Almas] en la suya. El rango (1 o 2) depende del hechizo que tenga aprendido el jugador. Esto aplica en arenas y campos de batalla.
2. **Ready check de arenas.** Solo en arenas. Cuando quedan 30 segundos de espera (configurable), todos reciben un cartel para confirmar si estan listos. Si todos aceptan, la espera pasa a 15 segundos. En BG no hay confirmacion de listos, solo los rituales.

No modifica archivos del core: solo usa hooks de scripts.

---

Arena and battleground QoL for AzerothCore 3.3.5a:

| Feature | Where | What |
| --- | --- | --- |
| Ritual of Refreshment | Arena + BG | Spawned at each mage's position on join |
| Ritual of Souls | Arena + BG | Spawned at each warlock's position on join |
| Ready check skip | Arena prep only | Cartel at 30s remaining; if everyone accepts, wait jumps to 15s. Never shown in battlegrounds. |

## Config

Copy `conf/arena_bg_qol.conf.dist` into the worldserver config directory (CMake does this on install).

```
ArenaBgQol.Enable = 1
ArenaBgQol.Rituals.Enable = 1
ArenaBgQol.Rituals.Arenas = 1
ArenaBgQol.Rituals.Battlegrounds = 1
ArenaBgQol.Rituals.Duration = 180
ArenaBgQol.ReadyCheck.Enable = 1
ArenaBgQol.ReadyCheck.TriggerTime = 30
ArenaBgQol.ReadyCheck.SkipToTime = 15
ArenaBgQol.ReadyCheck.Timeout = 0
```

## Install

Place this folder in `modules/mod-arena-bg-qol` and rebuild worldserver.

## Notes

- Rank 2 tables / soulwells are used when the player knows Ritual of Refreshment (58659) or Ritual of Souls (58887); otherwise rank 1.
- The soulwell is owned by the warlock so Improved Healthstone still applies.
- Ready check is arena-only. Battlegrounds only spawn rituals; they never show the ready confirmation.
- The Yes/No cartel is the native raid ready-check UI, using another player as initiator (the client hides the buttons if the initiator is yourself). A gossip window is also opened so 1v1 still has a clickable prompt.
- Answers come from `MSG_RAID_READY_CHECK` or the gossip options. If anyone clicks "not ready" or the timeout expires, the original countdown continues.
