#ifndef EVE_CHARACTER_FITTING_MGR_H
#define EVE_CHARACTER_FITTING_MGR_H

#include "services/Service.h"
#include "character/CharacterDB.h"

class PyRep;
class PyDict;
class PyInt;
class PyWString;
class PyObjectEx;

class CharFittingMgr : public Service <CharFittingMgr>
{
public:
    CharFittingMgr();

protected:
    PyResult GetFittings(PyCallArgs& call, PyInt* ownerID);
    PyResult SaveFitting(PyCallArgs& call, PyInt* ownerID, PyObject* fitting);
    PyResult SaveManyFittings(PyCallArgs& call, PyInt* ownerID, PyDict* fittingsToSave);
    PyResult DeleteFitting(PyCallArgs& call, PyInt* ownerID, PyInt* fittingID);
    PyResult UpdateNameAndDescription(PyCallArgs& call, PyInt* fittingID, PyInt* ownerID, PyWString* name, PyWString* description);

    // IMPORTANT: This MUST match the client call signature seen in logs:
    // (PyInt*, PyObjectEx*, PyInt*, PyDict*, PyDict*)
    PyResult FitFitting(PyCallArgs& call,
                        PyInt* ownerID,
                        PyObjectEx* fitting,
                        PyInt* fittingID,
                        PyDict* dronesByType,
                        PyDict* itemTypes);

    CharacterDB m_db;
};

#endif  // EVE_CHARACTER_FITTING_MGR_H



