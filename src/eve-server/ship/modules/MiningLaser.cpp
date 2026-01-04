/**
 * @name MiningLaser.cpp
 *   mining module class
 * @Author:         Allan
 * @date:      10 June 2015   -UD/RW 02 April 2017
 * @revised:  4 August 2017
 */

#include "eve-server.h"

#include "StaticDataMgr.h"
#include "StatisticMgr.h"
#include "character/Character.h"
#include "inventory/Inventory.h"
#include "ship/Ship.h"
#include "ship/modules/MiningLaser.h"
#include "system/SystemManager.h"
#include "system/cosmicMgrs/BeltMgr.h"

MiningLaser::MiningLaser(ModuleItemRef mRef, ShipItemRef sRef)
: ActiveModule(mRef, sRef)
{
    m_IsInitialCycle = true;
    m_rMiner = m_dcMiner = m_iMiner = m_gMiner = false;
    m_crystalDmg = m_crystalRoidGrp = m_crystalDmgAmount = m_crystalDmgChance = 0;

    if (m_modRef->groupID() == EVEDB::invGroups::Mining_Laser) {
        m_rMiner = true;
    } else if ((m_modRef->typeID() == 12108) || (m_modRef->typeID() == 18068) || (m_modRef->typeID() == 24305) || (m_modRef->typeID() == 28748)) {
        m_dcMiner = true;
    } else if ((m_modRef->typeID() == 16278) || (m_modRef->typeID() == 22229) || (m_modRef->typeID() == 22589) || (m_modRef->typeID() == 22591)
        || (m_modRef->typeID() == 22597) || (m_modRef->typeID() == 22599) || (m_modRef->typeID() == 28752)) {
        /* this includes 'dev testing modules' */
        m_iMiner = true;
    } else if (m_modRef->groupID() == EVEDB::invGroups::Gas_Cloud_Harvester) {
        m_gMiner = true;
    } else if (m_modRef->groupID() == EVEDB::invGroups::Frequency_Mining_Laser) {
        m_rMiner = true;
        m_reloadTime = 8000;    // this is not set in ActiveModule c'tor.  easier/cheaper to set here.
    } else if (m_modRef->groupID() == EVEDB::invGroups::Strip_Miner) {
        m_rMiner = true;
    }

    m_holdFlag = flagCargoHold;

    _log(MINING__TRACE, "MiningLaser Created for %s with %ums Duration.", mRef->name(), GetAttribute(AttrDuration).get_uint32());
}

void MiningLaser::LoadCharge(InventoryItemRef charge)
{
    ActiveModule::LoadCharge(charge);

    if (m_chargeRef.get() == nullptr)
        return;

    m_crystalDmg = m_chargeRef->GetAttribute(AttrDamage).get_float();
    m_crystalRoidGrp = m_chargeRef->GetAttribute(AttrSpecialisationAsteroidGroup).get_float();
    m_crystalDmgAmount = m_chargeRef->GetAttribute(AttrCrystalVolatilityDamage).get_float();
    m_crystalDmgChance = m_chargeRef->GetAttribute(AttrCrystalVolatilityChance).get_float();
}

void MiningLaser::UnloadCharge()
{
    _log(MODULE__TRACE, "%s calling ML::UnloadCharge()", m_modRef->name());
    m_crystalDmg = 0;
    m_crystalRoidGrp = 0;
    m_crystalDmgAmount = 0;
    m_crystalDmgChance = 0;

    ActiveModule::UnloadCharge();
}

