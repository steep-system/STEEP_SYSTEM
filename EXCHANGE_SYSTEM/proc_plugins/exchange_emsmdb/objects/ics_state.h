#ifndef _H_ICS_STATE_
#define _H_ICS_STATE_
#include "mapi_types.h"
#include "logon_object.h"


enum {
	ICS_STATE_CONTENTS_DOWN,
	ICS_STATE_CONTENTS_UP,
	ICS_STATE_HIERARCHY_DOWN,
	ICS_STATE_HIERARCHY_UP
};

typedef struct _ICS_STATE {
	int type;
	IDSET *pgiven;
	IDSET *pseen;
	IDSET *pseen_fai;
	IDSET *pread;
} ICS_STATE;

ICS_STATE* ics_state_create(LOGON_OBJECT *plogon, int type);

BOOL ics_state_append_idset(ICS_STATE *pstate,
	uint32_t state_property, IDSET *pset);

TPROPVAL_ARRAY* ics_state_serialize(ICS_STATE *pstate);

void ics_state_free(ICS_STATE *pstate);

#endif /* _H_ICS_STATE_ */
