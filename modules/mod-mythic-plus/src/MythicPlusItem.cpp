/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MythicPlusMgr.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Player.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "WorldPacket.h"
#include "WorldSession.h"

std::string MythicPlusMgr::KeystoneItemName(Player const* player)
{
    if (!player)
        return "Mythic Keystone";

    bool const es = IsSpanish(player);
    MythicProfile profile = GetProfile(player->GetGUID());
    if (!profile.Key.Level)
        return es ? "Piedra angular mitica" : "Mythic Keystone";

    return Acore::StringFormat(es ? "Piedra angular mitica: {} (+{})" : "Mythic Keystone: {} (+{})",
        DungeonName(profile.Key.DungeonId, es), profile.Key.Level);
}

std::string MythicPlusMgr::KeystoneItemDescription(Player const* player)
{
    if (!player)
        return "";

    bool const es = IsSpanish(player);
    MythicProfile profile = GetProfile(player->GetGUID());
    if (!profile.Key.Level)
    {
        return es ? "Inserta esta piedra en la Fuente de Poder a la entrada de la mazmorra."
                  : "Insert this keystone into the Font of Power at the dungeon entrance.";
    }

    MythicAffixSet affixes = GetAffixesForLevel(profile.Key.Level);
    std::string affixLine;
    uint8 const ids[4] = { affixes.FortTyr, affixes.Plus4, affixes.Plus7, affixes.Seasonal };
    for (uint8 id : ids)
    {
        if (!id)
            continue;
        if (!affixLine.empty())
            affixLine += ", ";
        affixLine += AffixName(id, es);
    }

    return Acore::StringFormat(
        es ? "Nivel {}{}. Inserta la piedra en la Fuente de Poder. Affijos: {}."
           : "Level {}{}. Insert the keystone at the Font of Power. Affixes: {}.",
        profile.Key.Level,
        profile.Key.Depleted ? (es ? " (agotada)" : " (depleted)") : "",
        affixLine.empty() ? (es ? "ninguno" : "none") : affixLine);
}

void MythicPlusMgr::BuildItemQueryPacket(WorldPacket& data, ItemTemplate const* proto,
    std::string const& name, std::string const& description) const
{
    data << proto->ItemId;
    data << proto->Class;
    data << proto->SubClass;
    data << proto->SoundOverrideSubclass;
    data << name;
    data << uint8(0x00);
    data << uint8(0x00);
    data << uint8(0x00);
    uint32 displayId = proto->DisplayInfoID;
    if (proto->ItemId == ITEM_MYTHIC_KEYSTONE)
        displayId = DISPLAY_MYTHIC_KEYSTONE;
    else if (proto->ItemId == ITEM_MYTHIC_RESIDUUM)
        displayId = DISPLAY_MYTHIC_RESIDUUM;
    data << displayId;
    data << proto->Quality;
    data << proto->Flags;
    data << proto->Flags2;
    data << proto->BuyPrice;
    data << proto->SellPrice;
    data << proto->InventoryType;
    data << proto->AllowableClass;
    data << proto->AllowableRace;
    data << proto->ItemLevel;
    data << proto->RequiredLevel;
    data << proto->RequiredSkill;
    data << proto->RequiredSkillRank;
    data << proto->RequiredSpell;
    data << proto->RequiredHonorRank;
    data << proto->RequiredCityRank;
    data << proto->RequiredReputationFaction;
    data << proto->RequiredReputationRank;
    data << int32(proto->MaxCount);
    data << int32(proto->Stackable);
    data << proto->ContainerSlots;
    data << proto->StatsCount;
    for (uint32 i = 0; i < proto->StatsCount; ++i)
    {
        data << proto->ItemStat[i].ItemStatType;
        data << proto->ItemStat[i].ItemStatValue;
    }
    data << proto->ScalingStatDistribution;
    data << proto->ScalingStatValue;
    for (int i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        data << proto->Damage[i].DamageMin;
        data << proto->Damage[i].DamageMax;
        data << proto->Damage[i].DamageType;
    }
    data << proto->Armor;
    data << proto->HolyRes;
    data << proto->FireRes;
    data << proto->NatureRes;
    data << proto->FrostRes;
    data << proto->ShadowRes;
    data << proto->ArcaneRes;
    data << proto->Delay;
    data << proto->AmmoType;
    data << proto->RangedModRange;
    for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
    {
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(proto->Spells[s].SpellId);
        if (spell)
        {
            bool dbData = proto->Spells[s].SpellCooldown >= 0 || proto->Spells[s].SpellCategoryCooldown >= 0;
            data << proto->Spells[s].SpellId;
            data << proto->Spells[s].SpellTrigger;
            data << int32(proto->Spells[s].SpellCharges);
            if (dbData)
            {
                data << uint32(proto->Spells[s].SpellCooldown);
                data << uint32(proto->Spells[s].SpellCategory);
                data << uint32(proto->Spells[s].SpellCategoryCooldown);
            }
            else
            {
                data << uint32(spell->RecoveryTime);
                data << uint32(spell->GetCategory());
                data << uint32(spell->CategoryRecoveryTime);
            }
        }
        else
        {
            data << uint32(0);
            data << uint32(0);
            data << uint32(0);
            data << uint32(-1);
            data << uint32(0);
            data << uint32(-1);
        }
    }
    data << proto->Bonding;
    data << description;
    data << proto->PageText;
    data << proto->LanguageID;
    data << proto->PageMaterial;
    data << proto->StartQuest;
    data << proto->LockID;
    data << int32(proto->Material);
    data << proto->Sheath;
    data << proto->RandomProperty;
    data << proto->RandomSuffix;
    data << proto->Block;
    data << proto->ItemSet;
    data << proto->MaxDurability;
    data << proto->Area;
    data << proto->Map;
    data << proto->BagFamily;
    data << proto->TotemCategory;
    for (int s = 0; s < MAX_ITEM_PROTO_SOCKETS; ++s)
    {
        data << proto->Socket[s].Color;
        data << proto->Socket[s].Content;
    }
    data << proto->socketBonus;
    data << proto->GemProperties;
    data << proto->RequiredDisenchantSkill;
    data << proto->ArmorDamageModifier;
    data << proto->Duration;
    data << proto->ItemLimitCategory;
    data << proto->HolidayId;
}

