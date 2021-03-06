#ifndef _H_RPC_EXT_
#define _H_RPC_EXT_
#include "ext_pack.h"

#define CALL_ID_LOGON								0x00
#define CALL_ID_UNLOADOBJECT						0x01
#define CALL_ID_OPENENTRY							0x02
#define CALL_ID_OPENSTOREENTRY						0x03
#define CALL_ID_OPENABENTRY							0x04
#define CALL_ID_RESOLVENAME							0x05
#define CALL_ID_GETPERMISSIONS						0x07
#define CALL_ID_MODIFYPERMISSIONS					0x08
#define CALL_ID_MODIFYRULES							0x09
#define CALL_ID_GETABGAL							0x0a
#define CALL_ID_LOADSTORETABLE						0x0b
#define CALL_ID_OPENSTORE							0x0c
#define CALL_ID_OPENPROPFILESEC						0x0d
#define CALL_ID_LOADHIERARCHYTABLE					0x0e
#define CALL_ID_LOADCONTENTTABLE					0x0f
#define CALL_ID_LOADRECIPIENTTABLE					0x10
#define CALL_ID_LOADRULETABLE						0x12
#define CALL_ID_CREATEMESSAGE						0x13
#define CALL_ID_DELETEMESSAGES						0x14
#define CALL_ID_COPYMESSAGES						0x15
#define CALL_ID_SETREADFLAGS						0x16
#define CALL_ID_CREATEFOLDER						0x17
#define CALL_ID_DELETEFOLDER						0x18
#define CALL_ID_EMPTYFOLDER							0x19
#define CALL_ID_COPYFOLDER							0x1a
#define CALL_ID_GETSTOREENTRYID						0x1b
#define CALL_ID_ENTRYIDFROMSOURCEKEY				0x1c
#define CALL_ID_STOREADVISE							0x1d
#define CALL_ID_UNADVISE							0x1e
#define CALL_ID_NOTIFDEQUEUE						0x1f
#define CALL_ID_QUERYROWS							0x20
#define CALL_ID_SETCOLUMNS							0x21
#define CALL_ID_SEEKROW								0x22
#define CALL_ID_SORTTABLE							0x23
#define CALL_ID_GETROWCOUNT							0x24
#define CALL_ID_RESTRICTTABLE						0x25
#define CALL_ID_FINDROW								0x26
#define CALL_ID_CREATEBOOKMARK						0x27
#define CALL_ID_FREEBOOKMARK						0x28
#define CALL_ID_GETRECEIVEFOLDER					0x29
#define CALL_ID_MODIFYRECIPIENTS					0x2a
#define CALL_ID_SUBMITMESSAGE						0x2b
#define CALL_ID_LOADATTACHMENTTABLE					0x2c
#define CALL_ID_OPENATTACHMENT						0x2d
#define CALL_ID_CREATEATTACHMENT					0x2e
#define CALL_ID_DELETEATTACHMENT					0x2f
#define CALL_ID_SETPROPVALS							0x30
#define CALL_ID_GETPROPVALS							0x31
#define CALL_ID_DELETEPROPVALS						0x32
#define CALL_ID_SETMESSAGEREADFLAG					0x33
#define CALL_ID_OPENEMBEDDED						0x34
#define CALL_ID_GETNAMEDPROPIDS						0x35
#define CALL_ID_GETPROPNAMES						0x36
#define CALL_ID_COPYTO								0x37
#define CALL_ID_SAVECHANGES							0x38
#define CALL_ID_HIERARCHYSYNC						0x39
#define CALL_ID_CONTENTSYNC							0x3a
#define CALL_ID_CONFIGSYNC							0x3b
#define CALL_ID_STATESYNC							0x3c
#define CALL_ID_SYNCMESSAGECHANGE					0x3d
#define CALL_ID_SYNCFOLDERCHANGE					0x3e
#define CALL_ID_SYNCREADSTATECHANGES				0x3f
#define CALL_ID_SYNCDELETIONS						0x40
#define CALL_ID_HIERARCHYIMPORT						0x41
#define CALL_ID_CONTENTIMPORT						0x42
#define CALL_ID_CONFIGIMPORT						0x43
#define CALL_ID_STATEIMPORT							0x44
#define CALL_ID_IMPORTMESSAGE						0x45
#define CALL_ID_IMPORTFOLDER						0x46
#define CALL_ID_IMPORTDELETION						0x47
#define CALL_ID_IMPORTREADSTATES					0x48
#define CALL_ID_GETSEARCHCRITERIA					0x49
#define CALL_ID_SETSEARCHCRITERIA					0x4a
#define CALL_ID_MESSAGETORFC822						0x4b
#define CALL_ID_RFC822TOMESSAGE						0x4c
#define CALL_ID_MESSAGETOICAL						0x4d
#define CALL_ID_ICALTOMESSAGE						0x4e
#define CALL_ID_MESSAGETOVCF						0x4f
#define CALL_ID_VCFTOMESSAGE						0x50
#define CALL_ID_UINFO								0x51
#define CALL_ID_CHECKSESSION						0x52
#define CALL_ID_GETUSERAVAILABILITY					0x53
#define CALL_ID_SETPASSWD							0x54
#define CALL_ID_LINKMESSAGE							0x55

