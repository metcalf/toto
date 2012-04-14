/*
 * rlm_totp.c
 *
 * Version:  $Id$
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Copyright 2001,2006  The FreeRADIUS server project
 * Copyright 2001  Kostas Kalevras <kkalev@noc.ntua.gr>
 */

#include <freeradius-devel/ident.h>
RCSID("$Id$")

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>

#include <ctype.h>

#include "../../include/md5.h"
#include "../../include/sha1.h"
//#include <oath.h>


/*
 *	Decode one base64 chunk
 */
static int decode_it(const char *src, uint8_t *dst)
{
	int i;
	unsigned int x = 0;

	for(i = 0; i < 4; i++) {
		if (src[i] >= 'A' && src[i] <= 'Z')
			x = (x << 6) + (unsigned int)(src[i] - 'A' + 0);
		else if (src[i] >= 'a' && src[i] <= 'z')
			 x = (x << 6) + (unsigned int)(src[i] - 'a' + 26);
		else if(src[i] >= '0' && src[i] <= '9')
			 x = (x << 6) + (unsigned int)(src[i] - '0' + 52);
		else if(src[i] == '+')
			x = (x << 6) + 62;
		else if (src[i] == '/')
			x = (x << 6) + 63;
		else if (src[i] == '=')
			x = (x << 6);
		else return 0;
	}

	dst[2] = (unsigned char)(x & 255); x >>= 8;
	dst[1] = (unsigned char)(x & 255); x >>= 8;
	dst[0] = (unsigned char)(x & 255);

	return 1;
}


/*
 *	Base64 decoding.
 */
static int base64_decode (const char *src, uint8_t *dst)
{
	int length, equals;
	int i, num;
	uint8_t last[3];

	length = equals = 0;
	while (src[length] && src[length] != '=') length++;

	while (src[length + equals] == '=') equals++;

	num = (length + equals) / 4;

	for (i = 0; i < num - 1; i++) {
		if (!decode_it(src, dst)) return 0;
		src += 4;
		dst += 3;
	}

	decode_it(src, last);
	for (i = 0; i < (3 - equals); i++) {
		dst[i] = last[i];
	}

	return (num * 3) - equals;
}

/*
 *      Define a structure for our module configuration.
 *
 *      These variables do not need to be in a structure, but it's
 *      a lot cleaner to do so, and a pointer to the structure can
 *      be used as the instance handle.
 */
typedef struct rlm_topt_t {
    const char *name;	/* CONF_SECTION->name, not strdup'd */
    int period;
    int token_digits;
    int pass_digits;
    int backward_drift;
    int forward_drift;
    int drift;    
} rlm_totp_t;

/*
 *      A mapping of configuration file names to internal variables.
 *
 *      Note that the string is dynamically allocated, so it MUST
 *      be freed.  When the configuration file parse re-reads the string,
 *      it free's the old one, and strdup's the new one, placing the pointer
 *      to the strdup'd string into 'config.string'.  This gets around
 *      buffer over-flows.
 */
static const CONF_PARSER module_config[] = {
	{ "period", PW_TYPE_INTEGER, offsetof(rlm_totp_t, period), NULL, "30" },
	{ "token_digits", PW_TYPE_INTEGER, offsetof(rlm_totp_t, token_digits), NULL, "6" },
	{ "pass_digits", PW_TYPE_INTEGER, offsetof(rlm_totp_t, pass_digits), NULL, "6" },
	{ "backward_drift", PW_TYPE_INTEGER, offsetof(rlm_totp_t, backward_drift), NULL, "1" },
	{ "forward_drift", PW_TYPE_INTEGER, offsetof(rlm_totp_t, forward_drift), NULL, "1" },
	{ "drift", PW_TYPE_INTEGER, offsetof(rlm_totp_t, drift), NULL, "0" },
	{ NULL, -1, 0, NULL, NULL }
};

/**
 * @brief Encode a CHAP password without using a valuepair
 *
 *	@bug FIXME: might not work with Ascend because
 *	we use vp->length, and Ascend gear likes
 *	to send an extra '\0' in the string!
 */