bool MiningLaser::CanActivate()
{
    if (m_targetSE == nullptr){
        _log(MINING__WARNING, "Activate() - Invalid target: m_targetSE == nullptr");
        if (m_shipRef->HasPilot())
            m_shipRef->GetPilot()->SendNotifyMsg("Module Activate: Invalid target - Ref: ServerError 15628");
        return false;
    }

    bool canActivate(false);

    // verify module vs target for activation.  disallow if not compatible.  set special ore hold if applicable
    if (m_rMiner) {
        if ((m_targetSE->GetSelf()->categoryID() == EVEDB::invCategories::Asteroid)
        and (m_targetSE->GetSelf()->groupID() != EVEDB::invGroups::Mercoxit)) {
            canActivate = true;
            if (m_shipRef->HasAttribute(AttrOreHoldCapacity))
                m_holdFlag = flagOreHold;
        }
    } else if (m_dcMiner) {
        if (m_targetSE->GetSelf()->groupID() == EVEDB::invGroups::Mercoxit) {
            canActivate = true;
            if (m_shipRef->HasAttribute(AttrOreHoldCapacity))
                m_holdFlag = flagOreHold;
        }
    } else if (m_iMiner) {
        if (m_targetSE->GetSelf()->groupID() == EVEDB::invGroups::Ice) {
            canActivate = true;
            if (m_shipRef->HasAttribute(AttrOreHoldCapacity))
                m_holdFlag = flagOreHold;
        }
    } else if (m_gMiner) {
        if (m_targetSE->GetSelf()->groupID() == EVEDB::invGroups::Harvestable_Cloud) {
            canActivate = true;
            if (m_shipRef->HasAttribute(AttrGasHoldCapacity))
                m_holdFlag = flagGasHold;
        }
    }

    if (canActivate) {
        // arm: first DoCycle call is "start timer" only, no mining.
        m_IsInitialCycle = true;

        // keep belt marked active
        m_targetSE->SystemMgr()->GetBeltMgr()->SetActive(m_targetSE->SysBubble()->GetID());

        // mining on current target approved. check for and set crystal variables here
        if (m_chargeLoaded and (m_chargeRef.get() != nullptr) and (m_crystalRoidGrp == 0)) {
            m_crystalDmg = m_chargeRef->GetAttribute(AttrDamage).get_float();
            m_crystalRoidGrp = m_chargeRef->GetAttribute(AttrSpecialisationAsteroidGroup).get_float();
            m_crystalDmgAmount = m_chargeRef->GetAttribute(AttrCrystalVolatilityDamage).get_float();
            m_crystalDmgChance = m_chargeRef->GetAttribute(AttrCrystalVolatilityChance).get_float();
        }

        return ActiveModule::CanActivate();
    } else {
        _log(MINING__WARNING, "Activate() - Invalid target: %s", m_targetSE->GetName());
        if (m_shipRef->HasPilot())
            m_shipRef->GetPilot()->SendNotifyMsg("Module Activate: %s is an invalid target - Ref: ServerError 15628", m_targetSE->GetName());
    }

    return false;
}


uint32 MiningLaser::DoCycle()
{
    if (!m_shipRef || !m_modRef)
        return 0;

    if (m_targetSE == nullptr || m_targetID == 0)
    {
        AbortCycle();
        return 0;
    }

    // Range check (prevents mining out of range; stops cleanly).
    const double maxRange = m_modRef->GetAttribute(AttrMaxRange).get_double();
    if (maxRange > 0.0)
    {
        const double dist =
            m_shipRef->position().distance(m_targetSE->GetPosition()) - (double)m_targetSE->GetRadius();

        if (dist > maxRange)
        {
            if (m_shipRef->HasPilot())
                m_shipRef->GetPilot()->SendNotifyMsg("%s is out of range.", m_targetSE->GetName());

            AbortCycle();
            return 0;
        }
    }

    // Consume per-cycle costs (cap, charge usage, etc). If this fails, do not mine.
    // NOTE: In your tree ActiveModule::DoCycle() is used as the "can cycle" gate.
    if (ActiveModule::DoCycle() == 0)
        return 0;

    // Determine true mining cycle time (ms). Prefer AttrDuration (Crucible behavior).
    uint32 cycleTime = 0;

    if (m_modRef->HasAttribute(AttrDuration))
        cycleTime = m_modRef->GetAttribute(AttrDuration).get_uint32();

    // fallback: some modules use AttrSpeed in older data
    if (cycleTime == 0 && m_modRef->HasAttribute(AttrSpeed))
        cycleTime = m_modRef->GetAttribute(AttrSpeed).get_uint32();

    // final fallback: do not allow 0ms cycling
    if (cycleTime == 0)
        cycleTime = 60000;

    // IMPORTANT:
    // ActiveModule::Activate() calls DoCycle() once immediately to arm the timer.
    // Do not mine on that call; first mining happens after one full cycle completes.
    if (m_IsInitialCycle)
    {
        m_IsInitialCycle = false;
        return cycleTime;
    }

    // End-of-cycle mining deposit
    ProcessCycle(false);

    return m_Stop ? 0 : cycleTime;
}



