-- Detect the che paladin ni profile name in the chat frame.

DELETE FROM `warden_checks` WHERE `id` = 803;
INSERT INTO `warden_checks` (`id`, `type`, `data`, `str`, `address`, `length`, `result`, `comment`) VALUES
(803, 139, NULL, 'local f=DEFAULT_CHAT_FRAME if f then for i=1,f:GetNumMessages() do local t=f:GetMessageInfo(i) if t and t:find(\"che paladin\") then return true end end end', NULL, NULL, NULL, 'Detects che paladin ni profile');