int totp_chap_encode(RADIUS_PACKET *packet, uint8_t *output, int id,
		     char *password, int password_length)
{
	int		i;
	uint8_t		*ptr;
	uint8_t		string[MAX_STRING_LEN * 2 + 1];
	VALUE_PAIR	*challenge;

	/*
	 *	Sanity check the input parameters
	 */
	if ((packet == NULL) || (password == NULL)) {
		return -1;
	}

	/*
	 *	Note that the password VP can be EITHER
	 *	a User-Password attribute (from a check-item list),
	 *	or a CHAP-Password attribute (the client asking
	 *	the library to encode it).
	 */

	i = 0;
	ptr = string;
	*ptr++ = id;

	i++;
	memcpy(ptr, password, password_length);
	ptr += password_length;
	i += password_length;

	/*
	 *	Use Chap-Challenge pair if present,
	 *	Request-Authenticator otherwise.
	 */
	challenge = pairfind(packet->vps, PW_CHAP_CHALLENGE);
	if (challenge) {
		memcpy(ptr, challenge->vp_strvalue, challenge->length);
		i += challenge->length;
	} else {
		memcpy(ptr, packet->vector, AUTH_VECTOR_LEN);
		i += AUTH_VECTOR_LEN;
	}

	*output = id;
	fr_md5_calc((uint8_t *)output + 1, (uint8_t *)string, i);

	return 0;
}


static int totp_detach(void *instance)
{
	rlm_totp_t *inst = (rlm_totp_t *) instance;
	free(inst);
	return 0;
}


static int totp_instantiate(CONF_SECTION *conf, void **instance)
{
        rlm_totp_t *inst;

        /*
         *      Set up a storage area for instance data
         */
        inst = rad_malloc(sizeof(*inst));
	if (!inst) {
		return -1;
	}
	memset(inst, 0, sizeof(*inst));

        /*
         *      If the configuration parameters can't be parsed, then
         *      fail.
         */
        if (cf_section_parse(conf, inst, module_config) < 0) {
	    totp_detach(inst);
	    return -1;
        }
	if(inst->pass_digits > inst->token_digits){
	    radlog(L_ERR, "rlm_totp: Pass digits must be less than or equal to token digits");
	    totp_detach(inst);
	    return -1;
	}
	if (inst->token_digits < 6 || inst->token_digits > 9) {
	    radlog(L_ERR, "rlm_totp: Token digits must be in the range 6-9");
	    totp_detach(inst);
	    return -1;
	}

	inst->name = cf_section_name2(conf);
	if (!inst->name) {
		inst->name = cf_section_name1(conf);
	}

        *instance = inst;

        return 0;
}

static int totp_authorize(void *instance, REQUEST *request)
{
	rlm_totp_t *inst = instance;

	if (!pairfind(request->packet->vps, PW_CHAP_PASSWORD)) {
		return RLM_MODULE_NOOP;
	}

	if (pairfind(request->config_items, PW_AUTHTYPE) != NULL) {
		RDEBUG2("WARNING: Auth-Type already set.  Not setting to TOTP");
		return RLM_MODULE_NOOP;
	}

	RDEBUG("Setting 'Auth-Type := TOTP'");
	pairadd(&request->config_items,
		pairmake("Auth-Type", "TOTP", T_OP_EQ));
	return RLM_MODULE_OK;
}


/*
 *	Find the named user in this modules database.  Create the set
 *	of attribute-value pairs to check and reply with for this user
 *	from the database. The authentication code only needs to check
 *	the password, the rest is done here.
 */
