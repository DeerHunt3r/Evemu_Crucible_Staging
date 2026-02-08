#include "eve-server.h"
#include "character/CharFittingMgr.h"

// Fix incomplete type 'Client' (Callable.h only forward declares it)
#include "Client.h"   // <- if your tree uses a different path, see notes below

#include "inventory/ItemFactory.h"
#include "inventory/InventoryItem.h"
#include "inventory/Inventory.h"


CharFittingMgr::CharFittingMgr() : Service("charFittingMgr")
{
    this->Add("GetFittings", &CharFittingMgr::GetFittings);
    this->Add("SaveFitting", &CharFittingMgr::SaveFitting);
    this->Add("SaveManyFittings", &CharFittingMgr::SaveManyFittings);
    this->Add("DeleteFitting", &CharFittingMgr::DeleteFitting);
    this->Add("UpdateNameAndDescription", &CharFittingMgr::UpdateNameAndDescription);

    // Fit button RPC
    this->Add("FitFitting", &CharFittingMgr::FitFitting);
}

PyResult CharFittingMgr::GetFittings(PyCallArgs& call, PyInt* ownerID)
{
    _log(PLAYER__CALL, "CharFittingMgr::Handle_GetFittings()");
    call.Dump(PLAYER__CALL_DUMP);

    return m_db.GetCharacterShipFittings(ownerID->value());
}

PyResult CharFittingMgr::SaveFitting(PyCallArgs& call, PyInt* ownerID, PyObject* fitting)
{
    if (fitting == nullptr || fitting->arguments() == nullptr || !fitting->arguments()->IsDict()) {
        codelog(SERVICE__ERROR, "CharFittingMgr::Handle_SaveFitting() - passed fitting is not a dict.");
        return nullptr;
    }

    PyDict* fittingData = fitting->arguments()->AsDict();
    const uint32 fittingID = m_db.SaveCharShipFitting(*fittingData, ownerID->value());
    return new PyInt(fittingID);
}

PyResult CharFittingMgr::SaveManyFittings(PyCallArgs& call, PyInt* ownerID, PyDict* fittingsToSave)
{
    _log(PLAYER__CALL, "CharFittingMgr::Handle_SaveManyFittings()");
    call.Dump(PLAYER__CALL_DUMP);

    // MUST be iterable for the client
    return new PyList();
}

PyResult CharFittingMgr::DeleteFitting(PyCallArgs& call, PyInt* ownerID, PyInt* fittingID)
{
    DBerror err;

    if (sDatabase.RunQuery(err,
        "DELETE FROM chrShipFittings WHERE id = %u AND characterID = %u",
        fittingID->value(), ownerID->value()))
    {
        sDatabase.RunQuery(err,
            "DELETE FROM shipFittings WHERE fittingID = %u",
            fittingID->value());
    }
    else
    {
        _log(DATABASE__ERROR, "Error deleting fitting %u for character %u: %s",
             fittingID->value(), ownerID->value(), err.c_str());
    }

    return nullptr;
}

PyResult CharFittingMgr::UpdateNameAndDescription(PyCallArgs& call,
                                                 PyInt* fittingID,
                                                 PyInt* ownerID,
                                                 PyWString* name,
                                                 PyWString* description)
{
    std::string cName, cDescription;
    sDatabase.DoEscapeString(cName, name->content());
    sDatabase.DoEscapeString(cDescription, description->content());

    DBerror err;
    if (!sDatabase.RunQuery(err,
        "UPDATE chrShipFittings SET name = '%s', description = '%s' WHERE id = %u AND characterID = %u",
        cName.c_str(), cDescription.c_str(),
        fittingID->value(), ownerID->value()))
    {
        _log(DATABASE__ERROR, "Error updating fitting %u for character %u: %s",
             fittingID->value(), ownerID->value(), err.c_str());
    }

    return nullptr;
}

PyResult CharFittingMgr::FitFitting(PyCallArgs& call,
                                   PyInt* ownerID,
                                   PyObjectEx* /*fitting*/,
                                   PyInt* fittingID,
                                   PyDict* dronesByType,
                                   PyDict* itemTypes)
{
    // Client expects iterable, and iterates failures.
    PyList* failures = new PyList();

    _log(PLAYER__CALL,
         "CharFittingMgr::FitFitting ownerID=%u fittingID=%u dronesByType=%u itemTypes=%u",
         (ownerID ? ownerID->value() : 0),
         (fittingID ? fittingID->value() : 0),
         (dronesByType ? dronesByType->size() : 0),
         (itemTypes ? itemTypes->size() : 0));

    call.Dump(PLAYER__CALL_DUMP);

    if (call.client == nullptr || ownerID == nullptr || fittingID == nullptr) {
        _log(SERVICE__ERROR, "CharFittingMgr::FitFitting - missing call.client/ownerID/fittingID");
        return failures;
    }

    const uint32 charID = call.client->GetCharacterID();
    const uint32 stationID = call.client->GetStationID();
    const uint32 shipID = call.client->GetShipID();

    // Safety: only allow fitting your own fittings
    if (ownerID->value() != charID) {
        _log(SERVICE__ERROR, "CharFittingMgr::FitFitting - owner mismatch (arg=%u session=%u)",
             ownerID->value(), charID);
        return failures;
    }

    if (stationID == 0 || shipID == 0) {
        _log(SERVICE__ERROR, "CharFittingMgr::FitFitting - invalid stationID(%u) or shipID(%u)", stationID, shipID);
        return failures;
    }

    // Load station inventory contents for this character
    InventoryItemRef stationRef = sItemFactory.GetItemRef(stationID);
    if (stationRef.get() == nullptr) {
        _log(SERVICE__ERROR, "CharFittingMgr::FitFitting - unable to resolve station item %u", stationID);
        return failures;
    }

    Inventory* stationInv = stationRef->GetMyInventory();
    if (stationInv == nullptr) {
        _log(SERVICE__ERROR, "CharFittingMgr::FitFitting - station inventory missing for %u", stationID);
        return failures;
    }

    stationInv->LoadContents();

    std::vector<InventoryItemRef> ownedAtStation;
    stationInv->GetInvForOwner(charID, ownedAtStation);

    // Pull fitting rows from DB, and verify it belongs to this character
    DBQueryResult res;
    if (!sDatabase.RunQuery(res,
        "SELECT s.itemID, s.flagID, s.qty "
        "FROM shipFittings AS s "
        "INNER JOIN chrShipFittings AS c ON c.id = s.fittingID "
        "WHERE s.fittingID = %u AND c.characterID = %u",
        fittingID->value(), charID))
    {
        _log(DATABASE__ERROR, "CharFittingMgr::FitFitting - DB query failed: %s", res.error.c_str());
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

                // Fit from personal hangar at station
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
                // Could not locate enough items to satisfy this line
                failures->AddItem(new_tuple(new PyInt(typeID), new PyInt(remaining)));
                break;
            }

            // Stack handling: split if needed
            if (!found->isSingleton() && found->quantity() > 1 && remaining < (uint32)found->quantity()) {
                InventoryItemRef split = found->Split((int32)remaining, true, false);
                if (split.get() != nullptr) {
                    split->Move(shipID, flag, true);
                    remaining = 0;
                } else {
                    // Split failed, fall back to moving the whole stack
                    found->Move(shipID, flag, true);
                    remaining = 0;
                }
            } else {
                // Singleton or full stack satisfies remaining
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
