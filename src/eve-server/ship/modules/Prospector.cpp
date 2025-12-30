
 /**
  * @name Prospector.cpp
  *   prospector module class (salvage, hacking, data mining)
  * @Author:         Allan
  * @date:   11 August 2016   -UD/RW 12 April 2017  -UD/RW 10 February 2018
  */


#include "StaticDataMgr.h"
#include "StatisticMgr.h"
#include "ship/modules/Prospector.h"
#include "system/Container.h"
#include "system/SystemManager.h"

/* this class is for all salvage and data mining types */

Prospector::Prospector(ModuleItemRef mRef, ShipItemRef sRef)
: ActiveModule(mRef, sRef),
pChar(nullptr),
m_success(false),
m_firstRun(true),
m_accessChance(0),
m_holdFlag(flagCargoHold),
m_salvager((m_modRef->groupID() == EVEDB::invGroups::Salvager)),
m_dataMiner((m_modRef->groupID() == EVEDB::invGroups::Data_Miner))
{
    if (m_shipRef->HasAttribute(AttrSalvageHoldCapacity))
        m_holdFlag = flagSalvageHold;

    if (!m_shipRef->HasPilot())
        return;

    pChar = m_shipRef->GetPilot()->GetChar().get();

    // increase scan speed by level of survey skill
    float cycleTime = GetAttribute(AttrDuration).get_float();
    cycleTime *= (1 + (0.03f * (pChar->GetSkillLevel(EvESkill::Survey, true))));
    SetAttribute(AttrDuration, cycleTime);
}

void Prospector::Update()
{
    if (!m_shipRef->HasPilot())
        return;

    pChar = m_shipRef->GetPilot()->GetChar().get();

    // increase scan speed by level of survey skill
    float cycleTime = GetAttribute(AttrDuration).get_float();
    cycleTime *= (1 + (0.03f * (pChar->GetSkillLevel(EvESkill::Survey, true))));
    SetAttribute(AttrDuration, cycleTime);

    ActiveModule::Update();
}

void Prospector::Activate(uint16 effectID, uint32 targetID, int16 repeat)
{
    // reset for each activation  MUST reset BEFORE ActiveModule::Activate() is called.....it calls salvage check.
    m_success = false;
    m_firstRun = true;

    ActiveModule::Activate(effectID, targetID, repeat);

    if (!m_needsTarget or (m_targetSE == nullptr)) {
        ActiveModule::Deactivate();
        return;
    }
    m_accessChance = m_targetSE->GetSelf()->GetAttribute(AttrAccessDifficulty).get_int();
    // are there any modifiers for access here?
    //  are rigs/skills added to module access chance?
}

bool Prospector::CanActivate()
{
    if (m_salvager)
        if (m_targetSE->IsWreckSE())
            return ActiveModule::CanActivate();
    if (m_dataMiner)
        if (m_targetSE->IsContainerSE())
            return ActiveModule::CanActivate();

    throw UserError ("DeniedActivateTargetModuleDisallowed");
}

uint32 Prospector::DoCycle()
{
    // First tick after activation just arms the module; actual salvaging
    // happens when the first cycle completes.
    if (m_firstRun) {
        m_firstRun = false;
        return ActiveModule::DoCycle();
    }

    // Resolve one salvage attempt per completed cycle.
    CheckSuccess();

    if (m_success) {
        // Successful salvage:
        //  - drop salvage/components
        //  - send success message
        //  - abort further cycling on this module, like TQ/Crucible.
        DropSalvage();
        AbortCycle();
        return 0;
    } else {
        // Salvage failed for this cycle; show the standard CCP message.
        SendFailure();
    }

    return ActiveModule::DoCycle();
}


/*
                  [PyTuple 3 items]
                    [PyString "OnRemoteMessage"]
                    [PyString "SalvagingFailure"]       (this is also a dungeon trigger)
                    [PyDict 1 kvp]
                      [PyString "type"]
                      [PyTuple 2 items]
                        [PyInt 4]           << cacheSolarSystemObjects???  cant find another reference for this.  always 4 so far.
                        [PyInt 26513]       << wreck type id

                    [PyTuple 2 items]       << this goes into effect.error
                      [PyString "SalvagingSuccess"]
                      [PyDict 1 kvp]
                        [PyString "type"]
                        [PyTuple 2 items]
                          [PyInt 4]         << cacheSolarSystemObjects???
                          [PyInt 26513]
                        */