typedef struct _REQ_LOGON {
	char *username;
	char *password;
	uint32_t flags;
} REQ_LOGON;

typedef struct _REQ_CHECKSESSION {
	GUID hsession;
} REQ_CHECKSESSION;

typedef struct _REQ_UINFO {
	char *username;
} REQ_UINFO;

typedef struct _REQ_UNLOADOBJECT {
	GUID hsession;
	uint32_t hobject;
} REQ_UNLOADOBJECT;

typedef struct _REQ_OPENENTRY {
	GUID hsession;
	BINARY entryid;
	uint32_t flags;
} REQ_OPENENTRY;

typedef struct _REQ_OPENSTOREENTRY {
	GUID hsession;
	uint32_t hobject;
	BINARY entryid;
	uint32_t flags;
} REQ_OPENSTOREENTRY;

typedef struct _REQ_OPENABENTRY {
	GUID hsession;
	BINARY entryid;
} REQ_OPENABENTRY;

typedef struct _REQ_RESOLVENAME {
	GUID hsession;
	TARRAY_SET *pcond_set;
} REQ_RESOLVENAME;

typedef struct _REQ_GETPERMISSIONS {
	GUID hsession;
	uint32_t hobject;
} REQ_GETPERMISSIONS;

typedef struct _REQ_MODIFYPERMISSIONS {
	GUID hsession;
	uint32_t hfolder;
	PERMISSION_SET *pset;
} REQ_MODIFYPERMISSIONS;

typedef struct _REQ_MODIFYRULES {
	GUID hsession;
	uint32_t hfolder;
	uint32_t flags;
	RULE_LIST *plist;
} REQ_MODIFYRULES;

typedef struct _REQ_GETABGAL {
	GUID hsession;
} REQ_GETABGAL;

typedef struct _REQ_LOADSTORETABLE {
	GUID hsession;
} REQ_LOADSTORETABLE;

typedef struct _REQ_OPENSTORE {
	GUID hsession;
	BINARY entryid;
} REQ_OPENSTORE;

typedef struct _REQ_OPENPROPFILESEC {
	GUID hsession;
	const FLATUID *puid;
} REQ_OPENPROPFILESEC;

typedef struct _REQ_LOADHIERARCHYTABLE {
	GUID hsession;
	uint32_t hfolder;
	uint32_t flags;
} REQ_LOADHIERARCHYTABLE;

typedef struct _REQ_LOADCONTENTTABLE {
	GUID hsession;
	uint32_t hfolder;
	uint32_t flags;
} REQ_LOADCONTENTTABLE;

typedef struct _REQ_LOADRECIPIENTTABLE {
	GUID hsession;
	uint32_t hmessage;
} REQ_LOADRECIPIENTTABLE;

typedef struct _REQ_LOADRULETABLE {
	GUID hsession;
	uint32_t hfolder;
} REQ_LOADRULETABLE;
	
typedef struct _REQ_CREATEMESSAGE {
	GUID hsession;
	uint32_t hfolder;
	uint32_t flags;
} REQ_CREATEMESSAGE;

typedef struct _REQ_DELETEMESSAGES {
	GUID hsession;
	uint32_t hfolder;
	BINARY_ARRAY *pentryids;
	uint32_t flags;
} REQ_DELETEMESSAGES;

typedef struct _REQ_COPYMESSAGES {
	GUID hsession;
	uint32_t hsrcfolder;
	uint32_t hdstfolder;
	BINARY_ARRAY *pentryids;
	uint32_t flags;
} REQ_COPYMESSAGES;

typedef struct _REQ_SETREADFLAGS {
	GUID hsession;
	uint32_t hfolder;
	BINARY_ARRAY *pentryids;
	uint32_t flags;
} REQ_SETREADFLAGS;

