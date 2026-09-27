/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include "MythicPlusMgr.h"
#include "Creature.h"
#include "Map.h"
#include "SharedDefines.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "TemporarySummon.h"
#include "ThreatManager.h"
#include "Unit.h"

char const* MythicPlusMgr::AffixName(uint8 affix, bool spanish)
{
    switch (affix)
    {
        case AFFIX_FORTIFIED:  return spanish ? "Fortificado" : "Fortified";
        case AFFIX_TYRANNICAL: return spanish ? "Tiranico" : "Tyrannical";
        case AFFIX_BOLSTERING: return spanish ? "Potenciador" : "Bolstering";
        case AFFIX_BURSTING:   return spanish ? "Estallido" : "Bursting";
        case AFFIX_RAGING:     return spanish ? "Enfurecido" : "Raging";
        case AFFIX_SANGUINE:   return spanish ? "Sanguino" : "Sanguine";
        case AFFIX_TEEMING:    return spanish ? "Bullente" : "Teeming";
        case AFFIX_NECROTIC:   return spanish ? "Necrotico" : "Necrotic";
        case AFFIX_SKITTISH:   return spanish ? "Asustadizo" : "Skittish";
        case AFFIX_VOLCANIC:   return spanish ? "Volcanico" : "Volcanic";
        case AFFIX_EXPLOSIVE:  return spanish ? "Explosivo" : "Explosive";
        case AFFIX_QUAKING:    return spanish ? "Sismico" : "Quaking";
        case AFFIX_GRIEVOUS:   return spanish ? "Doloroso" : "Grievous";
        case AFFIX_INSPIRING:  return spanish ? "Inspirador" : "Inspiring";
        case AFFIX_SPITEFUL:   return spanish ? "Malicioso" : "Spiteful";
        case AFFIX_STORMING:   return spanish ? "Tormentoso" : "Storming";
        case AFFIX_INFESTED:   return spanish ? "Infestado" : "Infested";
        case AFFIX_REAPING:    return spanish ? "Siega" : "Reaping";
        case AFFIX_BEGUILING:  return spanish ? "Seduccion" : "Beguiling";
        case AFFIX_AWAKENED:   return spanish ? "Despertado" : "Awakened";
        default:               return spanish ? "Ninguno" : "None";
    }
}

char const* MythicPlusMgr::AffixDesc(uint8 affix, bool spanish)
{
    switch (affix)
    {
        case AFFIX_FORTIFIED:
            return spanish ? "Los no-jefes tienen un 20% mas de vida y un 30% mas de dano."
                           : "Non-bosses have 20% more health and deal 30% more damage.";
        case AFFIX_TYRANNICAL:
            return spanish ? "Los jefes tienen un 40% mas de vida y un 15% mas de dano."
                           : "Bosses have 40% more health and deal 15% more damage.";
        case AFFIX_BOLSTERING:
            return spanish ? "Al morir, los no-jefes potencian a sus aliados un 20% de vida."
                           : "When slain, non-bosses bolster nearby allies for 20% health.";
        case AFFIX_BURSTING:
            return spanish ? "Al morir, los no-jefes explotan e infligen dano a todo el grupo."
                           : "When slain, non-bosses burst and damage the whole party.";
        case AFFIX_RAGING:
            return spanish ? "Los no-jefes se enfurecen al 30% de vida (+75% dano)."
                           : "Non-bosses enrage at 30% health (+75% damage).";
        case AFFIX_SANGUINE:
            return spanish ? "Los no-jefes dejan un charco que cura enemigos y dana jugadores."
                           : "Non-bosses leave a pool that heals enemies and hurts players.";
        case AFFIX_TEEMING:
            return spanish ? "Hay enemigos no-jefe adicionales."
                           : "Additional non-boss enemies are present.";
        case AFFIX_NECROTIC:
            return spanish ? "Los golpes cuerpo a cuerpo aplican un apilamiento que reduce la sanacion."
                           : "Melee hits apply a stacking blight that reduces healing.";
        case AFFIX_SKITTISH:
            return spanish ? "Los enemigos prestan mucha menos atencion a la amenaza."
                           : "Enemies pay far less attention to threat.";
        case AFFIX_VOLCANIC:
            return spanish ? "Los jugadores alejados sufren erupciones periodicas."
                           : "Distant players periodically erupt volcanic plumes.";
        case AFFIX_EXPLOSIVE:
            return spanish ? "Aparecen orbes explosivos que deben destruirse."
                           : "Explosive orbs spawn and must be destroyed.";
        case AFFIX_QUAKING:
            return spanish ? "Todos emiten una onda que dana e interrumpe a aliados cercanos."
                           : "Players emit a shockwave that damages and interrupts nearby allies.";
        case AFFIX_GRIEVOUS:
            return spanish ? "Por debajo del 90% de vida sufres un sangrado creciente."
                           : "Below 90% health you suffer a stacking grievous wound.";
        case AFFIX_INSPIRING:
            return spanish ? "Algunos enemigos inspiran a sus aliados e impiden el CC."
                           : "Some enemies inspire allies and prevent crowd control.";
        case AFFIX_SPITEFUL:
            return spanish ? "Al morir, los no-jefes envian un espectro que te fija."
                           : "When slain, non-bosses send a spiteful shade that fixates a player.";
        case AFFIX_STORMING:
            return spanish ? "Aparecen tempestades que persiguen a los jugadores."
                           : "Swirling tempests chase the party.";
        case AFFIX_INFESTED:
            return spanish ? "Algunos enemigos estan infestados y al morir sueltan dos engendros."
                           : "Some enemies are infested and spawn two oozes on death.";
        case AFFIX_REAPING:
            return spanish ? "Cada 20 muertes de no-jefes aparecen espectros de Bwonsamdi."
                           : "Every 20 non-boss deaths, Bwonsamdi's revenants appear.";
        case AFFIX_BEGUILING:
            return spanish ? "Hay emisarias encantadas extra en la mazmorra."
                           : "Enchanted emissaries appear throughout the dungeon.";
        case AFFIX_AWAKENED:
            return spanish ? "Manifestaciones de N'Zoth vigilan la mazmorra."
                           : "N'Zoth manifestations watch the dungeon.";
        default:
            return "";
    }
}

