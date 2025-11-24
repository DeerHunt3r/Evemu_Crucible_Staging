/*
    ------------------------------------------------------------------------------------
    LICENSE:
    ------------------------------------------------------------------------------------
    This file is part of EVEmu: EVE Online Server Emulator
    Copyright 2006 - 2021 The EVEmu Team
    For the latest information visit https://evemu.dev
    ------------------------------------------------------------------------------------
    This program is free software; you can redistribute it and/or modify it under
    the terms of the GNU Lesser General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any later
    version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License along with
    this program; if not, write to the Free Software Foundation, Inc., 59 Temple
    Place - Suite 330, Boston, MA 02111-1307, USA, or go to
    http://www.gnu.org/copyleft/lesser.txt.
    ------------------------------------------------------------------------------------
    Author:        Zhur
    Updates:    Allan
*/

#include "eve-server.h"

#include "../../eve-common/EVE_Character.h"



#include "EntityList.h"
#include "account/AccountService.h"
#include "character/CharMgrService.h"
#include "packets/CorporationPkts.h"
#include "services/ServiceManager.h"


// ➊ Add this helper:
template<typename T>
inline T* PySafeCast(PyRep* rep) {
    return dynamic_cast<T*>(rep);
}

CharMgrBound::CharMgrBound(EVEServiceManager& mgr, CharMgrService& parent, uint32 ownerID, uint16 contFlag) :
    EVEBoundObject(mgr, parent),
    m_ownerID(ownerID),
    m_containerFlag(contFlag)
{
    this->Add("List", &CharMgrBound::List);
    this->Add("ListStations", static_cast <PyResult (CharMgrBound::*)(PyCallArgs&, PyInt*, PyBool*)> (&CharMgrBound::ListStations));
    this->Add("ListStations", static_cast <PyResult (CharMgrBound::*)(PyCallArgs&, PyInt*, PyInt*)> (&CharMgrBound::ListStations));
    this->Add("ListStationItems", &CharMgrBound::ListStationItems);
    this->Add("ListStationBlueprintItems", &CharMgrBound::ListStationBlueprintItems);
}

PyResult CharMgrBound::List(PyCallArgs& call)
{
    return CharacterDB::List(m_ownerID);
}

PyResult CharMgrBound::ListStationItems(PyCallArgs& call, PyInt* stationID)
{
    // this is the assets window
    return CharacterDB::ListStationItems(m_ownerID, stationID->value());
}

PyResult CharMgrBound::ListStations(PyCallArgs& call, PyInt* blueprintOnly, PyBool* isCorporation)
{
  return ListStations(call, blueprintOnly, new PyInt(isCorporation->value()));
}

PyResult CharMgrBound::ListStations(PyCallArgs& call, PyInt* blueprintOnly, PyInt* isCorporation)
{
    //stations = sm.GetService('invCache').GetInventory(const.containerGlobal).ListStations(blueprintOnly, isCorp)

    call.Dump(CHARACTER__DEBUG);
    std::ostringstream flagIDs;
    flagIDs << flagHangar;
    /** @todo  test m_containerFlag to determine correct flag here and append to flagIDs string? */

    uint32 ownerID(m_ownerID);

    if (isCorporation->value()) {
        Client* pClient = sEntityList.FindClientByCharID(m_ownerID);
        if (pClient == nullptr)
            return nullptr; // make error here
        ownerID = pClient->GetCorporationID();
        //  add corp hangar flags
        flagIDs << "," << flagCorpHangar2 << "," << flagCorpHangar3 << "," << flagCorpHangar4 << ",";
        flagIDs << flagCorpHangar5 << "," << flagCorpHangar6 << "," << flagCorpHangar7;
    }

    return CharacterDB::ListStations(ownerID, flagIDs, isCorporation->value(), blueprintOnly->value());
}

PyResult CharMgrBound::ListStationBlueprintItems(PyCallArgs& call, PyInt* locationID, PyInt* stationID, PyInt* forCorporation)
{
    // this is the BP tab of the S&I window
    call.Dump(CHARACTER__DEBUG);

    /** @todo whats diff between stationID and locationID?
     *  none that i see so far...
     * this could be diff between station and pos   will need to check later (locationID == stationID)
     */
    uint32 ownerID(m_ownerID);

    if (forCorporation->value()) { //forCorp
        Client* pClient = sEntityList.FindClientByCharID(m_ownerID);
        if (pClient == nullptr)
            return nullptr; // make error here
        ownerID = pClient->GetCorporationID();
    }
    return CharacterDB::ListStationBlueprintItems(ownerID, stationID->value(), forCorporation->value());
}