typedef struct _REQ_CREATEFOLDER {
	GUID hsession;
	uint32_t hparent_folder;
	uint32_t folder_type;
	char *folder_name;
	char *folder_comment;
	uint32_t flags;
} REQ_CREATEFOLDER;

typedef struct _REQ_DELETEFOLDER {
	GUID hsession;
	uint32_t hparent_folder;
	BINARY entryid;
	uint32_t flags;
} REQ_DELETEFOLDER;

typedef struct _REQ_EMPTYFOLDER {
	GUID hsession;
	uint32_t hfolder;
	uint32_t flags;
} REQ_EMPTYFOLDER;

typedef struct _REQ_COPYFOLDER {
	GUID hsession;
	uint32_t hsrc_folder;
	BINARY entryid;
	uint32_t hdst_folder;
	char *new_name;
	uint32_t flags;
} REQ_COPYFOLDER;

typedef struct _REQ_GETSTOREENTRYID {
	char *mailbox_dn;
} REQ_GETSTOREENTRYID;

typedef struct _REQ_ENTRYIDFROMSOURCEKEY {
	GUID hsession;
	uint32_t hstore;
	BINARY folder_key;
	BINARY *pmessage_key;
} REQ_ENTRYIDFROMSOURCEKEY;

typedef struct _REQ_STOREADVISE {
	GUID hsession;
	uint32_t hstore;
	BINARY *pentryid;
	uint32_t event_mask;
} REQ_STOREADVISE;

typedef struct _REQ_UNADVISE {
	GUID hsession;
	uint32_t hstore;
	uint32_t sub_id;
} REQ_UNADVISE;

typedef struct _REQ_NOTIFDEQUEUE {
	NOTIF_SINK *psink;
	uint32_t timeval;
} REQ_NOTIFDEQUEUE;

typedef struct _REQ_QUERYROWS {
	GUID hsession;
	uint32_t htable;
	uint32_t start;
	uint32_t count;
	RESTRICTION *prestriction;
	PROPTAG_ARRAY *pproptags;
} REQ_QUERYROWS;

typedef struct _REQ_SETCOLUMNS {
	GUID hsession;
	uint32_t htable;
	PROPTAG_ARRAY *pproptags;
	uint32_t flags;
} REQ_SETCOLUMNS;

typedef struct _REQ_SEEKROW {
	GUID hsession;
	uint32_t htable;
	uint32_t bookmark;
	int32_t seek_rows;
} REQ_SEEKROW;

typedef struct _REQ_SORTTABLE {
	GUID hsession;
	uint32_t htable;
	SORTORDER_SET *psortset;
} REQ_SORTTABLE;

typedef struct _REQ_GETROWCOUNT {
	GUID hsession;
	uint32_t htable;
} REQ_GETROWCOUNT;

typedef struct _REQ_RESTRICTTABLE {
	GUID hsession;
	uint32_t htable;
	RESTRICTION *prestriction;
	uint32_t flags;
} REQ_RESTRICTTABLE;

typedef struct _REQ_FINDROW {
	GUID hsession;
	uint32_t htable;
	uint32_t bookmark;
	RESTRICTION *prestriction;
	uint32_t flags;
} REQ_FINDROW;

typedef struct _REQ_CREATEBOOKMARK {
	GUID hsession;
	uint32_t htable;
} REQ_CREATEBOOKMARK;

typedef struct _REQ_FREEBOOKMARK {
	GUID hsession;
	uint32_t htable;
	uint32_t bookmark;
} REQ_FREEBOOKMARK;

typedef struct _REQ_GETRECEIVEFOLDER {
	GUID hsession;
	uint32_t hstore;
	char *pstrclass;
} REQ_GETRECEIVEFOLDER;

typedef struct _REQ_MODIFYRECIPIENTS {
	GUID hsession;
	uint32_t hmessage;
	uint32_t flags;
	TARRAY_SET *prcpt_list;
} REQ_MODIFYRECIPIENTS;

typedef struct _REQ_SUBMITMESSAGE {
	GUID hsession;
	uint32_t hmessage;
} REQ_SUBMITMESSAGE;

typedef struct _REQ_LOADATTACHMENTTABLE {
	GUID hsession;
	uint32_t hmessage;
} REQ_LOADATTACHMENTTABLE;

typedef struct _REQ_OPENATTACHMENT {
	GUID hsession;
	uint32_t hmessage;
	uint32_t attach_id;
} REQ_OPENATTACHMENT;

typedef struct _REQ_CREATEATTACHMENT {
	GUID hsession;
	uint32_t hmessage;
} REQ_CREATEATTACHMENT;