void Prospector::SendFailure()
{
    // Salvager failure notification (CCP remote message)
    if (m_salvager) {
        if ((m_targetSE != nullptr) && m_shipRef && m_shipRef->HasPilot()) {
            PyTuple* type = new PyTuple(2);
                type->SetItem(0, new PyInt(4));                         // cacheSolarSystemObjects category
                type->SetItem(1, new PyInt(m_targetSE->GetTypeID()));
            PyDict* dict = new PyDict;
                dict->SetItemString("type", type);
            PyTuple* tup = new PyTuple(3);
                tup->SetItem(0, new PyString("OnRemoteMessage"));
                tup->SetItem(1, new PyString("SalvagingFailure"));
                tup->SetItem(2, dict);
            m_shipRef->GetPilot()->QueueDestinyEvent(&tup);
        } else {
            // Target vanished or no pilot; just skip the message rather than crashing.
        }
    }

    if (m_dataMiner) {
        // TODO: implement proper failure notification for data analyzers, as needed.
    }
}

void Prospector::SendSuccess(bool hollow)
{
    // Salvager success notification (CCP remote messages)
    if (!m_salvager)
        return;

    if ((m_targetSE == nullptr) || !m_shipRef || !m_shipRef->HasPilot())
        return;

    PyTuple* type = new PyTuple(2);
        type->SetItem(0, new PyInt(4));                         // cacheSolarSystemObjects category
        type->SetItem(1, new PyInt(m_targetSE->GetTypeID()));
    PyDict* dict = new PyDict;
        dict->SetItemString("type", type);
    PyTuple* tup = new PyTuple(3);
        tup->SetItem(0, new PyString("OnRemoteMessage"));
        tup->SetItem(1, new PyString(hollow ? "SalvagingSuccessHollow" : "SalvagingSuccess"));
        tup->SetItem(2, dict);
    m_shipRef->GetPilot()->QueueDestinyEvent(&tup);
}


void Prospector::CheckSuccess()
{
    // Safety: if we lost our target or ship, this cycle cannot succeed.
    if ((m_targetSE == nullptr) || !m_shipRef) {
        m_success = false;
        _log(MODULE__DEBUG,
             "Prospector::CheckSuccess - no target or ship, forced failure.");
        return;
    }

    // 1) Base difficulty comes from the wreck.
    // CCP data (Crucible era) was typically:
    //   Small T1:  30
    //   Medium T1: 20
    //   Large T1:  10
    //   Elite/T2/Sleeper: 0, -10, -20, etc.
    //
    // In EVEmu this should be in the wreck item as AttrAccessDifficulty.
    int baseDifficulty = 0;
    {
        InventoryItemRef selfRef = m_targetSE->GetSelf();
        if (selfRef.get() != nullptr) {
            baseDifficulty = selfRef->GetAttribute(AttrAccessDifficulty).get_int();
        }
    }

    // 2) Bonus comes from everything on the player **using a Salvager module**:
    //    - Salvager I / II
    //    - Salvaging skill
    //    - Salvage Tackle rigs
    //    - Prospector Salvaging implant
    //
    // Dogma aggregates all of that into AttrAccessDifficultyBonus for this module.
    int bonus = GetAttribute(AttrAccessDifficultyBonus).get_int();

    // 3) Final chance is base + bonus, treated as a percentage.
    int chance = baseDifficulty + bonus;

    // Clamp to 0?100% so bad data can't give negative or >100%.
    if (chance < 0)
        chance = 0;
    else if (chance > 100)
        chance = 100;

    // 4) Roll 0?99 so 'chance' behaves like a straight percentage.
    const uint8 roll = MakeRandomInt(0, 99);
    m_success = (roll < chance);

    _log(MODULE__DEBUG,
         "Prospector::CheckSuccess - baseDifficulty=%d, bonus=%d, finalChance=%d%%, roll=%u, result=%s",
         baseDifficulty,
         bonus,
         chance,
         roll,
         (m_success ? "SUCCESS" : "FAILURE"));
}