uint64 MythicPlusMgr::CrowdControlMask()
{
    return (1ULL << MECHANIC_STUN) | (1ULL << MECHANIC_ROOT) | (1ULL << MECHANIC_FEAR)
        | (1ULL << MECHANIC_SILENCE) | (1ULL << MECHANIC_DISORIENTED) | (1ULL << MECHANIC_FREEZE)
        | (1ULL << MECHANIC_KNOCKOUT) | (1ULL << MECHANIC_POLYMORPH) | (1ULL << MECHANIC_HORROR)
        | (1ULL << MECHANIC_SLEEP) | (1ULL << MECHANIC_SAPPED);
}

void MythicPlusMgr::ModifyCreatureDamage(Unit* attacker, Unit* /*victim*/, uint32& damage)
{
    if (!attacker || !attacker->IsCreature())
        return;
    Map* map = attacker->GetMap();
    if (!map)
        return;
    MythicRun const* run = GetRun(map->GetInstanceId());
    if (!run || !run->Active)
        return;
    damage = uint32(float(damage) * DamageMultiplier(*run, attacker->ToCreature()));
}

void MythicPlusMgr::ModifyHealReceived(Unit* target, uint32& heal)
{
    if (!target || !target->IsPlayer())
        return;
    MythicRun const* run = GetRunForPlayer(target->ToPlayer());
    if (!run || !run->Active || !run->HasAffix(AFFIX_NECROTIC))
        return;

    auto it = run->Necrotic.find(target->GetGUID());
    if (it == run->Necrotic.end() || !it->second.Stacks)
        return;
    float reduce = std::min(0.90f, it->second.Stacks * 0.04f);
    heal = uint32(float(heal) * (1.f - reduce));
}

void MythicPlusMgr::OnCreatureMeleeHit(Unit* attacker, Unit* victim)
{
    if (!attacker || !attacker->IsCreature() || !victim || !victim->IsPlayer())
        return;
    MythicRun* run = GetRunForPlayer(victim->ToPlayer());
    if (!run || !run->Active)
        return;

    if (run->HasAffix(AFFIX_NECROTIC))
    {
        MythicNecroticState& state = run->Necrotic[victim->GetGUID()];
        if (state.Stacks < 20)
            ++state.Stacks;
        state.LastHitMs = run->ElapsedMs;
    }

    if (run->HasAffix(AFFIX_SKITTISH))
    {
        if (Player* player = victim->ToPlayer())
            if (roll_chance_i(40))
                attacker->GetThreatMgr().ModifyThreatByPercent(player, -80);
    }
}