CharMgrService::CharMgrService(EVEServiceManager& mgr) :
    BindableService("charMgr", mgr, eAccessLevel_Character)
{
    this->Add("GetPublicInfo", &CharMgrService::GetPublicInfo);
    this->Add("GetPublicInfo3", &CharMgrService::GetPublicInfo3);
    this->Add("GetPrivateInfo", &CharMgrService::GetPrivateInfo);
    this->Add("GetTopBounties", &CharMgrService::GetTopBounties);
    this->Add("AddToBounty", &CharMgrService::AddToBounty);
    this->Add("GetContactList", &CharMgrService::GetContactList);
    this->Add("GetCloneTypeID", &CharMgrService::GetCloneTypeID);
    this->Add("GetHomeStation", &CharMgrService::GetHomeStation);
    this->Add("GetFactions", &CharMgrService::GetFactions);
    this->Add("SetActivityStatus", &CharMgrService::SetActivityStatus);
    this->Add("GetSettingsInfo", &CharMgrService::GetSettingsInfo);
    this->Add("LogSettings", &CharMgrService::LogSettings);
    this->Add("GetCharacterDescription", &CharMgrService::GetCharacterDescription);
    this->Add("SetCharacterDescription", &CharMgrService::SetCharacterDescription);
    this->Add("GetPaperdollState", &CharMgrService::GetPaperdollState);
    this->Add("GetNote", &CharMgrService::GetNote);
    this->Add("SetNote", &CharMgrService::SetNote);
    // Notepad / owner notes
    this->Add("AddOwnerNote",
        static_cast<PyResult (CharMgrService::*)(PyCallArgs&, PyString*, PyWString*)>(
            &CharMgrService::AddOwnerNote));
    this->Add("AddOwnerNote",
        static_cast<PyResult (CharMgrService::*)(PyCallArgs&, PyWString*, PyString*)>(
            &CharMgrService::AddOwnerNote));

    this->Add("GetOwnerNote", &CharMgrService::GetOwnerNote);
    this->Add("GetOwnerNoteLabels", &CharMgrService::GetOwnerNoteLabels);
   
   // EditOwnerNote overloads
this->Add(
    "EditOwnerNote",
    static_cast<PyResult (CharMgrService::*)(PyCallArgs&, PyInt*, PyWString*, PyWString*)>(
        &CharMgrService::EditOwnerNote)
);

this->Add(
    "EditOwnerNote",
    static_cast<PyResult (CharMgrService::*)(PyCallArgs&, PyInt*, PyWString*)>(
        &CharMgrService::EditOwnerNote)
);

this->Add(
    "EditOwnerNote",
    static_cast<PyResult (CharMgrService::*)(PyCallArgs&, PyInt*, PyString*, PyWString*)>(
        &CharMgrService::EditOwnerNote)
);

this->Add("EditOwnerNote",
    static_cast<PyResult (CharMgrService::*)(PyCallArgs&, PyInt*, PyWString*, PyString*)>(
        &CharMgrService::EditOwnerNote));
   

    this->Add("RemoveOwnerNote", &CharMgrService::RemoveOwnerNote);
    this->Add("AddContact", static_cast <PyResult(CharMgrService::*)(PyCallArgs&, PyInt*, PyInt*, PyInt*, PyInt*, std::optional<PyString*>)> (&CharMgrService::AddContact));
    this->Add("AddContact", static_cast <PyResult(CharMgrService::*)(PyCallArgs&, PyInt*, PyFloat*, PyInt*, PyBool*, std::optional<PyWString*>)> (&CharMgrService::AddContact));
    this->Add("EditContact", static_cast <PyResult(CharMgrService::*)(PyCallArgs&, PyInt*, PyInt*, PyInt*, PyInt*, std::optional<PyString*>)> (&CharMgrService::EditContact));
    this->Add("EditContact", static_cast <PyResult(CharMgrService::*)(PyCallArgs&, PyInt*, PyFloat*, PyInt*, PyBool*, std::optional<PyWString*>)> (&CharMgrService::EditContact));
    this->Add("DeleteContacts", &CharMgrService::DeleteContacts);
    this->Add("GetRecentShipKillsAndLosses", &CharMgrService::GetRecentShipKillsAndLosses);
    this->Add("BlockOwners", &CharMgrService::BlockOwners);
    this->Add("UnblockOwners", &CharMgrService::UnblockOwners);
    this->Add("EditContactsRelationshipID", &CharMgrService::EditContactsRelationshipID);
    this->Add("GetImageServerLink", &CharMgrService::GetImageServerLink);
    //these 2 are for labels in PnP window;
    this->Add("GetLabels", &CharMgrService::GetLabels);
    this->Add("CreateLabel", &CharMgrService::CreateLabel);
}