typedef struct _REQ_DELETEATTACHMENT {
	GUID hsession;
	uint32_t hmessage;
	uint32_t attach_id;
} REQ_DELETEATTACHMENT;

typedef struct _REQ_SETPROPVALS {
	GUID hsession;
	uint32_t hobject;
	TPROPVAL_ARRAY *ppropvals;
} REQ_SETPROPVALS;

typedef struct _REQ_GETPROPVALS {
	GUID hsession;
	uint32_t hobject;
	PROPTAG_ARRAY *pproptags;
} REQ_GETPROPVALS;

typedef struct _REQ_DELETEPROPVALS {
	GUID hsession;
	uint32_t hobject;
	PROPTAG_ARRAY *pproptags;
} REQ_DELETEPROPVALS;

typedef struct _REQ_SETMESSAGEREADFLAG {
	GUID hsession;
	uint32_t hmessage;
	uint32_t flags;
} REQ_SETMESSAGEREADFLAG;

typedef struct _REQ_OPENEMBEDDED {
	GUID hsession;
	uint32_t hattachment;
	uint32_t flags;
} REQ_OPENEMBEDDED;

typedef struct _REQ_GETNAMEDPROPIDS {
	GUID hsession;
	uint32_t hstore;
	PROPNAME_ARRAY *ppropnames;
} REQ_GETNAMEDPROPIDS;

typedef struct _REQ_GETPROPNAMES {
	GUID hsession;
	uint32_t hstore;
	PROPID_ARRAY *ppropids;
} REQ_GETPROPNAMES;

typedef struct _REQ_COPYTO {
	GUID hsession;
	uint32_t hsrcobject;
	PROPTAG_ARRAY *pexclude_proptags;
	uint32_t hdstobject;
	uint32_t flags;
} REQ_COPYTO;

typedef struct _REQ_SAVECHANGES {
	GUID hsession;
	uint32_t hobject;
} REQ_SAVECHANGES;

typedef struct _REQ_HIERARCHYSYNC {
	GUID hsession;
	uint32_t hfolder;
} REQ_HIERARCHYSYNC;

typedef struct _REQ_CONTENTSYNC {
	GUID hsession;
	uint32_t hfolder;
} REQ_CONTENTSYNC;

typedef struct _REQ_CONFIGSYNC {
	GUID hsession;
	uint32_t hctx;
	uint32_t flags;
	BINARY *pstate;
	RESTRICTION *prestriction;
} REQ_CONFIGSYNC;

typedef struct _REQ_STATESYNC {
	GUID hsession;
	uint32_t hctx;
} REQ_STATESYNC;

typedef struct _REQ_SYNCMESSAGECHANGE {
	GUID hsession;
	uint32_t hctx;
} REQ_SYNCMESSAGECHANGE;

typedef struct _REQ_SYNCFOLDERCHANGE {
	GUID hsession;
	uint32_t hctx;
} REQ_SYNCFOLDERCHANGE;

typedef struct _REQ_SYNCREADSTATECHANGES {
	GUID hsession;
	uint32_t hctx;
} REQ_SYNCREADSTATECHANGES;

typedef struct _REQ_SYNCDELETIONS {
	GUID hsession;
	uint32_t hctx;
	uint32_t flags;
} REQ_SYNCDELETIONS;

typedef struct _REQ_HIERARCHYIMPORT {
	GUID hsession;
	uint32_t hfolder;
} REQ_HIERARCHYIMPORT;

typedef struct _REQ_CONTENTIMPORT {
	GUID hsession;
	uint32_t hfolder;
} REQ_CONTENTIMPORT;

typedef struct _REQ_CONFIGIMPORT {
	GUID hsession;
	uint32_t hctx;
	uint8_t sync_type;
	BINARY *pstate;
} REQ_CONFIGIMPORT;
	
typedef struct _REQ_STATEIMPORT {
	GUID hsession;
	uint32_t hctx;
} REQ_STATEIMPORT;

typedef struct _REQ_IMPORTMESSAGE {
	GUID hsession;
	uint32_t hctx;
	uint32_t flags;
	TPROPVAL_ARRAY *pproplist;
} REQ_IMPORTMESSAGE;
	
typedef struct _REQ_IMPORTFOLDER {
	GUID hsession;
	uint32_t hctx;
	TPROPVAL_ARRAY *pproplist;
} REQ_IMPORTFOLDER;

typedef struct _REQ_IMPORTDELETION {
	GUID hsession;
	uint32_t hctx;
	uint32_t flags;
	BINARY_ARRAY *pbins;
} REQ_IMPORTDELETION;

