/**
 * @name EffectsDataMgr.h
 *   This file is for retrieving, manipulating and managing effect data
 *   Copyright 2017  EVEmu Team
 *
 * @Author:    Allan
 * @date:      29 January 2017
 *
 */

#ifndef _EVE_FX_PROC_DATAMGR_H__
#define _EVE_FX_PROC_DATAMGR_H__

#include "effects/EffectsData.h"

/*
 * IMPORTANT (Crucible compatibility / branch drift guard):
 * Different parts of this codebase (and older patches) call FxDataMgr using
 * different method names/casing (GetEffect vs GetEffects, GetExpression vs
 * GetExpressions, GetTypeEffect vs GetTypeEffects, IsOffensive vs isOffensive, etc).
 *
 * To avoid ?rename whack-a-mole?, we keep the original canonical methods and add
 * thin wrappers/aliases for alternate spellings.
 */

class FxDataMgr
: public Singleton< FxDataMgr >
{
public:
    FxDataMgr();
    ~FxDataMgr()                                        { /* do nothing here */ }

    int Initialize();
    void Populate();

    // --- Canonical API (original) ---
    bool isWarpSafe(uint16 eID);
    bool isOffensive(uint16 eID);
    bool isAssistance(uint16 eID);

    uint16 GetEffectID(std::string effectName);
    std::string GetEffectGuid(uint16 eID);
    std::string GetEffectName(uint16 eID);

    Effect GetEffect(uint16 eID);
    Operand GetOperand(uint16 oID);
    Expression GetExpression(uint16 eID);

    void GetTypeEffect(uint16 typeID, std::vector< TypeEffects >& typeEffMap);

    float GetFxTime()                                   { return m_time; }
    uint16 GetFxSize()                                  { return m_fxMap.size(); }

    // --- Compatibility aliases (do NOT change behavior) ---
    // Case variants
    bool IsWarpSafe(uint16 eID)                          { return isWarpSafe(eID); }
    bool IsOffensive(uint16 eID)                         { return isOffensive(eID); }
    bool IsAssistance(uint16 eID)                        { return isAssistance(eID); }

    // GUID naming variants
    std::string GetEffectGUID(uint16 eID)                { return GetEffectGuid(eID); }   // caps variant
    std::string GetEffectGuidName(uint16 eID)            { return GetEffectGuid(eID); }   // harmless extra alias

    // Singular/plural drift variants
    // Some branches/past patches used GetEffects/GetExpressions/GetTypeEffects
    Effect GetEffects(uint16 eID)                        { return GetEffect(eID); }
    Expression GetExpressions(uint16 eID)                { return GetExpression(eID); }
    void GetTypeEffects(uint16 typeID, std::vector< TypeEffects >& typeEffMap)
                                                        { GetTypeEffect(typeID, typeEffMap); }

protected:
    void GetOperands(DBQueryResult& res);
    void GetDgmEffects(DBQueryResult& res);
    void GetExpressions(DBQueryResult& res);
    void GetDgmTypeEffects(DBQueryResult &res);

private:
    bool m_loaded;
    float m_time;

    // data maps
    effectMapType m_fxMap;   // k,v of effID, data   -to search by effect

    effectMapType m_effectMap;  //std::map<uint16, Effect>
    std::map<uint16, Operand> m_opMap;
    std::map<uint16, Expression> m_expMap;
    std::map<std::string, uint16> m_effectName;  // k,v of effectID, effectName.  maps all effectIDs to their name.
    std::unordered_multimap<uint16, TypeEffects> m_typeFxMap;  // k,v of typeID, data<effectID, isDefault>
};

#define sFxDataMgr \
( FxDataMgr::get() )

#endif  // _EVE_FX_PROC_DATAMGR_H__
