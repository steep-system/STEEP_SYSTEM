#ifndef _H_ICS_STATE_
#define _H_ICS_STATE_
#include "mapi_types.h"
#include "store_object.h"


typedef struct _ICS_STATE {
	int type;
	IDSET *pgiven;
	IDSET *pseen;
	IDSET *pseen_fai;
	IDSET *pread;
} ICS_STATE;

ICS_STATE* ics_state_create(uint8_t type);

BINARY* ics_state_serialize(ICS_STATE *pstate);

BOOL ics_state_deserialize(ICS_STATE *pstate, const BINARY *pbin);

void ics_state_free(ICS_STATE *pstate);

#endif /* _H_ICS_STATE_ */