typedef struct _REQ_IMPORTREADSTATES {
	GUID hsession;
	uint32_t hctx;
	STATE_ARRAY *pstates;
} REQ_IMPORTREADSTATES;

typedef struct _REQ_GETSEARCHCRITERIA {
	GUID hsession;
	uint32_t hfolder;
} REQ_GETSEARCHCRITERIA;
	
typedef struct _REQ_SETSEARCHCRITERIA {
	GUID hsession;
	uint32_t hfolder;
	uint32_t flags;
	BINARY_ARRAY *pfolder_array;
	RESTRICTION *prestriction;
} REQ_SETSEARCHCRITERIA;

typedef struct _REQ_MESSAGETORFC822 {
	GUID hsession;
	uint32_t hmessage;
} REQ_MESSAGETORFC822;

typedef struct _REQ_RFC822TOMESSAGE {
	GUID hsession;
	uint32_t hmessage;
	BINARY *peml_bin;
} REQ_RFC822TOMESSAGE;

typedef struct _REQ_MESSAGETOICAL {
	GUID hsession;
	uint32_t hmessage;
} REQ_MESSAGETOICAL;

typedef struct _REQ_ICALTOMESSAGE {
	GUID hsession;
	uint32_t hmessage;
	BINARY *pical_bin;
} REQ_ICALTOMESSAGE;

typedef struct _REQ_MESSAGETOVCF {
	GUID hsession;
	uint32_t hmessage;
} REQ_MESSAGETOVCF;

typedef struct _REQ_VCFTOMESSAGE {
	GUID hsession;
	uint32_t hmessage;
	BINARY *pvcf_bin;
} REQ_VCFTOMESSAGE;

typedef struct _REQ_SAVESESSION {
	GUID hsession;
} REQ_SAVESESSION;

typedef struct _REQ_RESTORESESSION {
	BINARY *pdata_bin;
} REQ_RESTORESESSION;

typedef struct _REQ_GETUSERAVAILABILITY {
	GUID hsession;
	BINARY entryid;
	uint64_t starttime;
	uint64_t endtime;
} REQ_GETUSERAVAILABILITY;

typedef struct _REQ_SETPASSWD {
	char *username;
	char *passwd;
	char *new_passwd;
} REQ_SETPASSWD;

typedef struct _REQ_LINKMESSAGE {
	GUID hsession;
	BINARY search_entryid;
	BINARY message_entryid;
} REQ_LINKMESSAGE;