void Prospector::DropSalvage()
{
    if (m_targetSE == nullptr)
        return;

    // ----------------------------
    // 1) Salvage materials -> ship
    // ----------------------------
    std::vector<uint32> list;
    list.clear();
    sDataMgr.GetSalvage(atoi(m_targetSE->GetSelf()->customInfo().c_str()), list);

    if (!list.empty()) {
        uint8 drop = 0;
        switch (m_accessChance) {       // drop qty * rate in config
            case  30: drop = 1; break;  //  1 to 3
            case  20: drop = 2; break;  //  2 to 6
            case  10: drop = 3; break;  //  3 to 9
            case   0: drop = 4; break;  //  4 to 12
            case -10: drop = 5; break;  //  5 to 15
            case -20: drop = 6; break;  //  6 to 18
        }

        InventoryItemRef iRef(nullptr);
        Inventory* sInv(m_shipRef->GetMyInventory());
        uint32 quantity = 0;
        uint32 minDrop = drop;
        uint32 maxDrop = (drop * sConfig.rates.DropSalvage);

        if (sInv != nullptr) {
            for (auto cur : list) {
                // each drop has 50/50 chance. may need to change this later. base on char's salvage skill?
                if (IsEven(MakeRandomInt(0, 10)))
                    continue;

                quantity = MakeRandomInt(minDrop, maxDrop);
                ItemData iLoot(cur, pChar->itemID(), locTemp, flagNone, quantity);
                iRef = sItemFactory.SpawnItem(iLoot);
                if (iRef.get() == nullptr) // we'll get over it...continue
                    continue;

                if (sInv->HasAvailableSpace(m_holdFlag, iRef)) {
                    // place into cargo / salvage hold, merging with existing stacks
                    iRef->MergeTypesInCargo(m_shipRef.get(), m_holdFlag);
                    _log(MODULE__DEBUG,
                         "Prospector::DropSalvage - dropped %u %s of %u/%u",
                         quantity, iRef->name(), minDrop, maxDrop);
                } else {
                    _log(MODULE__DEBUG,
                         "Prospector::DropSalvage - %s's %s is full.",
                         m_shipRef->name(), sDataMgr.GetFlagName(m_holdFlag));
                    if (m_shipRef->HasPilot()) {
                        m_shipRef->GetPilot()->SendNotifyMsg(
                            "Your %s is full.  Remaining salvage is lost.",
                            sDataMgr.GetFlagName(m_holdFlag));
                    }
                    break;
                }
            }
        }
    }

    // ----------------------------------------
    // 2) Wreck loot -> jetcan (if any remains)
    //    IMPORTANT: grab loot BEFORE Salvaged()
    // ----------------------------------------
    std::map<uint32, InventoryItemRef> shipLoot;
    shipLoot.clear();

    Inventory* wreckInv = nullptr;
if (m_targetSE->GetSelf().get() != nullptr)
    wreckInv = m_targetSE->GetSelf()->GetMyInventory();


    if ((wreckInv != nullptr) && !wreckInv->IsEmpty()) {
        wreckInv->GetInventoryMap(shipLoot);

        if (!shipLoot.empty()) {
            // create new cargo container at wreck position
            ItemData p_idata(
                23,                                   // 23 = cargo container
                m_targetSE->GetSelf()->ownerID(),     // owner of wreck
                locTemp,
                flagNone,
                "Jettisoned Loot Container",
                m_targetSE->GetPosition());

            CargoContainerRef jetCanRef = sItemFactory.SpawnCargoContainer(p_idata);
            if (jetCanRef.get() != nullptr) {
                // move all loot items from wreck into the new can
                for (auto& cur : shipLoot)
                    cur.second->Move(jetCanRef->itemID(), flagNone);

                FactionData data;
                data.allianceID    = m_targetSE->GetAllianceID();
                data.corporationID = m_targetSE->GetCorporationID();
                data.factionID     = m_targetSE->GetWarFactionID();
                data.ownerID       = m_targetSE->GetSelf()->ownerID();

                ContainerSE* cSE = new ContainerSE(
                    jetCanRef,
                    m_targetSE->GetServices(),
                    m_sysMgr,
                    data);

                jetCanRef->SetMySE(cSE);
                m_sysMgr->AddEntity(cSE);

                // notify clients about the new can
                m_targetSE->DestinyMgr()->SendJettisonPacket();
            }
        }
    }

    // ----------------------------------------
    // 3) Mark wreck salvaged and remove it from space
    // ----------------------------------------
    if (m_targetSE->GetWreckSE() != nullptr)
        m_targetSE->GetWreckSE()->Salvaged();

    // Remove wreck entity from the system so it disappears
    // and target locks are cleared server-side.
    m_targetSE->Delete();
    m_targetSE = nullptr;

    // add data to StatisticMgr
    sStatMgr.Increment(Stat::shipsSalvaged);
}


void Prospector::DropItems()
{
    // this will be for data miners and hacking/archaeology shit.  dunno what all we'll need at this point.
    //  update StaticDataMgr for these items also.
}

/*
 *  accessDifficultyBonus       << salvage tackle(10), salvage tackleII(15),  salvage skill : salvagerI +5 per level, salvagerII +7 per level
 *  accessDifficulty (s:30,m:20,l:10,f:0,t2:0,o:-10,s:-20)           << in the item to salvage
 *
 *
 *  accessDifficultyBonus       << civilian analyzer(2), implant(5), analyzerII(7)
 *  accessDifficulty (0.000001)    << for analyzing structures ()
 *
 *
 */