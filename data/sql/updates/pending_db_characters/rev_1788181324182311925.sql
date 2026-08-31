-- Kick (1) without ban (2) for the che paladin chat-frame check.

DELETE FROM `warden_action` WHERE `wardenId` = 803;
INSERT INTO `warden_action` (`wardenId`, `action`) VALUES
(803, 1);