typedef union _REQUEST_PAYLOAD {
	REQ_LOGON logon;
	REQ_CHECKSESSION checksession;
	REQ_UINFO uinfo;
	REQ_UNLOADOBJECT unloadobject;
	REQ_OPENENTRY openentry;
	REQ_OPENSTOREENTRY openstoreentry;
	REQ_OPENABENTRY openabentry;
	REQ_RESOLVENAME resolvename;
	REQ_GETPERMISSIONS getpermissions;
	REQ_MODIFYPERMISSIONS modifypermissions;
	REQ_MODIFYRULES modifyrules;
	REQ_GETABGAL getabgal;
	REQ_LOADSTORETABLE loadstoretable;
	REQ_OPENSTORE openstore;
	REQ_OPENPROPFILESEC openpropfilesec;
	REQ_LOADHIERARCHYTABLE loadhierarchytable;
	REQ_LOADCONTENTTABLE loadcontenttable;
	REQ_LOADRECIPIENTTABLE loadrecipienttable;
	REQ_LOADRULETABLE loadruletable;
	REQ_CREATEMESSAGE createmessage;
	REQ_DELETEMESSAGES deletemessages;
	REQ_COPYMESSAGES copymessages;
	REQ_SETREADFLAGS setreadflags;
	REQ_CREATEFOLDER createfolder;
	REQ_DELETEFOLDER deletefolder;
	REQ_EMPTYFOLDER emptyfolder;
	REQ_COPYFOLDER copyfolder;
	REQ_GETSTOREENTRYID getstoreentryid;
	REQ_ENTRYIDFROMSOURCEKEY entryidfromsourcekey;
	REQ_STOREADVISE storeadvise;
	REQ_UNADVISE unadvise;
	REQ_NOTIFDEQUEUE notifdequeue;
	REQ_QUERYROWS queryrows;
	REQ_SETCOLUMNS setcolumns;
	REQ_SEEKROW seekrow;
	REQ_SORTTABLE sorttable;
	REQ_GETROWCOUNT getrowcount;
	REQ_RESTRICTTABLE restricttable;
	REQ_FINDROW findrow;
	REQ_CREATEBOOKMARK createbookmark;
	REQ_FREEBOOKMARK freebookmark;
	REQ_GETRECEIVEFOLDER getreceivefolder;
	REQ_MODIFYRECIPIENTS modifyrecipients;
	REQ_SUBMITMESSAGE submitmessage;
	REQ_LOADATTACHMENTTABLE loadattachmenttable;
	REQ_OPENATTACHMENT openattachment;
	REQ_CREATEATTACHMENT createattachment;
	REQ_DELETEATTACHMENT deleteattachment;
	REQ_SETPROPVALS setpropvals;
	REQ_GETPROPVALS getpropvals;
	REQ_DELETEPROPVALS deletepropvals;
	REQ_SETMESSAGEREADFLAG setmessagereadflag;
	REQ_OPENEMBEDDED openembedded;
	REQ_GETNAMEDPROPIDS getnamedpropids;
	REQ_GETPROPNAMES getpropnames;
	REQ_COPYTO copyto;
	REQ_SAVECHANGES savechanges;
	REQ_HIERARCHYSYNC hierarchysync;
	REQ_CONTENTSYNC contentsync;
	REQ_CONFIGSYNC configsync;
	REQ_STATESYNC statesync;
	REQ_SYNCMESSAGECHANGE syncmessagechange;
	REQ_SYNCFOLDERCHANGE syncfolderchange;
	REQ_SYNCREADSTATECHANGES syncreadstatechanges;
	REQ_SYNCDELETIONS syncdeletions;
	REQ_HIERARCHYIMPORT hierarchyimport;
	REQ_CONTENTIMPORT contentimport;
	REQ_CONFIGIMPORT configimport;
	REQ_STATEIMPORT stateimport;
	REQ_IMPORTMESSAGE importmessage;
	REQ_IMPORTFOLDER importfolder;
	REQ_IMPORTDELETION importdeletion;
	REQ_IMPORTREADSTATES importreadstates;
	REQ_GETSEARCHCRITERIA getsearchcriteria;
	REQ_SETSEARCHCRITERIA setsearchcriteria;
	REQ_MESSAGETORFC822 messagetorfc822;
	REQ_RFC822TOMESSAGE rfc822tomessage;
	REQ_MESSAGETOICAL messagetoical;
	REQ_ICALTOMESSAGE icaltomessage;
	REQ_MESSAGETOVCF messagetovcf;
	REQ_VCFTOMESSAGE vcftomessage;
	REQ_GETUSERAVAILABILITY getuseravailability;
	REQ_SETPASSWD setpasswd;
	REQ_LINKMESSAGE linkmessage;
} REQUEST_PAYLOAD;

typedef struct _RPC_REQUEST {
	uint8_t call_id;
	REQUEST_PAYLOAD payload;
} RPC_REQUEST;

typedef struct _RESP_LOGON {
	GUID hsession;
} RESP_LOGON;

typedef struct _RESP_UINFO {
	BINARY entryid;
	char *pdisplay_name;
	char *px500dn;
	uint32_t privilege_bits;
} RESP_UINFO;

typedef struct _RESP_OPENENTRY {
	uint8_t mapi_type;
	uint32_t hobject;
} RESP_OPENENTRY;

typedef struct _RESP_OPENSTOREENTRY {
	uint8_t mapi_type;
	uint32_t hobject;
} RESP_OPENSTOREENTRY;

typedef struct _RESP_OPENABENTRY {
	uint8_t mapi_type;
	uint32_t hobject;
} RESP_OPENABENTRY;

typedef struct _RESP_RESOLVENAME {
	TARRAY_SET result_set;
} RESP_RESOLVENAME;

typedef struct _RESP_GETPERMISSIONS {
	PERMISSION_SET perm_set;
} RESP_GETPERMISSIONS;

typedef struct _RESP_GETABGAL {
	BINARY entryid;
} RESP_GETABGAL;

typedef struct _RESP_LOADSTORETABLE {
	uint32_t hobject;
} RESP_LOADSTORETABLE;

typedef struct _RESP_OPENSTORE {
	uint32_t hobject;
} RESP_OPENSTORE;

typedef struct _RESP_OPENPROPFILESEC {
	uint32_t hobject;
} RESP_OPENPROPFILESEC;

typedef struct _RESP_LOADHIERARCHYTABLE {
	uint32_t hobject;
} RESP_LOADHIERARCHYTABLE;

typedef struct _RESP_LOADCONTENTTABLE {
	uint32_t hobject;
} RESP_LOADCONTENTTABLE;