void MiningLaser::DeactivateCycle(bool abort/*false*/)
{
    if (m_ModuleState != Module::State::Deactivating)
        return;

    // Stop dogma + visual FX
    ApplyEffect(FX::State::Active, false);
    ShowEffect(false, abort);

    // Only resolve mining on ABORT (partial cycle).
    // Full-cycle mining happens in DoCycle() on timer boundaries.
    if (abort && (m_targetSE != nullptr))
        ProcessCycle(true);

    // Reset hold selection
    m_holdFlag = flagCargoHold;

    // Post-deactivation resting state in this tree
    SetModuleState(Module::State::Online);
    Clear();
}



void MiningLaser::ProcessCycle(bool abort/*false*/)
{
    if (m_targetSE == nullptr)
        return;

    const float cycleVol = GetMiningVolume();

    InventoryItemRef roidRef(m_targetSE->GetSelf());
    if (roidRef.get() == nullptr)
        return;

    const float oreVolume = roidRef->GetAttribute(AttrVolume).get_float();
    if ((cycleVol <= 0.0f) || (oreVolume <= 0.0f) || (cycleVol < oreVolume))
        return;

    // Remaining ore on asteroids in this file is handled via AttrQuantity (see Depleted()).
    float roidQty = roidRef->GetAttribute(AttrQuantity).get_float();
    if (roidQty <= 0.0f)
        return;

    // Units per cycle = mined volume / ore volume
    float oreAmountF = (cycleVol / oreVolume);

    if (abort)
    {
        // partial-cycle scale (0..1)
        float delta = 1.0f - (GetRemainingCycleTimeMS() / GetAttribute(AttrDuration).get_float());
        if (delta < 0.0f) delta = 0.0f;
        if (delta > 1.0f) delta = 1.0f;
        oreAmountF *= delta;
    }

    uint32 oreAmount = (uint32)floor(oreAmountF + 0.0001f);
    if (oreAmount < 1)
        return;

    // Clamp to remaining ore
    if ((float)oreAmount > roidQty)
        oreAmount = (uint32)floor(roidQty + 0.0001f);

    if (oreAmount < 1)
        return;

    Client* const pClient = m_shipRef->GetPilot();
    const uint32 ownerID  = (pClient != nullptr) ? pClient->GetCharacterID() : m_shipRef->ownerID();

    // Spawn ore into locTemp first (forces a true Move/registration path).
    ItemData oreData(
        roidRef->typeID(),
        ownerID,
        locTemp,
        flagNone,
        oreAmount
    );

    InventoryItemRef oreRef(sItemFactory.SpawnItem(oreData));
    if (oreRef.get() == nullptr)
        return;

    // Ore must be stackable
    if (oreRef->isSingleton())
        oreRef->ChangeSingleton(false, false);

    if (!m_shipRef->GetMyInventory()->HasAvailableSpace(m_holdFlag, oreRef))
    {
        if (m_shipRef->HasPilot())
            m_shipRef->GetPilot()->SendNotifyMsg("Your cargo is full.");

        m_shipRef->CargoFull();
        ActiveModule::DeactivateCycle(true);
        return;
    }

    // Move ore into hold and notify via ship/inventory path.
    const uint32 movedItemID = m_shipRef->AddItemByFlag(m_holdFlag, oreRef, pClient);
    if (movedItemID == 0)
        return;

    // Safe merge: never merge into itself
    Inventory* shipInv = m_shipRef->GetMyInventory();
    if (shipInv != nullptr)
    {
        InventoryItemRef existing = shipInv->GetByTypeFlag(roidRef->typeID(), m_holdFlag);
        if (existing.get() != nullptr && existing->itemID() != oreRef->itemID())
        {
            if (!existing->isSingleton() && !oreRef->isSingleton())
                existing->Merge(oreRef, 0, true);
        }
    }

    // Deplete asteroid using AttrQuantity
    roidQty -= (float)oreAmount;
    if (roidQty < 0.0f)
        roidQty = 0.0f;

    roidRef->SetAttribute(AttrQuantity, roidQty, true);

    // If depleted, belt manager will handle cleanup logic on its pass; we just stop if it is empty.
    if (roidQty <= 0.0f)
        ActiveModule::DeactivateCycle(true);
}

/* --- The rest of your file (Depleted, AddOreAndDeactivate, GetMiningVolume, comments) stays unchanged --- */

