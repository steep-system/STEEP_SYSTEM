#include "tpropval_array.h"
#include "rop_util.h"
#include "oxvcard.h"
#include "vcard.h"
#include "util.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>


static uint32_t g_kind_imaddress = 0x00008062;
static uint32_t g_kind_categories = 0x00002328;
static uint32_t g_kind_email1address = 0x00008083;
static uint32_t g_kind_email2address = 0x00008093;
static uint32_t g_kind_email3address = 0x000080A3;
static uint32_t g_kind_workaddresscity = 0x00008046;
static uint32_t g_kind_freebusylocation = 0x000080D8;
static uint32_t g_kind_workaddressstate = 0x00008047;
static uint32_t g_kind_workaddressstreet = 0x00008045;
static uint32_t g_kind_contactuserfield1 = 0x0000804F;
static uint32_t g_kind_contactuserfield2 = 0x00008050;
static uint32_t g_kind_contactuserfield3 = 0x00008051;
static uint32_t g_kind_contactuserfield4 = 0x00008052;
static uint32_t g_kind_workaddresscountry = 0x00008049;
static uint32_t g_kind_bcdisplaydefinition = 0x00008040;
static uint32_t g_kind_workaddresspostalcode = 0x00008048;
static uint32_t g_kind_workaddresspostofficebox = 0x0000804A;

static const uint32_t g_n_proptags[] = 
	{PROP_TAG_SURNAME, PROP_TAG_GIVENNAME, PROP_TAG_MIDDLENAME,
	PROP_TAG_DISPLAYNAMEPREFIX, PROP_TAG_GENERATION};
static const uint32_t g_workaddr_proptags[] =
	{0x8000001F, 0x8001001F, 0x8002001F, 0x8003001F, 0x8004001F, 0x8005001F};
static const uint32_t g_homeaddr_proptags[] =
	{PROP_TAG_HOMEADDRESSPOSTOFFICEBOX, PROP_TAG_HOMEADDRESSSTREET,
	PROP_TAG_HOMEADDRESSCITY, PROP_TAG_HOMEADDRESSSTATEORPROVINCE,
	PROP_TAG_HOMEADDRESSPOSTALCODE, PROP_TAG_HOMEADDRESSCOUNTRY};
static const uint32_t g_otheraddr_proptags[] =
	{PROP_TAG_OTHERADDRESSPOSTOFFICEBOX, PROP_TAG_OTHERADDRESSSTREET,
	PROP_TAG_OTHERADDRESSCITY, PROP_TAG_OTHERADDRESSSTATEORPROVINCE,
	PROP_TAG_OTHERADDRESSPOSTALCODE, PROP_TAG_OTHERADDRESSCOUNTRY};
static const uint32_t g_email_proptags[] =
	{0x8006001F, 0x8007001F, 0x8008001F};
static const uint32_t g_im_proptag = 0x8009001F;
static const uint32_t g_categories_proptag = 0x800A101F;
static const uint32_t g_bcd_proptag = 0x800B0102;
static const uint32_t g_ufld_proptags[] = 
	{0x800C001F, 0x800D001F, 0x800E001F, 0x800F001F};
static const uint32_t g_fbl_proptag = 0x8010001F;


static BOOL oxvcard_check_compatible(const VCARD *pvcard)
{
	BOOL b_mailer;
	BOOL b_version;
	DOUBLE_LIST *plist;
	VCARD_LINE *pvline;
	const char *pstring;
	VCARD_VALUE *pvvalue;
	DOUBLE_LIST_NODE *pnode;
	
	b_version = FALSE;
	b_mailer = FALSE;
	plist = (DOUBLE_LIST*)pvcard;
	for (pnode=double_list_get_head(plist); NULL!=pnode;
		pnode=double_list_get_after(plist, pnode)) {
		pvline = (VCARD_LINE*)pnode->pdata;
		if (0 == strcasecmp(pvline->name, "VERSION")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				return FALSE;
			}
			if (0 != strcmp(pstring, "3.0")) {
				return FALSE;
			}
			b_version = TRUE;
		} else if (0 == strcasecmp(pvline->name, "MAILER") ||
			0 == strcasecmp(pvline->name, "PRODID")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				return FALSE;
			}
			if (0 != strcasecmp(pstring, "Microsoft Exchange")) {
				return FALSE;
			}
			b_mailer = TRUE;
		}
	}
	if (FALSE == b_version || FALSE == b_mailer) {
		return FALSE;
	}
	return TRUE;
}

static BOOL oxvcard_get_propids(PROPID_ARRAY *ppropids,
	GET_PROPIDS get_propids)
{
	PROPNAME_ARRAY propnames;
	PROPERTY_NAME propname_buff[17];
	
	
	propnames.count = 0;
	propnames.ppropname = propname_buff;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_workaddresspostofficebox;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_workaddressstreet;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_workaddresscity;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_workaddressstate;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_workaddresspostalcode;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_workaddresscountry;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_email1address;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_email2address;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_email3address;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_imaddress;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PS_PUBLIC_STRINGS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_categories;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PS_PUBLIC_STRINGS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_bcdisplaydefinition;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_contactuserfield1;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_contactuserfield2;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_contactuserfield3;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_contactuserfield4;
	propnames.count ++;
	
	propname_buff[propnames.count].kind = KIND_LID;
	rop_util_get_common_pset(PSETID_ADDRESS,
		&propname_buff[propnames.count].guid);
	propname_buff[propnames.count].plid =
		&g_kind_freebusylocation;
	propnames.count ++;
	
	return get_propids(&propnames, ppropids);
}