typedef struct _RESP_LOADRECIPIENTTABLE {
	uint32_t hobject;
} RESP_LOADRECIPIENTTABLE;

typedef struct _RESP_LOADRULETABLE {
	uint32_t hobject;
} RESP_LOADRULETABLE;
	
typedef struct _RESP_CREATEMESSAGE {
	uint32_t hobject;
} RESP_CREATEMESSAGE;

typedef struct _RESP_CREATEFOLDER {
	uint32_t hobject;
} RESP_CREATEFOLDER;

typedef struct _RESP_GETSTOREENTRYID {
	BINARY entryid;
} RESP_GETSTOREENTRYID;

typedef struct _RESP_ENTRYIDFROMSOURCEKEY {
	BINARY entryid;
} RESP_ENTRYIDFROMSOURCEKEY;

typedef struct _RESP_STOREADVISE {
	uint32_t sub_id;
} RESP_STOREADVISE;

typedef struct _RESP_NOTIFDEQUEUE {
	ZNOTIFICATION_ARRAY notifications;
} RESP_NOTIFDEQUEUE;

typedef struct _RESP_QUERYROWS {
	TARRAY_SET rowset;
} RESP_QUERYROWS;

typedef struct _RESP_SETCOLUMNS {
	GUID hsession;
	uint32_t htable;
	PROPTAG_ARRAY *pproptags;
	uint32_t flags;
} RESP_SETCOLUMNS;

typedef struct _RESP_SEEKROW {
	int32_t sought_rows;
} RESP_SEEKROW;

typedef struct _RESP_GETROWCOUNT {
	uint32_t count;
} RESP_GETROWCOUNT;

typedef struct _RESP_FINDROW {
	uint32_t row_idx;
} RESP_FINDROW;

typedef struct _RESP_CREATEBOOKMARK {
	uint32_t bookmark;
} RESP_CREATEBOOKMARK;

typedef struct _RESP_GETRECEIVEFOLDER {
	BINARY entryid;
} RESP_GETRECEIVEFOLDER;

typedef struct _RESP_LOADATTACHMENTTABLE {
	uint32_t hobject;
} RESP_LOADATTACHMENTTABLE;

typedef struct _RESP_OPENATTACHMENT {
	uint32_t hobject;
} RESP_OPENATTACHMENT;

typedef struct _RESP_CREATEATTACHMENT {
	uint32_t hobject;
} RESP_CREATEATTACHMENT;

typedef struct _RESP_GETPROPVALS {
	TPROPVAL_ARRAY propvals;
} RESP_GETPROPVALS;

typedef struct _RESP_OPENEMBEDDED {
	uint32_t hobject;
} RESP_OPENEMBEDDED;

typedef struct _RESP_GETNAMEDPROPIDS {
	PROPID_ARRAY propids;
} RESP_GETNAMEDPROPIDS;

typedef struct _RESP_GETPROPNAMES {
	PROPNAME_ARRAY propnames;
} RESP_GETPROPNAMES;

typedef struct _RESP_HIERARCHYSYNC {
	uint32_t hobject;
} RESP_HIERARCHYSYNC;

typedef struct _RESP_CONTENTSYNC {
	uint32_t hobject;
} RESP_CONTENTSYNC;

typedef struct _RESP_CONFIGSYNC {
	uint8_t b_changed;
	uint32_t count;
} RESP_CONFIGSYNC;

typedef struct _RESP_STATESYNC {
	BINARY state;
} RESP_STATESYNC;

typedef struct _RESP_SYNCMESSAGECHANGE {
	zend_bool b_new;
	TPROPVAL_ARRAY proplist;
} RESP_SYNCMESSAGECHANGE;

typedef struct _RESP_SYNCFOLDERCHANGE {
	TPROPVAL_ARRAY proplist;
} RESP_SYNCFOLDERCHANGE;

typedef struct _RESP_SYNCREADSTATECHANGES {
	STATE_ARRAY states;
} RESP_SYNCREADSTATECHANGES;

typedef struct _RESP_SYNCDELETIONS {
	BINARY_ARRAY bins;
} RESP_SYNCDELETIONS;

typedef struct _RESP_HIERARCHYIMPORT {
	uint32_t hobject;
} RESP_HIERARCHYIMPORT;

typedef struct _RESP_CONTENTIMPORT {
	uint32_t hobject;
} RESP_CONTENTIMPORT;
	
typedef struct _RESP_STATEIMPORT {
	BINARY state;
} RESP_STATEIMPORT;

