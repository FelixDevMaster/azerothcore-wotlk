-- Detect ni / Nevermore Internal loader (v3 and classic) via Warden Lua eval.
-- Action is overridden to kick (no ban) in pending_db_characters.

DELETE FROM `warden_checks` WHERE `id` IN (797, 798, 799);
INSERT INTO `warden_checks` (`id`, `type`, `data`, `str`, `address`, `length`, `result`, `comment`) VALUES
(797, 139, NULL, 'return not not (_G[string.char(110,105)] and _G[string.char(110,105)].loaded_init)', NULL, NULL, NULL, 'Detects ni loader (v3)'),
(798, 139, NULL, 'return not not (_G[string.char(110,105)] and _G[string.char(110,105)].backend)', NULL, NULL, NULL, 'Detects ni loader backend'),
(799, 139, NULL, 'return not not (_G[string.char(110,105)] and (_G[string.char(110,105)].rotation or _G[string.char(110,105)].vars))', NULL, NULL, NULL, 'Detects ni loader (classic)');