MESSAGE_CONTENT* oxvcard_import(
	const VCARD *pvcard, GET_PROPIDS get_propids)
{
	int i;
	int count;
	int tmp_len;
	int ufld_count;
	int mail_count;
	int list_count;
	BINARY tmp_bin;
	uint16_t propid;
	BOOL b_encoding;
	uint32_t proptag;
	struct tm tmp_tm;
	uint8_t tmp_byte;
	size_t decode_len;
	uint32_t tmp_int32;
	uint64_t tmp_int64;
	DOUBLE_LIST *plist;
	VCARD_LINE *pvline;
	const char *pstring;
	char* child_buff[16];
	VCARD_VALUE *pvvalue;
	VCARD_PARAM *pvparam;
	PROPID_ARRAY propids;
	MESSAGE_CONTENT *pmsg;
	BINARY_ARRAY bin_array;
	const char *photo_type;
	TAGGED_PROPVAL propval;
	DOUBLE_LIST_NODE *pnode;
	DOUBLE_LIST_NODE *pnode1;
	DOUBLE_LIST_NODE *pnode2;
	const char *address_type;
	STRING_ARRAY child_strings;
	STRING_ARRAY strings_array;
	ATTACHMENT_LIST *pattachments;
	ATTACHMENT_CONTENT *pattachment;
	char tmp_buff[VCARD_MAX_BUFFER_LEN];
	
	mail_count = 0;
	ufld_count = 0;
	child_strings.count = 0;
	child_strings.ppstr = child_buff;
	if (FALSE == oxvcard_check_compatible(pvcard)) {
		return NULL;
	}
	pmsg = message_content_init();
	if (NULL == pmsg) {
		return NULL;
	}
	propval.proptag = PROP_TAG_MESSAGECLASS;
	propval.pvalue = "IPM.Contact";
	if (FALSE == tpropval_array_set_propval(
		&pmsg->proplist, &propval)) {
		goto IMPORT_FAILURE;
	}
	plist = (DOUBLE_LIST*)pvcard;
	for (pnode=double_list_get_head(plist); NULL!=pnode;
		pnode=double_list_get_after(plist, pnode)) {
		pvline = (VCARD_LINE*)pnode->pdata;
		if (0 == strcasecmp(pvline->name, "FN")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				goto IMPORT_FAILURE;
			}
			propval.proptag = PROP_TAG_DISPLAYNAME;
			propval.pvalue = (void*)pstring;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
			propval.proptag = PROP_TAG_NORMALIZEDSUBJECT;
			propval.pvalue = tmp_buff;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
			propval.proptag = PROP_TAG_CONVERSATIONTOPIC;
			propval.pvalue = tmp_buff;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "N")) {
			count = 0;
			for (pnode1=double_list_get_head(&pvline->value_list);
				NULL!=pnode1; pnode1=double_list_get_after(
				&pvline->value_list, pnode1)) {
				if (count > 4) {
					break;
				}
				pvvalue = (VCARD_VALUE*)pnode1->pdata;
				list_count = double_list_get_nodes_num(&pvvalue->subval_list);
				if (list_count > 1) {
					goto IMPORT_FAILURE;
				}
				pnode2 = double_list_get_head(&pvvalue->subval_list);
				if (NULL != pnode2) {
					if (NULL != pnode2->pdata) {
						propval.proptag = g_n_proptags[count];
						propval.pvalue = pnode2->pdata;
						if (FALSE == tpropval_array_set_propval(
							&pmsg->proplist, &propval)) {
							goto IMPORT_FAILURE;
						}
					}
				}
				count ++;
			}
		} else if (0 == strcasecmp(pvline->name, "NICKNAME")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL != pstring) {
				propval.proptag = PROP_TAG_NICKNAME;
				propval.pvalue = (void*)pstring;
				if (FALSE == tpropval_array_set_propval(
					&pmsg->proplist, &propval)) {
					goto IMPORT_FAILURE;
				}
			}
		} else if (0 == strcasecmp(pvline->name, "PHOTO")) {
			if (NULL != pmsg->children.pattachments) {
				goto IMPORT_FAILURE;
			}
			b_encoding = FALSE;
			photo_type = NULL;
			for (pnode1=double_list_get_head(&pvline->param_list);
				NULL!=pnode1; pnode1=double_list_get_after(
				&pvline->param_list, pnode1)) {
				pvparam = (VCARD_PARAM*)pnode1->pdata;
				if (0 == strcasecmp(pvparam->name, "ENCODING")) {
					if (NULL == pvparam->pparamval_list) {
						goto IMPORT_FAILURE;
					}
					pnode2 = double_list_get_head(pvparam->pparamval_list);
					if (NULL == pnode2 || NULL == pnode2->pdata ||
						(0 != strcasecmp(pnode2->pdata, "b"))) {
						goto IMPORT_FAILURE;
					}
					b_encoding = TRUE;
				} else if (0 == strcasecmp(pvparam->name, "TYPE")) {
					if (NULL == pvparam->pparamval_list) {
						goto IMPORT_FAILURE;
					}
					pnode2 = double_list_get_head(pvparam->pparamval_list);
					if (NULL == pnode2 || NULL == pnode2->pdata) {
						goto IMPORT_FAILURE;
					}
					photo_type = pnode2->pdata;
				}
			}
			if (FALSE == b_encoding || NULL == photo_type) {
				goto IMPORT_FAILURE;
			}
			if (0 != strcasecmp(photo_type, "jpeg") &&
				0 != strcasecmp(photo_type, "jpg") &&
				0 != strcasecmp(photo_type, "bmp") &&
				0 != strcasecmp(photo_type, "gif") &&
				0 != strcasecmp(photo_type, "png")) {
				goto IMPORT_FAILURE;
			}
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				goto IMPORT_FAILURE;
			}
			pattachments = attachment_list_init();
			if (NULL == pattachments) {
				goto IMPORT_FAILURE;
			}
			message_content_set_attachments_internal(pmsg, pattachments);
			pattachment = attachment_content_init();
			if (NULL == pattachment) {
				goto IMPORT_FAILURE;
			}
			if (FALSE == attachment_list_append_internal(
				pattachments, pattachment)) {
				attachment_content_free(pattachment);
				goto IMPORT_FAILURE;
			}
			tmp_len = strlen(pstring);
			if (0 != decode64(pstring, tmp_len, tmp_buff, &decode_len)) {
				goto IMPORT_FAILURE;
			}
			tmp_bin.pb = tmp_buff;
			tmp_bin.cb = decode_len;
			propval.proptag = PROP_TAG_ATTACHDATABINARY;
			propval.pvalue = &tmp_bin;
			if (FALSE == tpropval_array_set_propval(
				&pattachment->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
			propval.proptag = PROP_TAG_ATTACHEXTENSION;
			propval.pvalue = (void*)photo_type;
			if (FALSE == tpropval_array_set_propval(
				&pattachment->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
			sprintf(tmp_buff, "ContactPhoto.%s", photo_type);
			propval.proptag = PROP_TAG_ATTACHLONGFILENAME;
			propval.pvalue = tmp_buff;
			if (FALSE == tpropval_array_set_propval(
				&pattachment->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
			propval.proptag = PROP_TAG_ATTACHMENTCONTACTPHOTO;
			propval.pvalue = &tmp_byte;
			tmp_byte = 1;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "BDAY")) {
			pstring = vcard_get_first_subvalue(pvline);
			memset(&tmp_tm, 0, sizeof(tmp_tm));
			if (NULL != strptime(pstring, "%Y-%m-%d", &tmp_tm)) {
				tmp_int64 = rop_util_unix_to_nttime(
							mktime(&tmp_tm) - timezone);
				propval.proptag = PROP_TAG_BIRTHDAY;
				propval.pvalue = &tmp_int64;
				if (FALSE == tpropval_array_set_propval(
					&pmsg->proplist, &propval)) {
					goto IMPORT_FAILURE;
				}
			}
		} else if (0 == strcasecmp(pvline->name, "ADR")) {
			pnode1 = double_list_get_head(&pvline->param_list);
			if (NULL == pnode1) {
				goto IMPORT_FAILURE;
			}
			pvparam = (VCARD_PARAM*)pnode1->pdata;
			if (0 != strcasecmp(pvparam->name, "TYPE") ||
				NULL == pvparam->pparamval_list) {
				goto IMPORT_FAILURE;
			}
			pnode2 = double_list_get_head(pvparam->pparamval_list);
			if (NULL == pnode2 || NULL == pnode2->pdata ||
				(0 != strcasecmp(pnode2->pdata, "work") &&
				0 != strcasecmp(pnode2->pdata, "home") &&
				0 != strcasecmp(pnode2->pdata, "postal") &&
				0 != strcasecmp(pnode2->pdata, "dom") &&
				0 != strcasecmp(pnode2->pdata, "intl") &&
				0 != strcasecmp(pnode2->pdata, "parcel"))) {
				goto IMPORT_FAILURE;
			}
			address_type = pnode2->pdata;
			count = 0;
			for (pnode1=double_list_get_head(&pvline->value_list);
				NULL!=pnode1; pnode1=double_list_get_after(
				&pvline->value_list, pnode1)) {
				if (count > 5) {
					break;
				}
				pvvalue = (VCARD_VALUE*)pnode1->pdata;
				list_count = double_list_get_nodes_num(&pvvalue->subval_list);
				if (list_count > 1) {
					goto IMPORT_FAILURE;
				}
				pnode2 = double_list_get_head(&pvvalue->subval_list);
				if (NULL != pnode2) {
					if (NULL == pnode2->pdata) {
						continue;
					}
					if (0 == strcasecmp(address_type, "work")) {
						propval.proptag = g_workaddr_proptags[count];
					} else if (0 == strcasecmp(address_type, "home")) {
						propval.proptag = g_homeaddr_proptags[count];
					} else {
						propval.proptag = g_otheraddr_proptags[count];
					}
					propval.pvalue = pnode2->pdata;
					if (FALSE == tpropval_array_set_propval(
						&pmsg->proplist, &propval)) {
						goto IMPORT_FAILURE;
					}
				}
				count ++;
			}
		} else if (0 == strcasecmp(pvline->name, "TEL")) {
			pnode1 = double_list_get_head(&pvline->param_list);
			if (NULL == pnode1) {
				goto IMPORT_FAILURE;
			}
			pvparam = (VCARD_PARAM*)pnode1->pdata;
			if (0 != strcasecmp(pvparam->name, "TYPE") ||
				NULL == pvparam->pparamval_list) {
				goto IMPORT_FAILURE;
			}
			pnode2 = double_list_get_head(pvparam->pparamval_list);
			if (NULL == pnode2 || NULL == pnode2->pdata) {
				goto IMPORT_FAILURE;
			}
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.pvalue = (void*)pstring;
			if (0 == strcasecmp(pnode2->pdata, "home")) {
				pnode1 = double_list_get_after(
					&pvline->param_list, pnode1);
				if (NULL == pnode1) {
					if (NULL != tpropval_array_get_propval(
						&pmsg->proplist, PROP_TAG_HOMETELEPHONENUMBER)) {
						propval.proptag = PROP_TAG_HOME2TELEPHONENUMBER;
					} else {
						propval.proptag = PROP_TAG_HOMETELEPHONENUMBER;
					}
				} else if (0 == strcasecmp(((VCARD_PARAM*)
					pnode1->pdata)->name, "fax")) {
					propval.proptag = PROP_TAG_HOMEFAXNUMBER;
				} else {
					goto IMPORT_FAILURE;
				}
			} else if (0 == strcasecmp(pnode2->pdata, "voice")) {
				propval.proptag = PROP_TAG_OTHERTELEPHONENUMBER;
			} else if (0 == strcasecmp(pnode2->pdata, "work")) {
				pnode1 = double_list_get_after(
					&pvline->param_list, pnode1);
				if (NULL == pnode1) {
					if (NULL != tpropval_array_get_propval(
						&pmsg->proplist, PROP_TAG_BUSINESSTELEPHONENUMBER)) {
						propval.proptag = PROP_TAG_BUSINESS2TELEPHONENUMBER;
					} else {
						propval.proptag = PROP_TAG_BUSINESSTELEPHONENUMBER;
					}
				} else if (0 == strcasecmp(((VCARD_PARAM*)
					pnode1->pdata)->name, "fax")) {
					propval.proptag = PROP_TAG_BUSINESSFAXNUMBER;
				} else {
					goto IMPORT_FAILURE;
				}
			} else if (0 == strcasecmp(pnode2->pdata, "cell")) {
				propval.proptag = PROP_TAG_MOBILETELEPHONENUMBER;
			} else if (0 == strcasecmp(pnode2->pdata, "pager")) {
				propval.proptag = PROP_TAG_PAGERTELEPHONENUMBER;
			} else if (0 == strcasecmp(pnode2->pdata, "car")) {
				propval.proptag = PROP_TAG_CARTELEPHONENUMBER;
			} else if (0 == strcasecmp(pnode2->pdata, "isdn")) {
				propval.proptag = PROP_TAG_ISDNNUMBER;
			} else if (0 == strcasecmp(pnode2->pdata, "pref")) {
				propval.proptag = PROP_TAG_PRIMARYTELEPHONENUMBER;
			} else {
				continue;
			}
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "EMAIL")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.pvalue = (void*)pstring;
			pnode1 = double_list_get_head(&pvline->param_list);
			if (NULL == pnode1) {
				if (mail_count > 2) {
					continue;
				}
				propval.proptag = g_email_proptags[mail_count];
				mail_count ++;
			} else {
				pvparam = (VCARD_PARAM*)pnode1->pdata;
				if (0 != strcasecmp(pvparam->name, "TYPE")) {
					goto IMPORT_FAILURE;
				}
				if (NULL == pvparam->pparamval_list) {
					goto IMPORT_FAILURE;
				}
				pnode2 = double_list_get_head(pvparam->pparamval_list);
				if (NULL == pnode2) {
					goto IMPORT_FAILURE;
				}
				if (0 == strcasecmp(pnode2->pdata, "INTERNET")) {
					if (mail_count > 2) {
						continue;
					}
					propval.proptag = g_email_proptags[mail_count];
					mail_count ++;
				} else {
					continue;
				}
			}
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "TITLE")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.proptag = PROP_TAG_TITLE;
			propval.pvalue = (void*)pstring;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "ROLE")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.proptag = PROP_TAG_PROFESSION;
			propval.pvalue = (void*)pstring;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "ORG")) {
			pnode1 = double_list_get_head(&pvline->value_list);
			if (NULL == pnode1) {
				continue;
			}
			if (NULL != pnode1->pdata) {
				pvvalue = (VCARD_VALUE*)pnode1->pdata;
				pnode2 = double_list_get_head(&pvvalue->subval_list);
				if (NULL != pnode2 && NULL != pnode2->pdata) {
					propval.proptag = PROP_TAG_COMPANYNAME;
					propval.pvalue = pnode2->pdata;
					if (FALSE == tpropval_array_set_propval(
						&pmsg->proplist, &propval)) {
						goto IMPORT_FAILURE;
					}
				}
			}
			pnode1 = double_list_get_after(&pvline->value_list, pnode1);
			if (NULL != pnode1 && NULL != pnode1->pdata) {
				pvvalue = (VCARD_VALUE*)pnode1->pdata;
				pnode2 = double_list_get_head(&pvvalue->subval_list);
				if (NULL != pnode2 && NULL != pnode2->pdata) {
					propval.proptag = PROP_TAG_DEPARTMENTNAME;
					propval.pvalue = pnode2->pdata;
					if (FALSE == tpropval_array_set_propval(
						&pmsg->proplist, &propval)) {
						goto IMPORT_FAILURE;
					}
				}
			}
		} else if (0 == strcasecmp(pvline->name, "CATEGORIS")) {
			pnode1 = double_list_get_head(&pvline->value_list);
			if (NULL == pnode1) {
				continue;
			}
			pvvalue = (VCARD_VALUE*)pnode1->pdata;
			strings_array.count = 0;
			strings_array.ppstr = (char**)tmp_buff;
			for (pnode2=double_list_get_head(&pvvalue->subval_list);
				NULL!=pnode2; pnode2=double_list_get_after(
				&pvvalue->subval_list, pnode2)) {
				if (NULL == pnode2->pdata) {
					continue;
				}
				strings_array.ppstr[strings_array.count] = pnode2->pdata;
				strings_array.count ++;
			}
			if (0 != strings_array.count && strings_array.count < 128) {
				propval.proptag = g_categories_proptag;
				propval.pvalue = &strings_array;
				if (FALSE == tpropval_array_set_propval(
					&pmsg->proplist, &propval)) {
					goto IMPORT_FAILURE;
				}
			}
		} else if (0 == strcasecmp(pvline->name, "NOTE")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.proptag = PROP_TAG_BODY;
			propval.pvalue = (void*)pstring;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "REV")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			memset(&tmp_tm, 0, sizeof(tmp_tm));
			if (NULL != strptime(pstring, "%Y-%m-%dT%H:%M:%S", &tmp_tm)) {
				tmp_int64 = rop_util_unix_to_nttime(
						mktime(&tmp_tm) - timezone);
				propval.proptag = PROP_TAG_LASTMODIFICATIONTIME;
				propval.pvalue = &tmp_int64;
				if (FALSE == tpropval_array_set_propval(
					&pmsg->proplist, &propval)) {
					goto IMPORT_FAILURE;
				}
			}
		} else if (0 == strcasecmp(pvline->name, "URL")) {
			pnode1 = double_list_get_head(&pvline->param_list);
			if (NULL == pnode1) {
				goto IMPORT_FAILURE;
			}
			pvparam = (VCARD_PARAM*)pnode1->pdata;
			if (0 != strcasecmp(pvparam->name, "TYPE") ||
				NULL == pvparam->pparamval_list) {
				goto IMPORT_FAILURE;
			}
			pnode2 = double_list_get_head(pvparam->pparamval_list);
			if (NULL == pnode2 || NULL == pnode2->pdata) {
				goto IMPORT_FAILURE;
			}
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.pvalue = (void*)pstring;
			if (0 == strcasecmp(pnode2->pdata, "home")) {
				propval.proptag = PROP_TAG_PERSONALHOMEPAGE;
			} else if (0 == strcasecmp(pnode2->pdata, "work")) {
				propval.proptag = PROP_TAG_BUSINESSHOMEPAGE;
			} else {
				continue;
			}
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "CLASS")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (0 == strcasecmp(pstring, "PRIVATE")) {
				tmp_int32 = 2;
			} else if (0 == strcasecmp(pstring, "CONFIDENTIAL")) {
				tmp_int32 = 3;
			} else {
				tmp_int32 = 0;
			}
			propval.proptag = PROP_TAG_SENSITIVITY;
			propval.pvalue = &tmp_int32;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "KEY")) {
			pnode1 = double_list_get_head(&pvline->param_list);
			if (NULL == pnode1) {
				goto IMPORT_FAILURE;
			}
			pvparam = (VCARD_PARAM*)pnode1->pdata;
			if (0 != strcasecmp(pvparam->name, "ENCODING") ||
				NULL == pvparam->pparamval_list) {
				goto IMPORT_FAILURE;
			}
			pnode2 = double_list_get_head(pvparam->pparamval_list);
			if (NULL == pnode2 || NULL == pnode2->pdata ||
				(0 != strcasecmp(pnode2->pdata, "b"))) {
				goto IMPORT_FAILURE;
			}
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				goto IMPORT_FAILURE;
			}
			tmp_len = strlen(pstring);
			if (0 != decode64(pstring, tmp_len, tmp_buff, &decode_len)) {
				goto IMPORT_FAILURE;
			}
			propval.proptag = PROP_TAG_USERX509CERTIFICATE;
			propval.pvalue = &bin_array;
			bin_array.count = 1;
			bin_array.pbin = &tmp_bin;
			tmp_bin.pb = tmp_buff;
			tmp_bin.cb = decode_len;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "X-MS-OL-DESIGN")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.proptag = g_bcd_proptag;
			propval.pvalue = &tmp_bin;
			tmp_bin.cb = strlen(pstring);
			tmp_bin.pb = (void*)pstring;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "X-MS-CHILD")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			if (child_strings.count > 16) {
				goto IMPORT_FAILURE;
			}
			child_strings.ppstr[child_strings.count] = (char*)pstring;
			child_strings.count ++;
		} else if (0 == strcasecmp(pvline->name, "X-MS-TEXT")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.pvalue = (void*)pstring;
			if (ufld_count > 3) {
				goto IMPORT_FAILURE;
			}
			propval.proptag = g_ufld_proptags[ufld_count];
			ufld_count ++;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "X-MS-IMADDRESS") ||
			0 == strcasecmp(pvline->name, "X-MS-RM-IMACCOUNT")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.proptag = g_im_proptag;
			propval.pvalue = (void*)pstring;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "X-MS-TEL")) {
			pnode1 = double_list_get_head(&pvline->param_list);
			if (NULL == pnode1) {
				goto IMPORT_FAILURE;
			}
			pvparam = (VCARD_PARAM*)pnode1->pdata;
			if (NULL != pvparam->pparamval_list) {
				goto IMPORT_FAILURE;
			}
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.pvalue = (void*)pstring;
			if (0 == strcasecmp(pvparam->name, "ASSISTANT")) {
				propval.proptag = PROP_TAG_ASSISTANTTELEPHONENUMBER;
			} else if (0 == strcasecmp(pvparam->name, "CALLBACK")) {
				propval.proptag = PROP_TAG_CALLBACKTELEPHONENUMBER;
			} else if (0 == strcasecmp(pvparam->name, "COMPANY")) {
				propval.proptag = PROP_TAG_COMPANYMAINTELEPHONENUMBER;
			} else if (0 != strcasecmp(pvparam->name, "RADIO")) {
				propval.proptag = PROP_TAG_RADIOTELEPHONENUMBER;
			} else if (0 == strcasecmp(pvparam->name, "TTYTTD")) {
				propval.proptag =
					PROP_TAG_TELECOMMUNICATIONSDEVICEFORDEAFTELEPHONENUMBER;
			} else {
				goto IMPORT_FAILURE;
			}
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "X-MS-ANNIVERSARY")) {
			pstring = vcard_get_first_subvalue(pvline);
			memset(&tmp_tm, 0, sizeof(tmp_tm));
			if (NULL != strptime(pstring, "%Y-%m-%d", &tmp_tm)) {
				tmp_int64 = rop_util_unix_to_nttime(
							mktime(&tmp_tm) - timezone);
				propval.proptag = PROP_TAG_WEDDINGANNIVERSARY;
				propval.pvalue = &tmp_int64;
				if (FALSE == tpropval_array_set_propval(
					&pmsg->proplist, &propval)) {
					goto IMPORT_FAILURE;
				}
			}	
		} else if (0 == strcasecmp(pvline->name, "X-MS-SPOUSE")) {
			pnode1 = double_list_get_head(&pvline->param_list);
			if (NULL == pnode1) {
				goto IMPORT_FAILURE;
			}
			pvparam = (VCARD_PARAM*)pnode1->pdata;
			if (0 != strcasecmp(pvparam->name, "N") ||
				NULL != pvparam->pparamval_list) {
				goto IMPORT_FAILURE;
			}
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.proptag = PROP_TAG_SPOUSENAME;
			propval.pvalue = (void*)pstring;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "X-MS-MANAGER")) {
			pnode1 = double_list_get_head(&pvline->param_list);
			if (NULL == pnode1) {
				goto IMPORT_FAILURE;
			}
			pvparam = (VCARD_PARAM*)pnode1->pdata;
			if (0 != strcasecmp(pvparam->name, "N") ||
				NULL != pvparam->pparamval_list) {
				goto IMPORT_FAILURE;
			}
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.proptag = PROP_TAG_MANAGERNAME;
			propval.pvalue = (void*)pstring;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "X-MS-ASSISTANT")) {
			pnode1 = double_list_get_head(&pvline->param_list);
			if (NULL == pnode1) {
				goto IMPORT_FAILURE;
			}
			pvparam = (VCARD_PARAM*)pnode1->pdata;
			if (0 != strcasecmp(pvparam->name, "N") ||
				NULL != pvparam->pparamval_list) {
				goto IMPORT_FAILURE;
			}
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.proptag = PROP_TAG_ASSISTANT;
			propval.pvalue = (void*)pstring;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "FBURL")) {
			pstring = vcard_get_first_subvalue(pvline);
			if (NULL == pstring) {
				continue;
			}
			propval.proptag = g_fbl_proptag;
			propval.pvalue = (void*)pstring;
			if (FALSE == tpropval_array_set_propval(
				&pmsg->proplist, &propval)) {
				goto IMPORT_FAILURE;
			}
		} else if (0 == strcasecmp(pvline->name, "X-MS-INTERESTS")) {
			pnode1 = double_list_get_head(&pvline->value_list);
			if (NULL == pnode1) {
				continue;
			}
			pvvalue = (VCARD_VALUE*)pnode1->pdata;
			strings_array.count = 0;
			strings_array.ppstr = (char**)tmp_buff;
			for (pnode2=double_list_get_head(&pvvalue->subval_list);
				NULL!=pnode2; pnode2=double_list_get_after(
				&pvvalue->subval_list, pnode2)) {
				if (NULL == pnode2->pdata) {
					continue;
				}
				strings_array.ppstr[strings_array.count] = pnode2->pdata;
				strings_array.count ++;
			}
			if (0 != strings_array.count && strings_array.count < 128) {
				propval.proptag = PROP_TAG_HOBBIES;
				propval.pvalue = &strings_array;
				if (FALSE == tpropval_array_set_propval(
					&pmsg->proplist, &propval)) {
					goto IMPORT_FAILURE;
				}
			}
		}
	}
	if (0 != child_strings.count) {
		propval.proptag = PROP_TAG_CHILDRENSNAMES;
		propval.pvalue = &child_strings;
		if (FALSE == tpropval_array_set_propval(
			&pmsg->proplist, &propval)) {
			goto IMPORT_FAILURE;
		}
	}
	for (i=0; i<pmsg->proplist.count; i++) {
		proptag = pmsg->proplist.ppropval[i].proptag;
		propid = proptag >> 16;
		if (propid & 0x8000) {
			break;
		}
	}
	if (i >= pmsg->proplist.count) {
		return pmsg;
	}
	if (FALSE == oxvcard_get_propids(&propids, get_propids)) {
		goto IMPORT_FAILURE;
	}
	for (i=0; i<pmsg->proplist.count; i++) {
		proptag = pmsg->proplist.ppropval[i].proptag;
		propid = proptag >> 16;
		if (0 == (propid & 0x8000)) {
			continue;
		}
		proptag = propids.ppropid[propid - 0x8000];
		proptag <<= 16;
		pmsg->proplist.ppropval[i].proptag &= 0xFFFF;
		pmsg->proplist.ppropval[i].proptag |= proptag;
	}
	return pmsg;
	
