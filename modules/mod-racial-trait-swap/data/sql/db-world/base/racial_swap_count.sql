-- --------------------------------------------------------
-- Table structure for tracking racial trait swap usage per account
-- --------------------------------------------------------

DROP TABLE IF EXISTS `mod_racial_swap_count`;
CREATE TABLE IF NOT EXISTS `mod_racial_swap_count` (
	`AccountID` INT(10) UNSIGNED NOT NULL DEFAULT '0' COMMENT 'Account that has used the racial trait swap NPC',
	`SwapCount` INT(10) UNSIGNED NOT NULL DEFAULT '0' COMMENT 'Number of times the swap has been performed on this account',
	PRIMARY KEY (`AccountID`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