BoundDispatcher* CharMgrService::BindObject(Client *client, PyRep* bindParameters) {
    _log(CHARACTER__BIND, "CharMgrService bind request:");
    bindParameters->Dump(CHARACTER__BIND, "    ");
    Call_TwoIntegerArgs args;
    //crap
    PyRep* tmp(bindParameters->Clone());
    if (!args.Decode(&tmp)) {
        codelog(SERVICE__ERROR, "%s: Failed to decode arguments.", GetName());
        return nullptr;
    }

    return new CharMgrBound(this->GetServiceManager(), *this, args.arg1, args.arg2);
}

void CharMgrService::BoundReleased(CharMgrBound *bound) {
    // nothing to be done here, this bound service is a singleton
}

PyResult CharMgrService::GetImageServerLink(PyCallArgs& call)
{
    // only called by billboard service for bounties...
    //  serverLink = sm.RemoteSvc('charMgr').GetImageServerLink()
    std::stringstream urlBuilder;
    urlBuilder << "http://" << sConfig.net.imageServer << ":" << sConfig.net.imageServerPort << "/";

    return new PyString(urlBuilder.str());
}

PyResult CharMgrService::GetRecentShipKillsAndLosses(PyCallArgs& call, PyInt* num, std::optional<PyInt*> startIndex)
{   /* cached object - can return db object as DBResultToCRowset*/
    return m_db.GetKillOrLoss(call.client->GetCharacterID());
}

PyResult CharMgrService::GetTopBounties(PyCallArgs& call)
{
    return m_db.GetTopBounties();
}

PyResult CharMgrService::GetLabels(PyCallArgs& call)
{
    return m_db.GetLabels(call.client->GetCharacterID());
}

PyResult CharMgrService::GetPaperdollState(PyCallArgs& call)
{
    return new PyInt(Char::PDState::NoRecustomization);
}

PyResult CharMgrService::GetPublicInfo3(PyCallArgs &call, PyInt* characterID)
{
    return m_db.GetCharPublicInfo3(characterID->value());
}

PyResult CharMgrService::GetContactList(PyCallArgs &call)
{
    PyDict* dict = new PyDict();
        dict->SetItemString("addresses", m_db.GetContacts(call.client->GetCharacterID(), false));
        dict->SetItemString("blocked", m_db.GetContacts(call.client->GetCharacterID(), true));
    PyObject* args = new PyObject("util.KeyVal", dict);
    if (is_log_enabled(CLIENT__RSP_DUMP))
        args->Dump(CLIENT__RSP_DUMP, "");
    return args;
}

PyResult CharMgrService::GetPrivateInfo(PyCallArgs& call, PyInt* characterID)
{
    // self.memberinfo = self.charMgr.GetPrivateInfo(self.charID)
    // this is called by corp/editMember
    PyRep* args(m_db.GetCharPrivateInfo(characterID->value()));
    if (is_log_enabled(CLIENT__RSP_DUMP))
        args->Dump(CLIENT__RSP_DUMP, "");
    return args;
}

PyResult CharMgrService::GetPublicInfo(PyCallArgs &call, PyInt* ownerID) {
    //single int arg: char id or corp id
    /*if (IsAgent(args.arg)) {
        //handle agents special right now...
        PyRep *result = m_db.GetAgentPublicInfo(args.arg);
        if (result == nullptr) {
            codelog(CLIENT__ERROR, "%s: Failed to find agent %u", call.client->GetName(), args.arg);
            return nullptr;
        }
        return result;
    }*/

    PyRep *result = m_db.GetCharPublicInfo(ownerID->value());
    if (result == nullptr)
        codelog(CHARACTER__ERROR, "%s: Failed to find char %u", call.client->GetName(), ownerID->value());

    return result;
}

PyResult CharMgrService::AddToBounty(PyCallArgs& call, PyInt* characterID, PyInt* amount) {
    if (call.client->GetCharacterID() == characterID->value()){
        call.client->SendErrorMsg("You cannot put a bounty on yourself.");
        return nullptr;
    }

    if (amount->value() < call.client->GetBalance()) {
        std::string reason = "Placing Bounty on ";
        reason += m_db.GetCharName(characterID->value());
        AccountService::TransferFunds(call.client->GetCharacterID(), corpCONCORD, amount->value(), reason, Journal::EntryType::Bounty, characterID->value());
        m_db.AddBounty(characterID->value(), call.client->GetCharacterID(), amount->value());
        // new system gives target a mail from concord about placement of bounty and char name placing it.
    } else {
        std::map<std::string, PyRep *> res;
        res["amount"] = new PyFloat(amount->value());
        res["balance"] = new PyFloat(call.client->GetBalance());
        throw UserError ("NotEnoughMoney")
                .AddISK ("amount", amount->value())
                .AddISK ("balance", call.client->GetBalance ());
    }

    return PyStatic.NewNone();
}