typedef struct _RESP_IMPORTMESSAGE {
	uint32_t hobject;
} RESP_IMPORTMESSAGE;

typedef struct _RESP_GETSEARCHCRITERIA {
	BINARY_ARRAY folder_array;
	RESTRICTION *prestriction;
	uint32_t search_stat;
} RESP_GETSEARCHCRITERIA;

typedef struct _RESP_MESSAGETORFC822 {
	BINARY eml_bin;
} RESP_MESSAGETORFC822;

typedef struct _RESP_MESSAGETOICAL {
	BINARY ical_bin;
} RESP_MESSAGETOICAL;

typedef struct _RESP_MESSAGETOVCF {
	BINARY vcf_bin;
} RESP_MESSAGETOVCF;

typedef struct _RESP_SAVESESSION {
	BINARY data_bin;
} RESP_SAVESESSION;

typedef struct _RESP_RESTORESESSION {
	GUID hsession;
} RESP_RESTORESESSION;

typedef struct _RESP_GETUSERAVAILABILITY {
	char *result_string;
} RESP_GETUSERAVAILABILITY;

typedef union _RESPONSE_PAYLOAD {
	RESP_LOGON logon;
	RESP_UINFO uinfo;
	RESP_OPENENTRY openentry;
	RESP_OPENSTOREENTRY openstoreentry;
	RESP_OPENABENTRY openabentry;
	RESP_RESOLVENAME resolvename;
	RESP_GETPERMISSIONS getpermissions;
	RESP_GETABGAL getabgal;
	RESP_LOADSTORETABLE loadstoretable;
	RESP_OPENSTORE openstore;
	RESP_OPENPROPFILESEC openpropfilesec;
	RESP_LOADHIERARCHYTABLE loadhierarchytable;
	RESP_LOADCONTENTTABLE loadcontenttable;
	RESP_LOADRECIPIENTTABLE loadrecipienttable;
	RESP_LOADRULETABLE loadruletable;
	RESP_CREATEMESSAGE createmessage;
	RESP_CREATEFOLDER createfolder;
	RESP_GETSTOREENTRYID getstoreentryid;
	RESP_ENTRYIDFROMSOURCEKEY entryidfromsourcekey;
	RESP_STOREADVISE storeadvise;
	RESP_NOTIFDEQUEUE notifdequeue;
	RESP_QUERYROWS queryrows;
	RESP_SEEKROW seekrow;
	RESP_GETROWCOUNT getrowcount;
	RESP_FINDROW findrow;
	RESP_CREATEBOOKMARK createbookmark;
	RESP_GETRECEIVEFOLDER getreceivefolder;
	RESP_LOADATTACHMENTTABLE loadattachmenttable;
	RESP_OPENATTACHMENT openattachment;
	RESP_CREATEATTACHMENT createattachment;
	RESP_GETPROPVALS getpropvals;
	RESP_OPENEMBEDDED openembedded;
	RESP_GETNAMEDPROPIDS getnamedpropids;
	RESP_GETPROPNAMES getpropnames;
	RESP_HIERARCHYSYNC hierarchysync;
	RESP_CONTENTSYNC contentsync;
	RESP_CONFIGSYNC configsync;
	RESP_STATESYNC statesync;
	RESP_SYNCMESSAGECHANGE syncmessagechange;
	RESP_SYNCFOLDERCHANGE syncfolderchange;
	RESP_SYNCREADSTATECHANGES syncreadstatechanges;
	RESP_SYNCDELETIONS syncdeletions;
	RESP_HIERARCHYIMPORT hierarchyimport;
	RESP_CONTENTIMPORT contentimport;
	RESP_STATEIMPORT stateimport;
	RESP_IMPORTMESSAGE importmessage;
	RESP_GETSEARCHCRITERIA getsearchcriteria;
	RESP_MESSAGETORFC822 messagetorfc822;
	RESP_MESSAGETOICAL messagetoical;
	RESP_MESSAGETOVCF messagetovcf;
	RESP_GETUSERAVAILABILITY getuseravailability;
} RESPONSE_PAYLOAD;

typedef struct _RPC_RESPONSE {
	uint8_t call_id;
	uint32_t result;
	RESPONSE_PAYLOAD payload;
} RPC_RESPONSE;

zend_bool rpc_ext_push_request(const RPC_REQUEST *prequest,
	BINARY *pbin_out);

zend_bool rpc_ext_pull_response(const BINARY *pbin_in,
	RPC_RESPONSE *presponse);

#endif /* _H_RPC_EXT_ */
