-- Chat-frame fallback: ni-v3 prints these strings when a profile is toggled on.
-- Does not depend on the hidden `_G.ni` table.

DELETE FROM `warden_checks` WHERE `id` IN (800, 801, 802);
INSERT INTO `warden_checks` (`id`, `type`, `data`, `str`, `address`, `length`, `result`, `comment`) VALUES
(800, 139, NULL, 'local f=DEFAULT_CHAT_FRAME for i=1,f:GetNumMessages() do if (f:GetMessageInfo(i)):find(\"Primary started\") then return true end end', NULL, NULL, NULL, 'Detects ni loader (Primary started)'),
(801, 139, NULL, 'local f=DEFAULT_CHAT_FRAME for i=1,f:GetNumMessages() do if (f:GetMessageInfo(i)):find(\"Secondary started\") then return true end end', NULL, NULL, NULL, 'Detects ni loader (Secondary started)'),
(802, 139, NULL, 'local f=DEFAULT_CHAT_FRAME for i=1,f:GetNumMessages() do if (f:GetMessageInfo(i)):find(\"Generic started\") then return true end end', NULL, NULL, NULL, 'Detects ni loader (Generic started)');