PyResult CharMgrService::GetCloneTypeID(PyCallArgs& call)
{
	uint32 typeID;
	if (!m_db.GetActiveCloneType(call.client->GetCharacterID(), typeID ) )
	{
		// This should not happen, because a clone is created at char creation.
		// We don't have a clone, so return a basic one. cloneTypeID = 9917 (Clone Grade Delta)
		typeID = 9917;
		sLog.Debug( "CharMgrService", "Returning a basic clone for Char %u of type %u", call.client->GetCharacterID(), typeID );
	}
    return new PyInt(typeID);
}

PyResult CharMgrService::GetHomeStation(PyCallArgs& call)
{
	uint32 stationID = 0;
    if (!CharacterDB::GetCharHomeStation(call.client->GetCharacterID(), stationID) ) {
		sLog.Error( "CharMgrService", "Could't get the home station for Char %u", call.client->GetCharacterID() );
		return PyStatic.NewNone();
	}
    return new PyInt(stationID);
}

PyResult CharMgrService::SetActivityStatus(PyCallArgs& call, PyInt* afk, PyInt* secondsAFK) {
    sLog.Cyan("CharMgrService::SetActivityStatus()", "Player %s(%u) AFK:%s, time:%i.", \
            call.client->GetName(), call.client->GetCharacterID(), (afk->value() ? "true" : "false"), secondsAFK->value());

    if (afk->value()) {
        call.client->SetAFK(true);
    } else {
        call.client->SetAFK(false);
    }

    // call code here to set an AFK watchdog?  config option?
    // returns nothing
    return nullptr;
}

PyResult CharMgrService::GetCharacterDescription(PyCallArgs &call, PyInt* characterID)
{
    sItemFactory.SetUsingClient(call.client);
    CharacterRef c = sItemFactory.GetCharacterRef(characterID->value());
    if (!c ) {
        _log(CHARACTER__ERROR, "GetCharacterDescription failed to load character %u.", characterID->value());
        return nullptr;
    }

    return new PyString(c->description());
}

PyResult CharMgrService::SetCharacterDescription(PyCallArgs &call, PyWString* description)
{
    CharacterRef c = call.client->GetChar();
    if (!c ) {
        _log(CHARACTER__ERROR, "SetCharacterDescription called with no char!");
        return nullptr;
    }
    c->SetDescription(description->content().c_str());

    return nullptr;
}

/** ***********************************************************************
 * @note   these below are partially coded
 */

PyResult CharMgrService::GetSettingsInfo(PyCallArgs& call) {
    /**
     *    def UpdateSettingsStatistics(self):
     *        code, verified = macho.Verify(sm.RemoteSvc('charMgr').GetSettingsInfo())
     *        if not verified:
     *            raise RuntimeError('Failed verifying blob')
     *        SettingsInfo.func_code = marshal.loads(code)
     *        ret = SettingsInfo()
     *        if len(ret) > 0:
     *            sm.RemoteSvc('charMgr').LogSettings(ret)
     */

    // Called in file "carbon/client/script/ui/services/settingsSvc.py"
    // This should return a marshaled python function.
    // It returns a tuple containing a dict that is then sent to
    // charMgr::LogSettings if the tuple has a length greater than zero.
    PyTuple* res = new PyTuple( 2 );
    // This returns an empty tuple
    unsigned char code[] = {
        0x63,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x43,0x00,0x00,
        0x00,0x73,0x0a,0x00,0x00,0x00,0x64,0x01,0x00,0x7d,0x00,0x00,0x7c,0x00,0x00,0x53,
        0x28,0x02,0x00,0x00,0x00,0x4e,0x28,0x00,0x00,0x00,0x00,0x28,0x00,0x00,0x00,0x00,
        0x28,0x01,0x00,0x00,0x00,0x74,0x03,0x00,0x00,0x00,0x74,0x75,0x70,0x28,0x00,0x00,
        0x00,0x00,0x28,0x00,0x00,0x00,0x00,0x73,0x10,0x00,0x00,0x00,0x2e,0x2f,0x6d,0x61,
        0x6b,0x65,0x5a,0x65,0x72,0x6f,0x52,0x65,0x74,0x2e,0x70,0x79,0x74,0x08,0x00,0x00,
        0x00,0x72,0x65,0x74,0x54,0x75,0x70,0x6c,0x65,0x0c,0x00,0x00,0x00,0x73,0x04,0x00,
        0x00,0x00,0x00,0x01,0x06,0x02
    };
    int codeLen = sizeof(code) / sizeof(*code);
    std::string codeString(code, code + codeLen);
    res->items[ 0 ] = new PyString(codeString);

    // error code? 0 = no error
    // if called with any value other than zero the exception output will show 'Verified = False'
    // if called with zero 'Verified = True'
    /*  Just a note
     * due to the client being in placebo mode, the verification code in evecrypto just checks if the signature is 0.
     * when the client is in cryptoapi mode it verifies the signature is signed by CCP's rsa key.
     */

    res->items[ 1 ] = new PyInt( 0 );
    return res;
}