void MythicPlusMgr::SendCustomItemQuery(Player* player, uint32 itemId)
{
    if (!player || !player->GetSession())
        return;

    ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
    if (!proto)
        return;

    bool const es = IsSpanish(player);
    std::string name;
    std::string description;
    if (itemId == _keystoneItem)
    {
        name = KeystoneItemName(player);
        description = KeystoneItemDescription(player);
    }
    else if (itemId == _residuumItem)
    {
        name = es ? "Ecos de Dominio" : "Echoes of Domination";
        description = es
            ? "Residuo titanico de las mazmorras miticas. Se usa con el Corredor de Piedras."
            : "Titan residue gathered from Mythic Keystone dungeons. Spend it at the Keystone Broker.";
    }
    else
        return;

    WorldPacket data(SMSG_ITEM_QUERY_SINGLE_RESPONSE, 600);
    BuildItemQueryPacket(data, proto, name, description);
    player->GetSession()->SendPacket(&data);
}

bool MythicPlusMgr::TryConsumeItemQuery(WorldSession* session, WorldPacket const& packet)
{
    if (!session)
        return false;
    Player* player = session->GetPlayer();
    if (!player)
        return false;

    uint16 const opcode = packet.GetOpcode();
    if (opcode != CMSG_ITEM_QUERY_SINGLE)
        return false;

    WorldPacket data(packet);
    uint32 itemId = 0;
    data >> itemId;
    if (itemId != _keystoneItem && itemId != _residuumItem)
        return false;

    SendCustomItemQuery(player, itemId);
    return true;
}

void MythicPlusMgr::RefreshKeyItem(Player* player)
{
    if (!player)
        return;

    LoadProfile(player->GetGUID());
    if (!_profiles[player->GetGUID()].Key.Level)
    {
        RemoveKeyItem(player);
        return;
    }

    if (player->HasItemCount(_keystoneItem, 1, true))
        RemoveKeyItem(player);

    // Push the new name into the client cache before the item lands in bags.
    SendCustomItemQuery(player, _keystoneItem);
    GiveItem(player, _keystoneItem, 1);
}