IMPORT_FAILURE:
	message_content_free(pmsg);
	return NULL;
}

BOOL oxvcard_export(const MESSAGE_CONTENT *pmsg,
	VCARD *pvcard, GET_PROPIDS get_propids)
{
	int i;
	BINARY *pbin;
	void *pvalue;
	size_t out_len;
	uint16_t propid;
	uint32_t proptag;
	time_t unix_time;
	struct tm tmp_tm;
	VCARD_LINE *pvline;
	VCARD_VALUE *pvvalue;
	VCARD_PARAM *pvparam;
	PROPID_ARRAY propids;
	const char *photo_type;
	ATTACHMENT_CONTENT *pattachment;
	char tmp_buff[VCARD_MAX_BUFFER_LEN];
	const char* tel_types[] =
		{"HOME", "HOME", "VOICE", "WORK", "WORK",
		"CELL", "PAGER", "CAR", "ISDN", "PREF"};
	const char* ms_tel_types[] =
		{"ASSISTANT", "CALLBACK", "COMPANY", "RADIO", "TTYTTD"};
	uint32_t tel_proptags[] =
		{PROP_TAG_HOMETELEPHONENUMBER, PROP_TAG_HOME2TELEPHONENUMBER,
		PROP_TAG_OTHERTELEPHONENUMBER, PROP_TAG_BUSINESSTELEPHONENUMBER,
		PROP_TAG_BUSINESS2TELEPHONENUMBER, PROP_TAG_MOBILETELEPHONENUMBER,
		PROP_TAG_PAGERTELEPHONENUMBER, PROP_TAG_CARTELEPHONENUMBER,
		PROP_TAG_ISDNNUMBER, PROP_TAG_PRIMARYTELEPHONENUMBER};
	uint32_t ms_tel_proptags[] =
		{PROP_TAG_ASSISTANTTELEPHONENUMBER, PROP_TAG_CALLBACKTELEPHONENUMBER,
		PROP_TAG_COMPANYMAINTELEPHONENUMBER, PROP_TAG_RADIOTELEPHONENUMBER,
		PROP_TAG_TELECOMMUNICATIONSDEVICEFORDEAFTELEPHONENUMBER};
	
	
	if (FALSE == oxvcard_get_propids(&propids, get_propids)) {
		return FALSE;
	}
	vcard_init(pvcard);
	pvline = vcard_new_simple_line("PROFILE", "VCARD");
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	
	pvline = vcard_new_simple_line("VERSION", "3.0");
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	
	pvline = vcard_new_simple_line("MAILER", "Microsoft Exchange");
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	
	pvline = vcard_new_simple_line("PRODID", "Microsoft Exchange");
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pmsg->proplist, PROP_TAG_DISPLAYNAME);
	if (NULL == pvalue) {
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pmsg->proplist, PROP_TAG_NORMALIZEDSUBJECT);
	}
	if (NULL != pvalue) {
		pvline = vcard_new_simple_line("FN", pvalue);
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
	}
	
	pvline = vcard_new_line("N");
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	for (i=0; i<5; i++) {
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
							&pmsg->proplist, g_n_proptags[i]);
		if (NULL == pvalue) {
			continue;
		}
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_NICKNAME);
	if (NULL != pvalue) {
		pvline = vcard_new_simple_line("NICKNAME", pvalue);
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
	}
	
	for (i=0; i<3; i++) {
		propid = g_email_proptags[i] >> 16;
		proptag = propids.ppropid[propid - 0x8000] << 16;
		proptag |= g_email_proptags[i] & 0xFFFF;
		pvalue = tpropval_array_get_propval(
			(TPROPVAL_ARRAY*)&pmsg->proplist, proptag);
		if (NULL == pvalue) {
			continue;
		}
		pvline = vcard_new_line("EMAIL");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("TYPE");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(pvparam, "INTERNET")) {
			goto EXPORT_FAILURE;
		}
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
		&pmsg->proplist, PROP_TAG_ATTACHMENTCONTACTPHOTO);
	if (NULL != pvalue && 0 != (uint8_t*)pvalue &&
		NULL != pmsg->children.pattachments) {
		for (i=0; i<pmsg->children.pattachments->count; i++) {
			pattachment = pmsg->children.pattachments->pplist[i];
			pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pattachment->proplist, PROP_TAG_ATTACHEXTENSION);
			if (NULL == pvalue) {
				continue;
			}
			if (0 != strcasecmp(pvalue, "jpeg") &&
				0 != strcasecmp(pvalue, "jpg") &&
				0 != strcasecmp(pvalue, "bmp") &&
				0 != strcasecmp(pvalue, "gif") &&
				0 != strcasecmp(pvalue, "png")) {
				continue;
			}
			photo_type = pvalue;
			pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pattachment->proplist, PROP_TAG_ATTACHDATABINARY);
			if (NULL == pvalue) {
				continue;
			}
			pvline = vcard_new_line("PHOTO");
			if (NULL == pvline) {
				goto EXPORT_FAILURE;
			}
			vcard_append_line(pvcard, pvline);
			pvparam = vcard_new_param("TYPE");
			if (NULL == pvparam) {
				goto EXPORT_FAILURE;
			}
			vcard_append_param(pvline, pvparam);
			if (FALSE == vcard_append_paramval(
				pvparam, photo_type)) {
				goto EXPORT_FAILURE;
			}
			pvparam = vcard_new_param("ENCODING");
			if (NULL == pvparam) {
				goto EXPORT_FAILURE;
			}
			vcard_append_param(pvline, pvparam);
			if (FALSE == vcard_append_paramval(pvparam, "B")) {
				goto EXPORT_FAILURE;
			}
			pvvalue = vcard_new_value();
			if (NULL == pvvalue) {
				goto EXPORT_FAILURE;
			}
			vcard_append_value(pvline, pvvalue);
			if (0 != encode64(((BINARY*)pvalue)->pb, ((BINARY*)pvalue)->cb,
				tmp_buff, VCARD_MAX_BUFFER_LEN - 1, &out_len)) {
				goto EXPORT_FAILURE;
			}
			tmp_buff[out_len] = '\0';
			if (FALSE == vcard_append_subval(pvvalue, tmp_buff)) {
				goto EXPORT_FAILURE;
			}
			break;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
						&pmsg->proplist, PROP_TAG_BODY);
	if (NULL != pvalue) {
		pvline = vcard_new_simple_line("NOTE", pvalue);
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
	}
	
	pvline = vcard_new_line("ORG");
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	pvvalue = vcard_new_value();
	if (NULL == pvvalue) {
		goto EXPORT_FAILURE;
	}
	vcard_append_value(pvline, pvvalue);
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_COMPANYNAME);
	vcard_append_subval(pvvalue, pvalue);
	pvvalue = vcard_new_value();
	if (NULL == pvvalue) {
		goto EXPORT_FAILURE;
	}
	vcard_append_value(pvline, pvvalue);
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pmsg->proplist, PROP_TAG_DEPARTMENTNAME);
	vcard_append_subval(pvvalue, pvalue);
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pmsg->proplist, PROP_TAG_SENSITIVITY);
	if (NULL == pvalue) {
		pvalue = "PUBLIC";
	} else {
		switch (*(uint32_t*)pvalue) {
		case 2:
			pvalue = "PRIVATE";
			break;
		case 3:
			pvalue = "CONFIDENTIAL";
			break;
		default:
			pvalue = "PUBLIC";
			break;
		}
	}
	pvline = vcard_new_simple_line("CLASS", pvalue);
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	
	pvline = vcard_new_line("ADR");
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	pvparam = vcard_new_param("TYPE");
	if (NULL == pvparam) {
		goto EXPORT_FAILURE;
	}
	vcard_append_param(pvline, pvparam);
	if (FALSE == vcard_append_paramval(pvparam, "WORK")) {
		goto EXPORT_FAILURE;
	}
	for (i=0; i<6; i++) {
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		propid = g_workaddr_proptags[i] >> 16;
		proptag = propids.ppropid[propid - 0x8000] << 16;
		proptag |= g_workaddr_proptags[i] & 0xFFFF;
		pvalue = tpropval_array_get_propval(
			(TPROPVAL_ARRAY*)&pmsg->proplist, proptag);
		if (NULL == pvalue) {
			continue;
		}
		vcard_append_subval(pvvalue, pvalue);
	}
	pvvalue = vcard_new_value();
	if (NULL == pvvalue) {
		goto EXPORT_FAILURE;
	}
	vcard_append_value(pvline, pvvalue);
	
	pvline = vcard_new_line("ADR");
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	pvparam = vcard_new_param("TYPE");
	if (NULL == pvparam) {
		goto EXPORT_FAILURE;
	}
	vcard_append_param(pvline, pvparam);
	if (FALSE == vcard_append_paramval(pvparam, "HOME")) {
		goto EXPORT_FAILURE;
	}
	for (i=0; i<6; i++) {
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, g_homeaddr_proptags[i]);
		if (NULL == pvalue) {
			continue;
		}
		vcard_append_subval(pvvalue, pvalue);
	}
	pvvalue = vcard_new_value();
	if (NULL == pvvalue) {
		goto EXPORT_FAILURE;
	}
	vcard_append_value(pvline, pvvalue);
	
	pvline = vcard_new_line("ADR");
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	pvparam = vcard_new_param("TYPE");
	if (NULL == pvparam) {
		goto EXPORT_FAILURE;
	}
	vcard_append_param(pvline, pvparam);
	if (FALSE == vcard_append_paramval(pvparam, "POSTAL")) {
		goto EXPORT_FAILURE;
	}
	for (i=0; i<6; i++) {
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, g_otheraddr_proptags[i]);
		if (NULL == pvalue) {
			continue;
		}
		vcard_append_subval(pvvalue, pvalue);
	}
	pvvalue = vcard_new_value();
	if (NULL == pvvalue) {
		goto EXPORT_FAILURE;
	}
	vcard_append_value(pvline, pvvalue);
	
	for (i=0; i<10; i++) {
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
							&pmsg->proplist, tel_proptags[i]);
		if (NULL == pvalue) {
			continue;
		}
		pvline = vcard_new_line("TEL");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("TYPE");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(pvparam, tel_types[i])) {
			goto EXPORT_FAILURE;
		}
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pmsg->proplist, PROP_TAG_HOMEFAXNUMBER);
	if (NULL != pvalue) {
			pvline = vcard_new_line("TEL");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("TYPE");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(pvparam, "HOME")) {
			goto EXPORT_FAILURE;
		}
		pvparam = vcard_new_param("FAX");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_BUSINESSFAXNUMBER);
	if (NULL != pvalue) {
			pvline = vcard_new_line("TEL");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("TYPE");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(pvparam, "WORK")) {
			goto EXPORT_FAILURE;
		}
		pvparam = vcard_new_param("FAX");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	propid = g_categories_proptag >> 16;
	proptag = propids.ppropid[propid - 0x8000] << 16;
	proptag |= g_categories_proptag & 0xFFFF;
	pvalue = tpropval_array_get_propval(
		(TPROPVAL_ARRAY*)&pmsg->proplist, proptag);
	if (NULL != pvalue) {
		pvline = vcard_new_line("CATEGORIS");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		for (i=0; i<((STRING_ARRAY*)pvalue)->count; i++) {
			if (FALSE == vcard_append_subval(pvvalue,
				((STRING_ARRAY*)pvalue)->ppstr[i])) {
				goto EXPORT_FAILURE;
			}
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_PROFESSION);
	if (NULL != pvalue) {
		pvline = vcard_new_simple_line("ROLE", pvalue);
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pmsg->proplist, PROP_TAG_PERSONALHOMEPAGE);
	if (NULL != pvalue) {
		pvline = vcard_new_line("URL");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("TYPE");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(pvparam, "HOME")) {
			goto EXPORT_FAILURE;
		}
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pmsg->proplist, PROP_TAG_BUSINESSHOMEPAGE);
	if (NULL != pvalue) {
		pvline = vcard_new_line("URL");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("TYPE");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(pvparam, "WORK")) {
			goto EXPORT_FAILURE;
		}
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	propid = g_bcd_proptag >> 16;
	proptag = propids.ppropid[propid - 0x8000] << 16;
	proptag |= g_bcd_proptag & 0xFFFF;
	pvalue = tpropval_array_get_propval(
		(TPROPVAL_ARRAY*)&pmsg->proplist, proptag);
	if (NULL != pvalue) {
		pvline = vcard_new_simple_line("X-MS-OL-DESIGN", pvalue);
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
				&pmsg->proplist, PROP_TAG_CHILDRENSNAMES);
	if (NULL != pvalue) {
		for (i=0; i<((STRING_ARRAY*)pvalue)->count; i++) {
			pvline = vcard_new_simple_line("X-MS-CHILD",
					((STRING_ARRAY*)pvalue)->ppstr[i]);
			if (NULL == pvline) {
				goto EXPORT_FAILURE;
			}
			vcard_append_line(pvcard, pvline);
		}
	}
	
	for (i=0; i<4; i++) {
		propid = g_ufld_proptags[i] >> 16;
		proptag = propids.ppropid[propid - 0x8000] << 16;
		proptag |= g_ufld_proptags[i] & 0xFFFF;
		pvalue = tpropval_array_get_propval(
			(TPROPVAL_ARRAY*)&pmsg->proplist, proptag);
		if (NULL == pvalue) {
			continue;
		}
		pvline = vcard_new_simple_line("X-MS-TEXT", pvalue);
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
	}
	
	for (i=0; i<5; i++) {
		pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, ms_tel_proptags[i]);
		if (NULL == pvalue) {
			continue;
		}
		pvline = vcard_new_line("X-MS-TEL");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("TYPE");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(
			pvparam, ms_tel_types[i])) {
			goto EXPORT_FAILURE;
		}
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_SPOUSENAME);
	if (NULL != pvalue) {
		pvline = vcard_new_line("X-MS-SPOUSE");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("N");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_MANAGERNAME);
	if (NULL != pvalue) {
		pvline = vcard_new_line("X-MS-MANAGER");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("N");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_ASSISTANT);
	if (NULL != pvalue) {
		pvline = vcard_new_line("X-MS-ASSISTANT");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("N");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, pvalue)) {
			goto EXPORT_FAILURE;
		}
	}
	
	propid = g_fbl_proptag >> 16;
	proptag = propids.ppropid[propid - 0x8000] << 16;
	proptag |= g_fbl_proptag & 0xFFFF;
	pvalue = tpropval_array_get_propval(
		(TPROPVAL_ARRAY*)&pmsg->proplist, proptag);
	if (NULL != pvalue) {
		pvline = vcard_new_simple_line("FBURL", pvalue);
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_HOBBIES);
	if (NULL != pvalue) {
		pvline = vcard_new_line("X-MS-INTERESTS");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		for (i=0; i<((STRING_ARRAY*)pvalue)->count; i++) {
			if (FALSE == vcard_append_subval(pvvalue,
				((STRING_ARRAY*)pvalue)->ppstr[i])) {
				goto EXPORT_FAILURE;
			}
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
		&pmsg->proplist, PROP_TAG_USERX509CERTIFICATE);
	if (NULL != pvalue && 0 != ((BINARY_ARRAY*)pvalue)->count) {
		pvline = vcard_new_line("KEY");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("ENCODING");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(pvparam, "B")) {
			goto EXPORT_FAILURE;
		}
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		pbin = ((BINARY_ARRAY*)pvalue)->pbin;
		if (0 != encode64(pbin->pb, pbin->cb, tmp_buff,
			VCARD_MAX_BUFFER_LEN - 1, &out_len)) {
			goto EXPORT_FAILURE;
		}
		tmp_buff[out_len] = '\0';
		if (FALSE == vcard_append_subval(pvvalue, tmp_buff)) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
						&pmsg->proplist, PROP_TAG_TITLE);
	pvline = vcard_new_simple_line("TITLE", pvalue);
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	
	propid = g_im_proptag >> 16;
	proptag = propids.ppropid[propid - 0x8000] << 16;
	proptag |= g_im_proptag & 0xFFFF;
	pvalue = tpropval_array_get_propval(
		(TPROPVAL_ARRAY*)&pmsg->proplist, proptag);
	pvline = vcard_new_simple_line("X-MS-IMADDRESS", pvalue);
	if (NULL == pvline) {
		goto EXPORT_FAILURE;
	}
	vcard_append_line(pvcard, pvline);
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
					&pmsg->proplist, PROP_TAG_BIRTHDAY);
	if (NULL != pvalue) {
		unix_time = rop_util_nttime_to_unix(*(uint64_t*)pvalue);
		gmtime_r(&unix_time, &tmp_tm);
		strftime(tmp_buff, 1024, "%Y-%m-%d", &tmp_tm);
		pvline = vcard_new_line("BDAY");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("VALUE");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(pvparam, "DATE")) {
			goto EXPORT_FAILURE;
		}
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, tmp_buff)) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
		&pmsg->proplist, PROP_TAG_LASTMODIFICATIONTIME);
	if (NULL != pvalue) {
		unix_time = rop_util_nttime_to_unix(*(uint64_t*)pvalue);
		gmtime_r(&unix_time, &tmp_tm);
		strftime(tmp_buff, 1024, "%Y-%m-%dT%H:%M:%S", &tmp_tm);
		pvline = vcard_new_line("REV");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("VALUE");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(pvparam, "DATE-TIME")) {
			goto EXPORT_FAILURE;
		}
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, tmp_buff) ||
			FALSE == vcard_append_subval(pvvalue, "000Z")) {
			goto EXPORT_FAILURE;
		}
	}
	
	pvalue = tpropval_array_get_propval((TPROPVAL_ARRAY*)
			&pmsg->proplist, PROP_TAG_WEDDINGANNIVERSARY);
	if (NULL != pvalue) {
		unix_time = rop_util_nttime_to_unix(*(uint64_t*)pvalue);
		gmtime_r(&unix_time, &tmp_tm);
		strftime(tmp_buff, 1024, "%Y-%m-%d", &tmp_tm);
		pvline = vcard_new_line("X-MS-ANNIVERSARY");
		if (NULL == pvline) {
			goto EXPORT_FAILURE;
		}
		vcard_append_line(pvcard, pvline);
		pvparam = vcard_new_param("VALUE");
		if (NULL == pvparam) {
			goto EXPORT_FAILURE;
		}
		vcard_append_param(pvline, pvparam);
		if (FALSE == vcard_append_paramval(pvparam, "DATE")) {
			goto EXPORT_FAILURE;
		}
		pvvalue = vcard_new_value();
		if (NULL == pvvalue) {
			goto EXPORT_FAILURE;
		}
		vcard_append_value(pvline, pvvalue);
		if (FALSE == vcard_append_subval(pvvalue, tmp_buff)) {
			goto EXPORT_FAILURE;
		}
	}
	
	return TRUE;
	
EXPORT_FAILURE:
	vcard_free(pvcard);
	return FALSE;
}