// this takes in the value returned from the function we return in GetSettingsInfo
// based on the captured data, this is used to gather info about game settings the players use
PyResult CharMgrService::LogSettings(PyCallArgs& call, PyRep* settingsInfoRet) {
    /*
     *    [PyTuple 1 items]
     *      [PyTuple 2 items]
     *        [PyInt 0]
     *        [PySubStream 734 bytes]
     *          [PyTuple 4 items]
     *            [PyInt 1]
     *            [PyString "LogSettings"]
     *            [PyTuple 1 items]
     *              [PyDict 36 kvp]
     *                [PyString "locale"]
     *                [PyString "en_US"]
     *                [PyString "shadowquality"]
     *                [PyInt 1]
     *                [PyString "video_vendorid"]
     *                [PyIntegerVar 4318]
     *                [PyString "dronemodelsenabled"]
     *                [PyInt 1]
     *                [PyString "loadstationenv2"]
     *                [PyInt 1]
     *                [PyString "video_deviceid"]
     *                [PyIntegerVar 3619]
     *                [PyString "camerashakesenabled"]
     *                [PyInt 1]
     *                [PyString "interiorshaderquality"]
     *                [PyInt 0]
     *                [PyString "presentation_interval"]
     *                [PyInt 1]
     *                [PyString "windowed_resolution"]
     *                [PyString "1440x900"]
     *                [PyString "explosioneffectssenabled"]
     *                [PyInt 1]
     *                [PyString "uiscalingfullscreen"]
     *                [PyFloat 1]
     *                [PyString "lod"]
     *                [PyInt 1]
     *                [PyString "audioenabled"]
     *                [PyInt 1]
     *                [PyString "voiceenabled"]
     *                [PyInt 1]
     *                [PyString "autodepth_stencilformat"]
     *                [PyInt 75]
     *                [PyString "hdrenabled"]
     *                [PyInt 1]
     *                [PyString "backbuffer_format"]
     *                [PyInt 22]
     *                [PyString "effectssenabled"]
     *                [PyInt 1]
     *                [PyString "breakpadUpload"]
     *                [PyInt 1]
     *                [PyString "windowed"]
     *                [PyBool True]
     *                [PyString "transgaming"]
     *                [PyBool False]
     *                [PyString "missilesenabled"]
     *                [PyInt 1]
     *                [PyString "sunocclusion"]
     *                [PyInt 1]
     *                [PyString "optionalupgrade"]
     *                [PyInt 0]
     *                [PyString "uiscalingwindowed"]
     *                [PyFloat 1]
     *                [PyString "textureqality"]
     *                [PyInt 0]
     *                [PyString "interiorgraphicsquality"]
     *                [PyInt 0]
     *                [PyString "antialiasing"]
     *                [PyInt 2]
     *                [PyString "defaultDockingView"]
     *                [PyString "hangar"]
     *                [PyString "turretsenabled"]
     *                [PyInt 1]
     *                [PyString "advancedcamera"]
     *                [PyInt 0]
     *                [PyString "postprocessing"]
     *                [PyInt 1]
     *                [PyString "fixedwindow"]
     *                [PyBool False]
     *                [PyString "shaderquality"]
     *                [PyInt 3]
     *                [PyString "fullscreen_resolution"]
     *                [PyString "1440x900"]
     *            [PyDict 1 kvp]
     *              [PyString "machoVersion"]
     *              [PyInt 1]
     */
    sLog.Warning( "CharMgrService::Handle_LogSettings()", "size= %lli", call.tuple->size());
    call.Dump(CHARACTER__TRACE);
    return nullptr;
}

//17:09:10 L CharMgrService::Handle_GetNote(): size= 1
PyResult CharMgrService::GetNote(PyCallArgs& call, PyInt* itemID)
{
    uint32 ownerID = call.client->GetCharacterID();

	PyString *str = m_db.GetNote(ownerID, itemID->value());
    if (!str)
        str = new PyString("");

    return str;
}

PyResult CharMgrService::SetNote(PyCallArgs &call, PyInt* itemID, PyString* note)
{
    m_db.SetNote(call.client->GetCharacterID(), itemID->value(), note->content().c_str());

    return PyStatic.NewNone();
}

// Helper: lossy narrow from wstring to std::string (ASCII only)
static inline std::string _NarrowLossy(const std::wstring& w)
{
    std::string out;
    out.reserve(w.size());
    for (wchar_t ch : w) {
        out.push_back(ch < 0x80 ? char(ch) : '?');
    }
    return out;
}

static inline std::string _NarrowLossy(const std::string& s)
{
    // content() from PyWString / PyString in this fork already returns std::string,
    // so we don’t need to convert — just return it as-is.
    return s;
}