static int totp_authenticate(void *instance, REQUEST *request)
{
    rlm_totp_t *inst = instance;
    VALUE_PAIR *passwd_item, *chap;

    uint8_t pass_str[MAX_STRING_LEN];
    uint8_t secret_length;
    VALUE_PAIR *module_fmsg_vp;
    char module_fmsg[MAX_STRING_LEN];
    char hotp_output[inst->token_digits];
    int8_t i;
    uint32_t period;
    time_t secs;

	if (!request->username) {
		radlog_request(L_AUTH, 0, request, "rlm_totp: Attribute \"User-Name\" is required for authentication.\n");
		return RLM_MODULE_INVALID;
	}

	chap = pairfind(request->packet->vps, PW_CHAP_PASSWORD);
	if (!chap) {
		RDEBUG("ERROR: You set 'Auth-Type = TOTP' for a request that does not contain a CHAP-Password attribute!");
		return RLM_MODULE_INVALID;
	}

	if (chap->length == 0) {
		RDEBUG("ERROR: CHAP-Password is empty in totp");
		return RLM_MODULE_INVALID;
	}

	if (chap->length != CHAP_VALUE_LENGTH + 1) {
		RDEBUG("ERROR: CHAP-Password has invalid length in totp");
		return RLM_MODULE_INVALID;
	}
	
	/*
	 *	Don't print out the CHAP password here.  It's binary crap.
	 */
	RDEBUG("login attempt by \"%s\" with CHAP password",
		request->username->vp_strvalue);

	if ((passwd_item = pairfind(request->config_items, PW_CLEARTEXT_PASSWORD)) == NULL){
	  RDEBUG("Cleartext-Password is required for authentication");
		snprintf(module_fmsg, sizeof(module_fmsg),
			 "rlm_chap: Clear text password not available");
		module_fmsg_vp = pairmake("Module-Failure-Message",
					  module_fmsg, T_OP_EQ);
		pairadd(&request->packet->vps, module_fmsg_vp);
		return RLM_MODULE_INVALID;
	}
	
	if (passwd_item->length > 254) {
	    RDEBUG("ERROR: Cleartext TOTP secret is too long");
	    return RLM_MODULE_INVALID;
	}

	RDEBUG("Using clear text password \"%s\" for user %s authentication.",
	      passwd_item->vp_strvalue, request->username->vp_strvalue);

	secret_length = passwd_item->length;
	uint8_t secret[secret_length];
	base64_decode(passwd_item->vp_strvalue, secret);

	secs = time(NULL);
	for(i=-(inst->backward_drift); i <= inst->forward_drift; i++){
	     period = secs / inst->period + inst->drift + i;
	     /*if(oath_hotp_generate((char *)secret,
				   secret_length,
				   period,
				   inst->token_digits,
				   false,
				   OATH_HOTP_DYNAMIC_TRUNCATION,
				   hotp_output)){
		 return RLM_MODULE_REJECT;	     
		 }*/
	     hotp_output[0] = "12345";
	     
	     
	     RDEBUG("HOTP of %s for period %d",
		    hotp_output,
		    period);
	     totp_chap_encode(request->packet,
			      pass_str,
			      chap->vp_octets[0],
			      hotp_output,
			      inst->pass_digits);
	     if (rad_digest_cmp(pass_str + 1, chap->vp_octets + 1,
				CHAP_VALUE_LENGTH) == 0) {
		 RDEBUG("chap user %s authenticated succesfully",
			request->username->vp_strvalue);
		 
		 return RLM_MODULE_OK;
	     }
	}

	RDEBUG("Password check failed");
	snprintf(module_fmsg, sizeof(module_fmsg),
		 "rlm_chap: Wrong user password");
	module_fmsg_vp = pairmake("Module-Failure-Message",
				  module_fmsg, T_OP_EQ);
	pairadd(&request->packet->vps, module_fmsg_vp);
	return RLM_MODULE_REJECT;
}



/*
 *	The module name should be the only globally exported symbol.
 *	That is, everything else should be 'static'.
 *
 *	If the module needs to temporarily modify it's instantiation
 *	data, the type should be changed to RLM_TYPE_THREAD_UNSAFE.
 *	The server will then take care of ensuring that the module
 *	is single-threaded.
 */
module_t rlm_totp = {
	RLM_MODULE_INIT,
	"TOTP",
	RLM_TYPE_CHECK_CONFIG_SAFE | RLM_TYPE_HUP_SAFE | RLM_TYPE_THREAD_SAFE,   	/* type */
	totp_instantiate,		/* instantiation */
	totp_detach,			/* detach */
	{
		totp_authenticate,	/* authentication */
		totp_authorize,		/* authorization */
		NULL,			/* preaccounting */
		NULL,			/* accounting */
		NULL,			/* checksimul */
		NULL,			/* pre-proxy */
		NULL,			/* post-proxy */
		NULL			/* post-auth */
	},
};