void MythicPlusMgr::ApplyAffixDeath(Map* map, MythicRun& run, Creature* creature)
{
    if (!map || !creature)
        return;
    Player* source = FirstOnlineMember(run);

    if (run.HasAffix(AFFIX_BOLSTERING))
    {
        for (auto const& pair : map->GetCreatureBySpawnIdStore())
        {
            Creature* ally = pair.second;
            if (!ally || !ally->IsAlive() || ally == creature || IsBoss(ally))
                continue;
            if (ally->GetExactDist(creature) > 30.f)
                continue;
            uint8& stacks = run.BolsterStacks[ally->GetGUID()];
            if (stacks >= 10)
                continue;
            ++stacks;
            uint32 bonus = std::max<uint32>(1, ally->GetMaxHealth() / 5);
            ally->SetCreateHealth(ally->GetMaxHealth() + bonus);
            ally->SetMaxHealth(ally->GetMaxHealth() + bonus);
            ally->SetHealth(ally->GetHealth() + bonus);
        }
    }

    if (run.HasAffix(AFFIX_BURSTING))
    {
        map->DoForAllPlayers([&run](Player* player)
        {
            MythicBurstingState& state = run.Bursting[player->GetGUID()];
            if (state.Stacks < 8)
                ++state.Stacks;
            state.ExpireMs = 4000;
        });
    }

    if (run.HasAffix(AFFIX_SANGUINE))
    {
        MythicSanguinePool pool;
        pool.Pos = creature->GetPosition();
        pool.ExpireMs = 20000;
        run.Pools.push_back(pool);
        if (source)
            source->SummonCreature(NPC_MYTHIC_SANGUINE, creature->GetPosition(),
                TEMPSUMMON_TIMED_DESPAWN, 20000);
    }

    if (run.HasAffix(AFFIX_SPITEFUL) && source)
    {
        if (TempSummon* shade = source->SummonCreature(NPC_MYTHIC_SPITEFUL, *creature,
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 20000))
        {
            ScaleCreature(shade, run);
            if (Player* target = ObjectAccessor::FindConnectedPlayer(
                    run.Members[urand(0, run.Members.size() - 1)]))
            {
                shade->SetInCombatWith(target);
                shade->AddThreat(target, 1000000.f);
                shade->GetMotionMaster()->MoveChase(target);
            }
        }
    }

    if (run.HasAffix(AFFIX_INFESTED) && run.Infested.count(creature->GetGUID()) && source)
    {
        for (uint8 i = 0; i < 2; ++i)
        {
            if (TempSummon* spawn = source->SummonCreature(NPC_MYTHIC_INFESTED, *creature,
                    TEMPSUMMON_CORPSE_TIMED_DESPAWN, 15000))
            {
                ScaleCreature(spawn, run);
                if (Player* target = ObjectAccessor::FindConnectedPlayer(
                        run.Members[urand(0, run.Members.size() - 1)]))
                {
                    spawn->SetInCombatWith(target);
                    spawn->AddThreat(target, 100000.f);
                }
            }
        }
    }

    if (run.HasAffix(AFFIX_REAPING) && run.ReapingKills >= 20 && source)
    {
        run.ReapingKills = 0;
        Announce(map,
            Acore::StringFormat("|cff9900ff{}|r", AffixName(AFFIX_REAPING, false)),
            Acore::StringFormat("|cff9900ff{}|r", AffixName(AFFIX_REAPING, true)));
        for (uint8 i = 0; i < 3; ++i)
        {
            if (TempSummon* revenant = source->SummonCreature(NPC_MYTHIC_REAPING, *source,
                    TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30000))
                ScaleCreature(revenant, run);
        }
    }
}