// Unified AddOwnerNote that accepts any mix of (PyString / PyWString)
// and figures out which argument is the label ("S:..." or "N:...") and
// which argument is the note body.
PyResult CharMgrService::AddOwnerNote(PyCallArgs& call)
{
    sLog.Warning("CharMgrService::Handle_AddOwnerNote(unified)", "size=%lu",
                 call.tuple ? call.tuple->size() : 0);
    call.Dump(CHARACTER__DEBUG);

    if (call.tuple == nullptr || call.tuple->size() != 2) {
        sLog.Error("CharMgrService",
                   "AddOwnerNote expects exactly 2 args, got %zu",
                   call.tuple ? call.tuple->size() : 0);
        return nullptr;
    }

    PyRep* a0 = call.tuple->GetItem(0);
    PyRep* a1 = call.tuple->GetItem(1);

    // Helper: convert any PyString / PyWString into std::string (UTF-8-ish).
    auto Narrow = [](PyRep* r) -> std::string {
        if (auto* s = dynamic_cast<PyString*>(r))
            return s->content();
        if (auto* w = dynamic_cast<PyWString*>(r))
            return _NarrowLossy(w->content());
        return std::string();
    };

    std::string s0 = Narrow(a0);
    std::string s1 = Narrow(a1);

    // Helper: does this look like a note label? ("S:..." or "N:...")
    auto IsLabel = [](const std::string& s) -> bool {
        return s.size() >= 2 &&
               ( (s[0] == 'S' || s[0] == 'N') && s[1] == ':' );
    };

    std::string label;
    std::string body;

    if (IsLabel(s0)) {
        // First arg is the label (e.g. "S:Folders" or "N:My note title")
        label = s0;
        body  = s1;
    } else if (IsLabel(s1)) {
        // Second arg is the label (e.g. "S:Folders" / "N:...")
        label = s1;
        body  = s0;
    } else {
        // Fallback: no obvious label; keep existing order.
        label = s0;
        body  = s1;
        sLog.Warning("CharMgrService::AddOwnerNote",
                     "Could not detect label prefix, using arg0 as label.");
    }

    uint32 noteID = m_db.AddOwnerNote(call.client->GetCharacterID(), label, body);
    if (noteID == 0)
        return nullptr;

    return new PyInt(noteID);
}


// 1) Client sends a single WString containing "label\nbody"
PyResult CharMgrService::EditOwnerNote(PyCallArgs& call, PyInt* noteID, PyWString* bodyW)
{
    const uint32 charID = call.client->GetCharacterID();
    const uint32 nid    = noteID->value();

    // In this codebase, content() returns std::string
    const std::string full = bodyW->content();

    std::string label;
    std::string body;

    // Split at first '\n': first line = label, rest = body
    size_t pos = full.find('\n');
    if (pos == std::string::npos)
    {
        label = full;
        body.clear();
    }
    else
    {
        label = full.substr(0, pos);

        size_t startBody = pos + 1;
        if (startBody < full.size() && full[startBody] == '\r')
            ++startBody;

        if (startBody < full.size())
            body = full.substr(startBody);
        else
            body.clear();
    }

    if (!m_db.EditOwnerNote(charID, nid, label, body))
    {
        _log(SERVICE__WARNING,
             "EditOwnerNote(body-only): DB update failed for charID=%u, noteID=%u",
             charID, nid);
    }

    return PyStatic.NewNone();
}

// 2) Client sends label + body as WStrings
PyResult CharMgrService::EditOwnerNote(PyCallArgs& call, PyInt* noteID, PyWString* labelW, PyWString* bodyW)
{
    const uint32 charID = call.client->GetCharacterID();
    const uint32 nid    = noteID->value();

    const std::string label = labelW->content();  // narrow already
    const std::string body  = bodyW->content();

    if (!m_db.EditOwnerNote(charID, nid, label, body))
    {
        _log(SERVICE__WARNING,
             "EditOwnerNote(labelW,bodyW): DB update failed for charID=%u, noteID=%u",
             charID, nid);
    }

    return PyStatic.NewNone();
}

// 3) Client sends label as PyString and body as PyWString
PyResult CharMgrService::EditOwnerNote(PyCallArgs& call, PyInt* noteID, PyString* labelS, PyWString* bodyW)
{
    const uint32 charID = call.client->GetCharacterID();
    const uint32 nid    = noteID->value();

    const std::string label = labelS->content();
    const std::string body  = bodyW->content();

    if (!m_db.EditOwnerNote(charID, nid, label, body))
    {
        _log(SERVICE__WARNING,
             "EditOwnerNote(labelS,bodyW): DB update failed for charID=%u, noteID=%u",
             charID, nid);
    }

    return PyStatic.NewNone();
}

