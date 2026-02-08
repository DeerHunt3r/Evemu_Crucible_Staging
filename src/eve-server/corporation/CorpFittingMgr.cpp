#include "eve-server.h"
#include "corporation/CorpFittingMgr.h"

// Fix incomplete type 'Client' (Callable.h only forward declares it)
#include "Client.h"

#include "inventory/ItemFactory.h"
#include "inventory/InventoryItem.h"
#include "inventory/Inventory.h"


CorpFittingMgr::CorpFittingMgr()
    : Service("corpFittingMgr", eAccessLevel_Corporation)
{
    this->Add("GetFittings", &CorpFittingMgr::GetFittings);
    this->Add("SaveFitting", &CorpFittingMgr::SaveFitting);
    this->Add("SaveManyFittings", &CorpFittingMgr::SaveManyFittings);
    this->Add("DeleteFitting", &CorpFittingMgr::DeleteFitting);
    this->Add("UpdateNameAndDescription", &CorpFittingMgr::UpdateNameAndDescription);

    this->Add("FitFitting", &CorpFittingMgr::FitFitting);
}

PyResult CorpFittingMgr::GetFittings(PyCallArgs& call, PyInt* ownerID)
{
    _log(CORP__CALL, "CorpFittingMgr::Handle_GetFittings()");
    call.Dump(CORP__CALL_DUMP);

    // MUST be iterable
    return new PyDict();
}

PyResult CorpFittingMgr::SaveFitting(PyCallArgs& call, PyInt* ownerID, PyObject* fitting)
{
    _log(CORP__CALL, "CorpFittingMgr::Handle_SaveFitting()");
    call.Dump(CORP__CALL_DUMP);

    return new PyInt(0);
}

PyResult CorpFittingMgr::SaveManyFittings(PyCallArgs& call, PyInt* ownerID, PyDict* fittingsToSave)
{
    _log(CORP__CALL, "CorpFittingMgr::Handle_SaveManyFittings()");
    call.Dump(CORP__CALL_DUMP);

    return new PyList();
}

PyResult CorpFittingMgr::DeleteFitting(PyCallArgs& call, PyInt* ownerID, PyInt* fittingID)
{
    _log(CORP__CALL, "CorpFittingMgr::Handle_DeleteFitting()");
    call.Dump(CORP__CALL_DUMP);

    return nullptr;
}

PyResult CorpFittingMgr::UpdateNameAndDescription(PyCallArgs& call, PyInt* fittingID, PyInt* ownerID, PyWString* name, PyWString* description)
{
    _log(CORP__CALL, "CorpFittingMgr::Handle_UpdateNameAndDescription()");
    call.Dump(CORP__CALL_DUMP);

    return nullptr;
}

PyResult CorpFittingMgr::FitFitting(PyCallArgs& call,
                                   PyInt* ownerID,
                                   PyObjectEx* /*fitting*/,
                                   PyInt* fittingID,
                                   PyDict* dronesByType,
                                   PyDict* itemTypes)
{
    // Client expects iterable
    PyList* failures = new PyList();

    _log(CORP__CALL,
         "CorpFittingMgr::FitFitting ownerID=%u fittingID=%u dronesByType=%u itemTypes=%u",
         (ownerID ? ownerID->value() : 0),
         (fittingID ? fittingID->value() : 0),
         (dronesByType ? dronesByType->size() : 0),
         (itemTypes ? itemTypes->size() : 0));

    call.Dump(CORP__CALL_DUMP);

    // For now, corp fitting delegates to same logic assumptions as character fitting:
    // fitting from personal hangar to active ship. (Corp hangar fitting is a separate behavior.)
    if (call.client == nullptr || ownerID == nullptr || fittingID == nullptr)
        return failures;

    const uint32 charID = call.client->GetCharacterID();
    const uint32 stationID = call.client->GetStationID();
    const uint32 shipID = call.client->GetShipID();

    if (ownerID->value() != charID || stationID == 0 || shipID == 0)
        return failures;

    InventoryItemRef stationRef = sItemFactory.GetItemRef(stationID);
    if (stationRef.get() == nullptr)
        return failures;

    Inventory* stationInv = stationRef->GetMyInventory();
    if (stationInv == nullptr)
        return failures;

    stationInv->LoadContents();

    std::vector<InventoryItemRef> ownedAtStation;
    stationInv->GetInvForOwner(charID, ownedAtStation);

    DBQueryResult res;
    if (!sDatabase.RunQuery(res,
        "SELECT s.itemID, s.flagID, s.qty "
        "FROM shipFittings AS s "
        "INNER JOIN chrShipFittings AS c ON c.id = s.fittingID "
        "WHERE s.fittingID = %u AND c.characterID = %u",
        fittingID->value(), charID))
    {
        _log(DATABASE__ERROR, "CorpFittingMgr::FitFitting - DB query failed: %s", res.error.c_str());
        return failures;
    }

    DBResultRow row;
    while (res.GetRow(row))
    {
        const uint32 typeID = row.GetInt(0);
        const EVEItemFlags flag = (EVEItemFlags)row.GetInt(1);
        uint32 remaining = row.GetInt(2);

        while (remaining > 0)
        {
            InventoryItemRef found(nullptr);

            for (auto& cur : ownedAtStation) {
                if (cur.get() == nullptr)
                    continue;
                if (cur->flag() != flagHangar)
                    continue;
                if (cur->typeID() != typeID)
                    continue;
                if (cur->quantity() < 1)
                    continue;

                found = cur;
                break;
            }

            if (found.get() == nullptr) {
                failures->AddItem(new_tuple(new PyInt(typeID), new PyInt(remaining)));
                break;
            }

            if (!found->isSingleton() && found->quantity() > 1 && remaining < (uint32)found->quantity()) {
                InventoryItemRef split = found->Split((int32)remaining, true, false);
                if (split.get() != nullptr) {
                    split->Move(shipID, flag, true);
                    remaining = 0;
                } else {
                    found->Move(shipID, flag, true);
                    remaining = 0;
                }
            } else {
                found->Move(shipID, flag, true);

                if (!found->isSingleton() && found->quantity() > 1) {
                    remaining = 0;
                } else {
                    --remaining;
                }
            }
        }
    }

    return failures;
}