void MythicPlusMgr::TickAffixes(Map* map, MythicRun& run, uint32 diff)
{
    if (!map)
        return;
    Player* source = FirstOnlineMember(run);
    run.AffixDotMs += diff;
    bool const dotTick = run.AffixDotMs >= 1000;
    if (dotTick)
        run.AffixDotMs -= 1000;

    for (auto const& pair : map->GetCreatureBySpawnIdStore())
    {
        Creature* creature = pair.second;
        if (!creature || !creature->IsAlive() || IsBoss(creature))
            continue;
        if (run.HasAffix(AFFIX_RAGING) && creature->GetHealthPct() <= 30.f)
            run.Raging.insert(creature->GetGUID());
    }

    if (run.HasAffix(AFFIX_BURSTING))
    {
        for (auto& pair : run.Bursting)
        {
            if (!pair.second.Stacks || !pair.second.ExpireMs)
                continue;
            if (pair.second.ExpireMs <= diff)
            {
                pair.second.Stacks = 0;
                pair.second.ExpireMs = 0;
                continue;
            }
            pair.second.ExpireMs -= diff;
            if (!dotTick)
                continue;
            if (Player* player = ObjectAccessor::FindConnectedPlayer(pair.first))
            {
                uint32 tick = std::max<uint32>(1, player->CountPctFromMaxHealth(2 * pair.second.Stacks));
                Unit::DealDamage(player, player, tick, nullptr, NODAMAGE);
            }
        }
    }

    if (run.HasAffix(AFFIX_SANGUINE))
    {
        for (auto it = run.Pools.begin(); it != run.Pools.end();)
        {
            if (it->ExpireMs <= diff)
            {
                it = run.Pools.erase(it);
                continue;
            }
            it->ExpireMs -= diff;
            if (dotTick)
            {
                map->DoForAllPlayers([&](Player* player)
                {
                    if (player->GetExactDist(&it->Pos) <= 5.f)
                        Unit::DealDamage(player, player, player->CountPctFromMaxHealth(3), nullptr, NODAMAGE);
                });
            }
            for (auto const& pair : map->GetCreatureBySpawnIdStore())
            {
                Creature* creature = pair.second;
                if (creature && creature->IsAlive() && creature->GetExactDist(&it->Pos) <= 5.f)
                    creature->ModifyHealth(int32(creature->CountPctFromMaxHealth(5)));
            }
            ++it;
        }
    }

    if (run.HasAffix(AFFIX_NECROTIC))
    {
        for (auto& pair : run.Necrotic)
        {
            if (pair.second.Stacks && run.ElapsedMs > pair.second.LastHitMs + 4000)
                pair.second.Stacks = pair.second.Stacks > 1 ? pair.second.Stacks - 1 : 0;
            if (dotTick && pair.second.Stacks)
            {
                if (Player* player = ObjectAccessor::FindConnectedPlayer(pair.first))
                    Unit::DealDamage(player, player,
                        player->CountPctFromMaxHealth(pair.second.Stacks), nullptr, NODAMAGE);
            }
        }
    }

    if (run.HasAffix(AFFIX_GRIEVOUS) && dotTick)
    {
        map->DoForAllPlayers([&run](Player* player)
        {
            uint8& stacks = run.Grievous[player->GetGUID()];
            if (player->GetHealthPct() >= 90.f)
            {
                stacks = 0;
                return;
            }
            if (stacks < 10)
                ++stacks;
            Unit::DealDamage(player, player, player->CountPctFromMaxHealth(2 * stacks), nullptr, NODAMAGE);
        });
    }

    if (run.HasAffix(AFFIX_QUAKING))
    {
        if (run.QuakingMs <= diff)
        {
            run.QuakingMs = 20000;
            Announce(map,
                Acore::StringFormat("|cffff9900{}|r", AffixName(AFFIX_QUAKING, false)),
                Acore::StringFormat("|cffff9900{}|r", AffixName(AFFIX_QUAKING, true)));
            std::vector<Player*> players;
            map->DoForAllPlayers([&players](Player* player) { players.push_back(player); });
            for (Player* a : players)
            {
                bool nearAlly = false;
                for (Player* b : players)
                {
                    if (a == b)
                        continue;
                    if (a->GetExactDist(b) <= 8.f)
                    {
                        nearAlly = true;
                        Unit::DealDamage(a, b, b->CountPctFromMaxHealth(8), nullptr, NODAMAGE);
                    }
                }
                if (nearAlly)
                    a->InterruptNonMeleeSpells(false);
            }
        }
        else
            run.QuakingMs -= diff;
    }

    if (run.HasAffix(AFFIX_VOLCANIC) && source)
    {
        if (run.VolcanicMs <= diff)
        {
            run.VolcanicMs = 10000;
            map->DoForAllPlayers([&](Player* player)
            {
                bool nearEnemy = false;
                for (auto const& pair : map->GetCreatureBySpawnIdStore())
                {
                    Creature* creature = pair.second;
                    if (creature && creature->IsAlive() && !creature->IsTrigger()
                        && player->GetExactDist(creature) <= 15.f)
                    {
                        nearEnemy = true;
                        break;
                    }
                }
                if (nearEnemy)
                    return;
                Unit::DealDamage(player, player, player->CountPctFromMaxHealth(6), nullptr, NODAMAGE);
                source->SummonCreature(NPC_MYTHIC_VOLCANIC, *player, TEMPSUMMON_TIMED_DESPAWN, 3000);
            });
        }
        else
            run.VolcanicMs -= diff;
    }

    if (run.HasAffix(AFFIX_EXPLOSIVE) && source)
    {
        if (run.ExplosiveMs <= diff)
        {
            run.ExplosiveMs = 8000;
            Creature* host = nullptr;
            for (auto const& pair : map->GetCreatureBySpawnIdStore())
            {
                Creature* creature = pair.second;
                if (creature && creature->IsAlive() && creature->IsInCombat() && !IsBoss(creature))
                {
                    host = creature;
                    break;
                }
            }
            if (host)
            {
                if (TempSummon* orb = source->SummonCreature(NPC_MYTHIC_EXPLOSIVE, *host,
                        TEMPSUMMON_TIMED_DESPAWN, 7000))
                {
                    ScaleCreature(orb, run);
                    orb->SetReactState(REACT_PASSIVE);
                    MythicTimedGuid timed;
                    timed.Guid = orb->GetGUID();
                    timed.ExpireMs = 6000;
                    run.Explosives.push_back(timed);
                }
            }
        }
        else
            run.ExplosiveMs -= diff;

        for (auto it = run.Explosives.begin(); it != run.Explosives.end();)
        {
            Creature* orb = map->GetCreature(it->Guid);
            if (!orb || !orb->IsAlive())
            {
                it = run.Explosives.erase(it);
                continue;
            }
            if (it->ExpireMs <= diff)
            {
                map->DoForAllPlayers([&](Player* player)
                {
                    if (player->GetExactDist(orb) <= 40.f)
                        Unit::DealDamage(orb, player, player->CountPctFromMaxHealth(15), nullptr, DIRECT_DAMAGE);
                });
                orb->DespawnOrUnsummon();
                it = run.Explosives.erase(it);
            }
            else
            {
                it->ExpireMs -= diff;
                ++it;
            }
        }
    }

    if (run.HasAffix(AFFIX_STORMING) && source)
    {
        if (run.StormingMs <= diff)
        {
            run.StormingMs = 15000;
            if (TempSummon* storm = source->SummonCreature(NPC_MYTHIC_STORMING, *source,
                    TEMPSUMMON_TIMED_DESPAWN, 8000))
            {
                storm->SetReactState(REACT_PASSIVE);
                MythicTimedGuid timed;
                timed.Guid = storm->GetGUID();
                timed.ExpireMs = 8000;
                run.Storms.push_back(timed);
            }
        }
        else
            run.StormingMs -= diff;

        for (auto it = run.Storms.begin(); it != run.Storms.end();)
        {
            Creature* storm = map->GetCreature(it->Guid);
            if (!storm || !storm->IsAlive() || it->ExpireMs <= diff)
            {
                if (storm)
                    storm->DespawnOrUnsummon();
                it = run.Storms.erase(it);
                continue;
            }
            it->ExpireMs -= diff;
            if (Player* target = ObjectAccessor::FindConnectedPlayer(
                    run.Members[urand(0, run.Members.size() - 1)]))
                storm->GetMotionMaster()->MoveFollow(target, 0.f, 0.f);
            map->DoForAllPlayers([&](Player* player)
            {
                if (dotTick && player->GetExactDist(storm) <= 4.f)
                    Unit::DealDamage(player, player, player->CountPctFromMaxHealth(4), nullptr, NODAMAGE);
            });
            ++it;
        }
    }

    if (run.HasAffix(AFFIX_INSPIRING))
    {
        for (ObjectGuid const& guid : run.Inspiring)
        {
            Creature* inspirer = map->GetCreature(guid);
            if (!inspirer || !inspirer->IsAlive())
                continue;
            for (auto const& pair : map->GetCreatureBySpawnIdStore())
            {
                Creature* ally = pair.second;
                if (ally && ally->IsAlive() && ally->GetExactDist(inspirer) <= 15.f)
                    ally->RemoveAurasWithMechanic(CrowdControlMask());
            }
        }
    }
}