// 4) Client sends label as PyWString and body as PyString
PyResult CharMgrService::EditOwnerNote(PyCallArgs& call, PyInt* noteID, PyWString* labelW, PyString* bodyS)
{
    const uint32 charID = call.client->GetCharacterID();
    const uint32 nid    = noteID->value();

    const std::string label = labelW->content();  // OK (content() returns std::string in this fork)
    const std::string body  = bodyS->content();   // OK — PyString::content() is std::string

    if (!m_db.EditOwnerNote(charID, nid, label, body))
    {
        _log(SERVICE__WARNING,
             "EditOwnerNote(labelW,bodyS): DB update failed for charID=%u, noteID=%u",
             charID, nid);
    }

    return PyStatic.NewNone();
}




PyResult CharMgrService::RemoveOwnerNote(PyCallArgs& call, PyInt* noteID)
{
    const uint32 charID = call.client->GetCharacterID();
    const uint32 nid    = noteID->value();

    if (!m_db.RemoveOwnerNote(charID, nid)) {
        _log(SERVICE__WARNING,
             "RemoveOwnerNote(): DB delete failed for charID=%u, noteID=%u", charID, nid);
    }

    return PyStatic.NewNone();
}


// Overload: (PyString, PyWString)
PyResult CharMgrService::AddOwnerNote(PyCallArgs& call, PyString* /*idStr*/, PyWString* /*part*/)
{
    // We ignore the typed parameters and just use call.tuple
    // so both overloads share the same logic.
    return AddOwnerNote(call);
}

// Overload: (PyWString, PyString)
PyResult CharMgrService::AddOwnerNote(PyCallArgs& call, PyWString* /*part*/, PyString* /*idStr*/)
{
    return AddOwnerNote(call);
}

PyResult CharMgrService::GetOwnerNote(PyCallArgs& call, PyInt* noteID)
{
    // Keep it dead simple: ask the DB for this character's specific note.
    uint32 charID = call.client->GetCharacterID();
    uint32 nid    = noteID->value();

    // Directly return the DB result (PyRep*), which is compatible with PyResult.
    return m_db.GetOwnerNote(charID, nid);
}


PyResult CharMgrService::GetOwnerNoteLabels(PyCallArgs &call)
{  /*
        [PyObjectEx Type2]
          [PyTuple 2 items]
            [PyTuple 1 items]
              [PyToken dbutil.CRowset]
            [PyDict 1 kvp]
              [PyString "header"]
              [PyObjectEx Normal]
                [PyTuple 2 items]
                  [PyToken blue.DBRowDescriptor]
                  [PyTuple 1 items]
                    [PyTuple 2 items]
                      [PyTuple 2 items]
                        [PyString "noteID"]
                        [PyInt 3]
                      [PyTuple 2 items]
                        [PyString "label"]
                        [PyInt 130]
          [PyPackedRow 5 bytes]
            ["noteID" => <4424362> [I4]]
            ["label" => <S:Folders> [WStr]]
          [PyPackedRow 5 bytes]
            ["noteID" => <4424363> [I4]]
            ["label" => <N:Ratting - Chaining Rules> [WStr]]
          [PyPackedRow 5 bytes]
            ["noteID" => <4424364> [I4]]
            ["label" => <N:Harbinger Training Schedule> [WStr]]
          [PyPackedRow 5 bytes]
            ["noteID" => <4424365> [I4]]
            ["label" => <N:Damage Types for Races and Rats> [WStr]]
          [PyPackedRow 5 bytes]
            ["noteID" => <5332889> [I4]]
            ["label" => <N:KES Link> [WStr]]
          [PyPackedRow 5 bytes]
            ["noteID" => <5536321> [I4]]
            ["label" => <N:Pelorn's PvP Route> [WStr]]
    [PyNone]
*/
  sLog.Warning( "CharMgrService::Handle_GetOwnerNoteLabels()", "size= %lli", call.tuple->size());
  call.Dump(CHARACTER__DEBUG);

    return m_db.GetOwnerNoteLabels(call.client->GetCharacterID());
}

/**     ***********************************************************************
 * @note   these do absolutely nothing at this time....
 */

//18:07:30 L CharMgrService::Handle_AddContact(): size=1, 0=Integer(2784)
//18:07:35 L CharMgrService::Handle_AddContact(): size=1, 0=Integer(63177)

PyResult CharMgrService::AddContact(PyCallArgs& call, PyInt* characterID, PyInt* standing, PyInt* inWatchlist, PyInt* notify, std::optional<PyString*> note)
{
  sLog.Warning( "CharMgrService::Handle_AddContact()", "size=%lu", call.tuple->size());
  call.Dump(CHARACTER__DEBUG);

    //TODO: Notify char that they have been added as a contact if notify is True

    m_db.AddContact(call.client->GetCharacterID(), characterID->value(), standing->value(), inWatchlist->value());

  return nullptr;
}