void MiningLaser::Depleted(std::multimap<float, MiningLaser*> &mMap)
{
    float total = GetMiningVolume(), percent = 0.0f;
    for (auto cur : mMap)
        total += cur.first;

    InventoryItemRef roidRef(m_targetSE->GetSelf());
    float oreVolume(roidRef->GetAttribute(AttrVolume).get_float());
    if (oreVolume <= 0) {
        _log(MINING__ERROR,
             "%s(%u) - Depleted() -  oreVolume is <0 for %s(%u)",
             m_modRef->name(), m_modRef->itemID(), roidRef->name(), m_targetSE->GetID() );

        for (auto cur : mMap) {
            cur.second->GetShipRef()->GetPilot()->SendNotifyMsg(
                "Your %s deactivates because there was a processing error.  Ref: ServerError 03123.",
                cur.second->GetSelf()->name());
            cur.second->CancelOnError();
        }
        return;
    }

    float roidQuantity(roidRef->GetAttribute(AttrQuantity).get_float());
    double oreAmount(0);

    for (auto cur : mMap) {
        if ((cur.first < oreVolume) or (cur.first < 0.1)) {
            _log(MINING__ERROR,
                 "%s(%u) - Depleted() -  Mining Laser could not extract ore from %s(%u)",
                 cur.second->GetSelf()->name(), cur.second->GetSelf()->itemID(),
                 roidRef->name(), m_targetSE->GetID() );
            cur.second->GetShipRef()->GetPilot()->SendNotifyMsg(
                "Your %s deactivates because there was an error in it's processing.  Ref: ServerError 06428.",
                cur.second->GetSelf()->name());
            cur.second->CancelOnError();
            continue;
        }

        percent = cur.first / total;
        oreAmount = roidQuantity * percent;

        cur.second->AddOreAndDeactivate(roidRef->typeID(), oreAmount);

        PyTuple* tuple = new PyTuple(2);
        tuple->SetItem(0, new PyString("MiningItemDepleted"));
        PyDict* dict = new PyDict();
        dict->SetItemString("modulename", new PyString(cur.second->GetSelf()->itemName()));
        tuple->SetItem(1, dict);
        cur.second->GetShipRef()->GetPilot()->QueueDestinyUpdate(&tuple);
    }

    percent = GetMiningVolume() / total;
    AddOreAndDeactivate(roidRef->typeID(), roidQuantity * percent, false);
}

void MiningLaser::AddOreAndDeactivate(uint16 typeID, float amt, bool slave/*true*/)
{
    Client* const pClient = (m_shipRef ? m_shipRef->GetPilot() : nullptr);
    if (!m_shipRef || !m_modRef || !pClient)
        return;

    const uint32 quantity = (uint32)floor(amt + 0.0001f);
    if (quantity < 1) {
        ActiveModule::DeactivateCycle(true);
        return;
    }

    ItemData oreData(
        typeID,
        m_shipRef->ownerID(),
        m_shipRef->itemID(),  // containerItemID
        m_holdFlag,           // correct hold flag
        quantity
    );

    InventoryItemRef oreRef(sItemFactory.SpawnItem(oreData));
    if (oreRef.get() == nullptr) {
        _log(MINING__ERROR, "%s(%u) - Failed to spawn ore item type %u x%u",
             m_modRef->name(), m_modRef->itemID(), typeID, quantity);
        ActiveModule::DeactivateCycle(true);
        return;
    }

    oreRef->MergeTypesInCargo(m_shipRef.get(), m_holdFlag);

    if (!slave) {
        pClient->SendNotifyMsg("%s deactivates; target depleted.", m_modRef->name());
    }

    ActiveModule::DeactivateCycle(true);
}

float MiningLaser::GetMiningVolume()
{
    float cycleVol(GetAttribute(AttrMiningAmount).get_float());
    if (m_chargeLoaded)
        if (m_targetSE->GetGroupID() == m_crystalRoidGrp)
            cycleVol = GetAttribute(AttrSpecialtyMiningAmount).get_float();

    if (m_shipRef->HasPilot()) {
        ShipSE* pShip(m_shipRef->GetPilot()->GetShipSE());
        if (pShip != nullptr)
            if (pShip->IsBoosted())
                cycleVol *= (1 + (0.03f * pShip->GetMiningBoostAmount())); // 3% increase/level
    }

    return cycleVol;
}