PyResult CharMgrService::AddContact(PyCallArgs& call, PyInt* characterID, PyFloat* standing, PyInt* inWatchlist, PyBool* notify, std::optional<PyWString*> note)
{
  sLog.Warning( "CharMgrService::Handle_AddContact()", "size=%lu", call.tuple->size());
  call.Dump(CHARACTER__DEBUG);

    //TODO: Notify char that they have been added as a contact if notify is True

    m_db.AddContact(call.client->GetCharacterID(), characterID->value(), standing->value(), inWatchlist->value());

  return nullptr;
}

PyResult CharMgrService::EditContact(PyCallArgs& call, PyInt* characterID, PyInt* standing, PyInt* inWatchlist, PyInt* notify, std::optional<PyString*> note)
{
  sLog.Warning( "CharMgrService::Handle_EditContact()", "size=%lu", call.tuple->size());
  call.Dump(CHARACTER__DEBUG);

  m_db.UpdateContact(standing->value(), characterID->value(), call.client->GetCharacterID());
  return nullptr;
}

PyResult CharMgrService::EditContact(PyCallArgs& call, PyInt* characterID, PyFloat* standing, PyInt* inWatchlist, PyBool* notify, std::optional<PyWString*> note)
{
  sLog.Warning( "CharMgrService::Handle_EditContact()", "size=%lu", call.tuple->size());
  call.Dump(CHARACTER__DEBUG);

  m_db.UpdateContact(standing->value(), characterID->value(), call.client->GetCharacterID());
  return nullptr;
}

PyResult CharMgrService::CreateLabel(PyCallArgs& call)
{
  sLog.Warning( "CharMgrService::Handle_CreateLabel()", "size=%lu", call.tuple->size());
  call.Dump(CHARACTER__DEBUG);

  return nullptr;
}

PyResult CharMgrService::DeleteContacts(PyCallArgs& call, PyList* contactIDs)
{
  // sm.RemoteSvc('charMgr').DeleteContacts([contactIDs])

  sLog.Warning( "CharMgrService::Handle_DeleteContacts()", "size=%lu", call.tuple->size());
  call.Dump(CHARACTER__DEBUG);

  for (PyList::const_iterator itr = contactIDs->begin(); itr != contactIDs->end(); ++itr) {
      m_db.RemoveContact(PyRep::IntegerValueU32(*itr), call.client->GetCharacterID());
  }

  return nullptr;
}

PyResult CharMgrService::BlockOwners(PyCallArgs& call, PyList* ownerIDs)
{
  //        sm.RemoteSvc('charMgr').BlockOwners([ownerID])
  sLog.Warning( "CharMgrService::Handle_BlockOwners()", "size=%lu", call.tuple->size());
  call.Dump(CHARACTER__DEBUG);

  for (PyList::const_iterator itr = ownerIDs->begin(); itr != ownerIDs->end(); ++itr) {
      m_db.SetBlockContact(PyRep::IntegerValueU32(*itr), call.client->GetCharacterID(), true);
  }

  return nullptr;
}

PyResult CharMgrService::UnblockOwners(PyCallArgs& call, PyList* ownerIDs)
{
  //            sm.RemoteSvc('charMgr').UnblockOwners(blocked)
  sLog.Warning( "CharMgrService::Handle_UnblockOwners()", "size=%lu", call.tuple->size());
  call.Dump(CHARACTER__DEBUG);

  for (PyList::const_iterator itr = ownerIDs->begin(); itr != ownerIDs->end(); ++itr) {
      m_db.SetBlockContact(PyRep::IntegerValueU32(*itr), call.client->GetCharacterID(), false);
  }

  return nullptr;
}

PyResult CharMgrService::EditContactsRelationshipID(PyCallArgs& call, PyList* contactIDs, PyInt* relationshipID)
{
  /*
            sm.RemoteSvc('charMgr').EditContactsRelationshipID(contactIDs, relationshipID)
 */
  sLog.Warning( "CharMgrService::Handle_EditContactsRelationshipID()", "size=%lu", call.tuple->size());
  call.Dump(CHARACTER__DEBUG);

  for (PyList::const_iterator itr = contactIDs->begin(); itr != contactIDs->end(); ++itr) {
      m_db.UpdateContact(relationshipID->value(), PyRep::IntegerValueU32(*itr), call.client->GetCharacterID());
  }

  return nullptr;
}

PyResult CharMgrService::GetFactions(PyCallArgs& call)
{
    sLog.Warning( "CharMgrService::Handle_GetFactions()", "size= %lli", call.tuple->size());
    call.Dump(CHARACTER__TRACE);
    return nullptr;
}

