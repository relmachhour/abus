/*
 * Copyright (C) 2011 Stephane Fillod
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 */

/*
 * JSON RPC 2.0 for A-Bus
 *	http://jsonrpc.org/spec.html
 *
 * Rem: only exposed API is doxygenized
 */

/*!
 * \addtogroup abus
 * @{
 */
#include "abus_config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdbool.h>

#include "json.h"
#include "hashtab.h"

#include "jsonrpc.h"

#define LogError(...)    do { fprintf(stderr, ##__VA_ARGS__); fprintf(stderr, "\n"); } while (0)
#define LogDebug(...)    do { fprintf(stderr, ##__VA_ARGS__); fprintf(stderr, "\n"); } while (0)

static int json_rpc_add_val(json_rpc_t *json_rpc, int type, const char *data, int length);
static int json_rpc_add_array(json_rpc_t *json_rpc);
static int json_rpc_add_object_to_array(json_rpc_t *json_rpc);
static int json_rpc_parser_callback(void *userdata, int type, const char *data, size_t length);

const char *json_rpc_strerror(int errnum)
{
	switch (errnum) {
		case 0: return "success";
		case JSONRPC_PARSE_ERROR: return JSONRPC_PARSE_ERROR_MSG;
		case JSONRPC_INVALID_REQUEST: return JSONRPC_INVALID_REQUEST_MSG;
		case JSONRPC_NO_METHOD: return JSONRPC_NO_METHOD_MSG;
		case JSONRPC_INVALID_METHOD: return JSONRPC_INVALID_METHOD_MSG;
		case JSONRPC_INTERNAL_ERROR: return JSONRPC_INTERNAL_ERROR_MSG;
		case JSONRPC_SERVER_ERROR: return JSONRPC_SERVER_ERROR_MSG;
		default:
			return "Unknown error";
	}
	return "";
}

static void json_val_set_string(json_val_t *json_val, char *data, int length)
{
	json_val->type = JSON_STRING;
	json_val->u.data = data;
	json_val->length = length;
}

int json_val_is_undef(const json_val_t *json_val)
{
	return json_val->type == JSON_NONE || json_val->type == JSON_NULL;
}
 
void json_val_free(json_val_t *json_val)
{
	assert(json_val->type != JSON_ARRAY_HTAB);

	if (json_val->u.data) {
		free(json_val->u.data);
		json_val->u.data = NULL;
	}
}

int json_rpc_init(json_rpc_t *json_rpc)
{
	/* need to set various fields to zero */
	memset(json_rpc, 0, sizeof(json_rpc_t));

	json_rpc->sock = -1;

	json_rpc->parsing_status = PARSING_UNKNOWN;
	json_rpc->params_htab = hcreate(3);

	return 0;
}

void json_rpc_cleanup(json_rpc_t *json_rpc)
{
	if (hfirst(json_rpc->params_htab)) do
	{
		json_val_t *val;

		free(hkey(json_rpc->params_htab));
		val = hstuff(json_rpc->params_htab);

		if (val->type == JSON_ARRAY_HTAB) {
			int i;
			for (i=0; i < val->length; i++) {
				htab *h = val->u.htab_array[i];
				if (hfirst(h)) do
				{
					json_val_t *ha_val;

					free(hkey(h));
					ha_val = hstuff(h);

					if (ha_val->u.data)
						free(ha_val->u.data);
					free(ha_val);
				}
				while (hnext(h));
				hdestroy(h);
			}
			free(val->u.htab_array);
		} else if (val->u.data)
			free(val->u.data);
		free(val);
	}
	while (hnext(json_rpc->params_htab));
	hdestroy(json_rpc->params_htab);

	if (json_rpc->service_name)
		free(json_rpc->service_name);
	if (json_rpc->method_name)
		free(json_rpc->method_name);
	if (json_rpc->last_param_key)
		free(json_rpc->last_param_key);
	json_val_free(&json_rpc->id);
	if (json_rpc->msgbuf)
		free(json_rpc->msgbuf);
}

int json_rpc_parse_msg(json_rpc_t *json_rpc, const char *buffer, size_t len)
{
	int ret = 0;
	size_t processed = len;
	json_parser parser;
	json_config config;

	memset(&config, 0, sizeof(config));
	config.max_nesting = 6;
	config.max_data = JSONRPC_REQ_SZ_MAX;

	ret = json_parser_init(&parser, &config, &json_rpc_parser_callback, json_rpc);
	if (ret) {
		LogError("libjson error: initializing parser failed: [code=%d]\n", ret);
		return ret;
	}

	ret = json_parser_string(&parser, buffer, len, &processed);
	if (ret) {
		LogError("libjson parser error: %d\n", ret);
		return JSONRPC_PARSE_ERROR;
	}

	ret = json_parser_is_done(&parser);
	if (!ret || json_rpc->parsing_status != PARSING_V2_0) {
		LogError("libjson syntax error\n");

		ret = JSONRPC_PARSE_ERROR;
	} else if ((json_rpc->service_name && json_rpc->method_name)
					|| !json_rpc->error_code) {
		json_rpc->parsing_status = PARSING_OK;
		ret = 0;
	}

	json_parser_free(&parser);

	return ret;
}

int json_rpc_is_req(json_rpc_t *json_rpc)
{
	return json_rpc->parsing_status == PARSING_OK && !json_rpc->error_code
		&& json_rpc->service_name && json_rpc->method_name;
}

struct jsprint_cb_user {
	char *dest;
	size_t remaining;
};

static int json_print_val_callback(void *user, const char *s, size_t length)
{
	struct jsprint_cb_user *jsprint_user = (struct jsprint_cb_user *)user;
	size_t n;

	n = length > jsprint_user->remaining ? jsprint_user->remaining : length;

	memcpy(jsprint_user->dest, s, n);
	jsprint_user->dest += n;
	jsprint_user->remaining -= n;

	return n;
}

static int json_print_val(char *s, int len, const json_val_t *val)
{
	json_printer print;
	struct jsprint_cb_user jsprint_user;
	int ret;

	if (len == 0)
		return -ENOSPC;

	jsprint_user.dest = s;
	jsprint_user.remaining = len;
	ret = json_print_init(&print, json_print_val_callback, &jsprint_user);
	if (ret)
		return ret;

	ret = json_print_raw(&print, val->type, val->u.data, val->length);

	json_print_free(&print);

	if (ret < 0 || ret == len)
		return ret == len ? -ENOSPC : ret;

	s[ret] = '\0';

	return ret;
}

static char *msg_p(json_rpc_t *json_rpc)
{
	return json_rpc->msgbuf + json_rpc->msglen;
}

static int msg_rem(json_rpc_t *json_rpc)
{
	int rem = json_rpc->msgbufsz - json_rpc->msglen - 1;
	return rem < 0 ? 0 : rem;
}


int json_rpc_resp_init(json_rpc_t *json_rpc)
{
	char *resp;
	int len;

	resp = malloc(JSONRPC_RESP_SZ_MAX);
	if (!resp)
		return JSON_ERROR_NO_MEMORY;

	json_rpc->msglen = 0;
	json_rpc->msgbufsz = JSONRPC_RESP_SZ_MAX;

	if (json_rpc->msgbuf)
		free(json_rpc->msgbuf);
	json_rpc->msgbuf = resp;

	if (json_rpc->error_code != 0) {
		struct json_val error_msg_val;
		const char *error_msg;

		json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc),
						"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%d,\"message\":",
						json_rpc->error_code);

		error_msg = json_rpc_strerror(json_rpc->error_code);
		json_val_set_string(&error_msg_val, (char *)error_msg, strlen(error_msg));

		len = json_print_val(msg_p(json_rpc), msg_rem(json_rpc), &error_msg_val);
		if (len < 0)
			return len;

		json_rpc->msglen += len;

	} else {
		/* NB: imply "result": to be a dict */
		json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc),
						"{\"jsonrpc\":\"2.0\",\"result\":{");
	}

	return 0;
}

int json_rpc_resp_finalize(json_rpc_t *json_rpc)
{
	int len;

	/* force "id":null in case of invalid JSON */
	if (json_rpc->error_code != 0 && json_rpc->id.type == JSON_NONE)
		json_rpc->id.type = JSON_NULL;

	if (!json_val_is_undef(&json_rpc->id)) {
		json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc),
						"},\"id\":");

		len = json_print_val(msg_p(json_rpc), msg_rem(json_rpc), &json_rpc->id);
		if (len < 0)
			return len;

		json_rpc->msglen += len;
	}

	json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc), "}");

	return 0;
}

/*
  semi-optimized lexer for JSON-RPC keywords
*/
static enum json_rpc_key_token_e json_rpc_key_token(const char *data, size_t length)
{
#define KEY_IDX(c) ((c)-'a')
	static const struct { const char *key; enum json_rpc_key_token_e token; }
		key_token[26] = {
			[KEY_IDX('c')] = { "code", TOK_CODE },	/* in "error" level */
			[KEY_IDX('d')] = { "data", TOK_DATA },	/* in "error" level */
			[KEY_IDX('e')] = { "error", TOK_ERROR },
			[KEY_IDX('i')] = { "id", TOK_ID },
			[KEY_IDX('j')] = { "jsonrpc", TOK_JSONRPC },
			[KEY_IDX('m')] = { "method", TOK_METHOD },
			/* "message" (specific to "error" level) is handled explicitely
			 * because it conflicts with "method"
			 */
			[KEY_IDX('p')] = { "params", TOK_PARAMS },
			[KEY_IDX('r')] = { "result", TOK_RESULT },
		};
	int key_idx = KEY_IDX(data[0]);

	if (!length || data[0] < 'a' || data[0] > 'z' || !key_token[key_idx].key)
		return TOK_UNKNOWN;

	if (!strncmp(data, key_token[key_idx].key, length))
		return key_token[key_idx].token;

	if (!strncmp(data, "message", length))
		return TOK_MESSAGE;

	return TOK_UNKNOWN;
}

int json_rpc_parser_callback(void *userdata, int type, const char *data, size_t length)
{
	json_rpc_t *json_rpc = userdata;

#if 0
	LogDebug("atom: %d %.*s\n", type, length, data);
#endif

	switch (type) {
	case JSON_OBJECT_BEGIN:
		++json_rpc->nesting_level;
		if (json_rpc->nesting_level == 2 &&
						(json_rpc->last_key_token == TOK_PARAMS
						 || json_rpc->last_key_token == TOK_RESULT
						 || json_rpc->last_key_token == TOK_ERROR))
			json_rpc->param_state = true;
		else if (json_rpc->nesting_level == 4)
			json_rpc_add_object_to_array(json_rpc);
		break;

	case JSON_OBJECT_END:
		--json_rpc->nesting_level;
		if (json_rpc->param_state && json_rpc->nesting_level == 3)
			json_rpc->pointed_htab = NULL;
		else if (json_rpc->nesting_level < 2)
			json_rpc->param_state = false;
		break;

	case JSON_ARRAY_BEGIN:
		if (++json_rpc->nesting_level == 3 && json_rpc->param_state) 
		{
			int ret = json_rpc_add_array(json_rpc);
			if (ret != 0) {
				json_rpc->parsing_status = PARSING_INVALID;
				return ret;
			}
		}
		break;

	case JSON_ARRAY_END:
		--json_rpc->nesting_level;
		assert (json_rpc->last_array);
		json_rpc->last_array = NULL;
		break;

	case JSON_KEY:
		if (!json_rpc->param_state) {
			json_rpc->last_key_token = json_rpc_key_token(data, length);
			if (json_rpc->last_key_token == TOK_ERROR)
				json_rpc->error_token_seen = true;
		} else {
			json_rpc->last_param_key = strndup(data, length);
			if (json_rpc->error_token_seen)
				json_rpc->last_key_token = json_rpc_key_token(data, length);
		}
		break;

	case JSON_STRING:
		if (!json_rpc->param_state) {
			if (json_rpc->last_key_token == TOK_JSONRPC) {
				if (strncmp(data, "2.0", length) != 0) {
					json_rpc->parsing_status = PARSING_INVALID;
					return JSON_ERROR_CALLBACK;
				}
				json_rpc->parsing_status = PARSING_V2_0;
				json_rpc->last_key_token = TOK_NONE;
				break;
			} else if (json_rpc->last_key_token == TOK_METHOD) {
				char *p = memchr(data, '.', length);
				if (p) {
					int service_len = p - data;
					if (service_len >= JSONRPC_SVCNAME_SZ_MAX)
						json_rpc->parsing_status = PARSING_INVALID;
					else
						json_rpc->service_name = strndup(data, service_len);
					data += service_len+1;
					length -= service_len+1;
				}
				if (length >= JSONRPC_METHNAME_SZ_MAX)
					json_rpc->parsing_status = PARSING_INVALID;
				else
					json_rpc->method_name = strndup(data, length);
				json_rpc->last_key_token = TOK_NONE;
			}
			break;
		}
		/* fall through */

	case JSON_INT:
	case JSON_FLOAT:
	case JSON_NULL:
	case JSON_TRUE:
	case JSON_FALSE:

		if (!json_rpc->param_state && json_rpc->last_key_token == TOK_ID) {
			/* TODO: double check response's id matches the request */
			json_val_free(&json_rpc->id);

			json_rpc->id.type = type;
			json_rpc->id.length = length;
			json_rpc->id.u.data = data ? strndup(data, length) : NULL;
			json_rpc->last_key_token = TOK_NONE;
			break;
		}

		if (json_rpc->param_state) {
			int ret = json_rpc_add_val(json_rpc, type, data, length);
			if (ret != 0) {
				json_rpc->parsing_status = PARSING_INVALID;
				return ret;
			}
			if (json_rpc->error_token_seen && type == JSON_INT &&
							json_rpc->last_key_token == TOK_CODE) {
				char *endptr;
				json_rpc->error_code = strtol(data, &endptr, 10);
				if (data == endptr)
					json_rpc->parsing_status = PARSING_INVALID;
			}
		}

		break;
	default:
		json_rpc->parsing_status = PARSING_INVALID;
		return JSON_ERROR_CALLBACK;
	}
	return 0;
}

int json_rpc_add_val(json_rpc_t *json_rpc, int type, const char *data, int length)
{
	json_val_t *json_val;
	htab *p_htab;

	if (json_rpc->last_key_token == TOK_NONE ||
					json_rpc->last_key_token == TOK_UNKNOWN)
		return JSON_ERROR_CALLBACK;

	json_val = malloc(sizeof(json_val_t));
	if (!json_val)
		return JSON_ERROR_NO_MEMORY;

	json_val->type = type;
	json_val->length = length;
	json_val->u.data = data ? strndup(data, length) : NULL;

	if (json_rpc->pointed_htab)
		p_htab = json_rpc->pointed_htab;
	else
		p_htab = json_rpc->params_htab;

	if (!hadd(p_htab, json_rpc->last_param_key,
							strlen(json_rpc->last_param_key), (void *)json_val))
	{
		/* duplicate */
		return JSON_ERROR_CALLBACK;
	}

	json_rpc->last_param_key = NULL;

	return 0;
}

int json_rpc_add_array(json_rpc_t *json_rpc)
{
	json_val_t *json_val;

	if (json_rpc->last_key_token == TOK_NONE ||
					json_rpc->last_key_token == TOK_UNKNOWN)
		return JSON_ERROR_CALLBACK;

	json_val = malloc(sizeof(json_val_t));
	if (!json_val)
		return JSON_ERROR_NO_MEMORY;

	json_val->type = JSON_ARRAY_HTAB;
	json_val->length = 0;
	json_val->u.htab_array = NULL;

	if (!hadd(json_rpc->params_htab, json_rpc->last_param_key,
							strlen(json_rpc->last_param_key), (void *)json_val))
	{
		/* duplicate */
		return JSON_ERROR_CALLBACK;
	}

	json_rpc->last_array = json_val;
	json_rpc->last_param_key = NULL;

	return 0;
}

int json_rpc_add_object_to_array(json_rpc_t *json_rpc)
{
	htab **pp_htab;
	int len;

	if (!json_rpc->last_array)
		return JSON_ERROR_CALLBACK;

	pp_htab = json_rpc->last_array->u.htab_array;
	len = json_rpc->last_array->length++;
	pp_htab = realloc(pp_htab, json_rpc->last_array->length * sizeof(htab*));
	if (!pp_htab)
		return JSON_ERROR_NO_MEMORY;

	json_rpc->last_array->u.htab_array = pp_htab;
	pp_htab[len] = hcreate(2);
	json_rpc->pointed_htab = pp_htab[len];

	return 0;
}

/*!
	Get the JSON RPC type of a paramter

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" designating an integer value
  \return a nul of positive number representing the JSON type (JSON_{INT,FLOAT,STRING,TRUE,FALSE,NULL}), a negative value in case of error
 */
int json_rpc_get_type(json_rpc_t *json_rpc, const char *name)
{
	json_val_t *json_val;
	htab *p_htab;

	if (json_rpc->parsing_status != PARSING_OK || !name)
		return JSONRPC_PARSE_ERROR;

	if (json_rpc->pointed_htab)
		p_htab = json_rpc->pointed_htab;
	else
		p_htab = json_rpc->params_htab;

	if (!hfind(p_htab, name, strlen(name)))
		return JSONRPC_INVALID_METHOD;	/* not found */

	json_val = hstuff(p_htab);

	return json_val->type;
}

static int json_rpc_check_val(json_rpc_t *json_rpc, const char *name, json_val_t **json_valp)
{
	json_val_t *json_val;
	htab *p_htab;

	if (json_rpc->parsing_status != PARSING_OK || !name || !json_valp)
		return JSONRPC_PARSE_ERROR;

	if (json_rpc->pointed_htab)
		p_htab = json_rpc->pointed_htab;
	else
		p_htab = json_rpc->params_htab;

	if (!hfind(p_htab, name, strlen(name)))
		return JSONRPC_INVALID_METHOD;	/* not found */

	json_val = hstuff(p_htab);
	*json_valp = json_val;

	if (json_val->type == JSON_NULL)
		return -3;	/* null type */

	return 0;
}

static int json_rpc_check_val_type(json_rpc_t *json_rpc, const char *name, json_val_t **json_valp, int type)
{
	json_val_t *json_val;
	int ret;

	ret = json_rpc_check_val(json_rpc, name, json_valp);
	if (ret)
		return ret;

	json_val = *json_valp;

	if (json_val->type != type &&
			!(json_val->type == JSON_INT && type == JSON_LLINT))
		return JSONRPC_INVALID_METHOD;	/* wrong type */

	if (!json_val->u.data ||
				(type != JSON_STRING && json_val->length == 0))
		return JSONRPC_INTERNAL_ERROR;

	return 0;
}

/*!
	Get the value of a parameter of type integer from a RPC

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" designating an integer value
  \param[out] val pointer to location where to store the integer value
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_get_int(json_rpc_t *json_rpc, const char *name, int *val)
{
	json_val_t *json_val;
	char *endptr;
	int ret;

	ret = json_rpc_check_val_type(json_rpc, name, &json_val, JSON_INT);
	if (ret)
		return ret;

	*val = strtol(json_val->u.data, &endptr, 10);

	if (json_val->u.data == endptr)
		return JSONRPC_PARSE_ERROR;

	return 0;
}

/*!
	Get the value of a parameter of type long long integer from a RPC

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" designating a long long integer value
  \param[out] val pointer to location where to store the integer value
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_get_llint(json_rpc_t *json_rpc, const char *name, long long *val)
{
	json_val_t *json_val;
	char *endptr;
	int ret;

	ret = json_rpc_check_val_type(json_rpc, name, &json_val, JSON_LLINT);
	if (ret)
		return ret;

	*val = strtoll(json_val->u.data, &endptr, 10);

	if (json_val->u.data == endptr)
		return JSONRPC_PARSE_ERROR;

	return 0;
}

/*!
	Get the value of a parameter of type boolean from a RPC

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" designating a boolean value
  \param[out] val pointer to location where to store the boolean value
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_get_bool(json_rpc_t *json_rpc, const char *name, bool *val)
{
	json_val_t *json_val;
	int ret;

	ret = json_rpc_check_val(json_rpc, name, &json_val);
	if (ret)
		return ret;

	switch (json_val->type) {
		case JSON_TRUE:  *val = true; break;
		case JSON_FALSE: *val = false; break;
		default:
			return JSONRPC_PARSE_ERROR;
	}

	return 0;
}

/*!
	Get the value of a parameter of type double float from a RPC

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" designating a double float value
  \param[out] val pointer to location where to store the double float value
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_get_double(json_rpc_t *json_rpc, const char *name, double *val)
{
	json_val_t *json_val;
	char *endptr;
	int ret;

	ret = json_rpc_check_val_type(json_rpc, name, &json_val, JSON_FLOAT);
	if (ret)
		return ret;

	*val = strtod(json_val->u.data, &endptr);

	if (json_val->u.data == endptr)
		return JSONRPC_PARSE_ERROR;

	return 0;
}

/*!
	Get the value of a parameter of type string from a RPC

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" designating a string value
  \param[out] val pointer to location where to store the string value
  \param[in,out] n pointer to maximum size of the location where to store the string, real size upon return
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_get_strn(json_rpc_t *json_rpc, const char *name, char *val, size_t *n)
{
	json_val_t *json_val;
	int ret, len;

	ret = json_rpc_check_val_type(json_rpc, name, &json_val, JSON_STRING);
	if (ret)
		return ret;

	len = *n < json_val->length ? *n : json_val->length;
	memcpy(val, json_val->u.data, len);
	*n = len;

	return 0;
}

/*!
	Get a pointer to the value of a parameter of type string from a RPC

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" designating a string value
  \param[out] pval pointer to location where to store a pointer to the string value
  \param[out] n pointer where to store the real size of the string
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_get_strp(json_rpc_t *json_rpc, const char *name, const char **pval, size_t *n)
{
	json_val_t *json_val;
	int ret;

	ret = json_rpc_check_val_type(json_rpc, name, &json_val, JSON_STRING);
	if (ret)
		return ret;

	*pval = (const char*)json_val->u.data;
	*n = json_val->length;

	return 0;
}

/*!
	Get the value of a parameter of type string (nul terminated) from a RPC

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" designating a string value
  \param[out] val pointer to location where to store the string value
  \param[in] n maximum size of the location where to store the string
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_get_str(json_rpc_t *json_rpc, const char *name, char *val, size_t n)
{
	int ret;
    size_t len;

    if (n == 0)
        return JSONRPC_INVALID_METHOD;

    len = n-1;

    ret = json_rpc_get_strn(json_rpc, name, val, &len);
	if (ret < 0)
		return ret;

    if (len >= 0 && len < n)
        val[len] = '\0';

	return 0;
}

/*!
	Get the count of objects in "params" or "result" in a parsed JSON RPC, pertaining to a key

	\param json_rpc	handle to a JSON RPC, be it a received request (service side) or a received response/event (client side)
	\param[in] name	key name of a "params"/"result" containing an array of objects
	\return	a nul of positive number representing the count of objects in the array, a negative value in case of error
 */
int json_rpc_get_array_count(json_rpc_t *json_rpc, const char *name)
{
	json_val_t *json_val;

	if (json_rpc->parsing_status != PARSING_OK || !name)
		return -1;

	if (!hfind(json_rpc->params_htab, name, strlen(name)))
		return -2;	/* not found */

	json_val = hstuff(json_rpc->params_htab);

	if (json_val->type != JSON_ARRAY_HTAB)
		return -3;	/* not an array type */

	return json_val->length;
}

/*!
  Aim the json_rpc_get_{int,bool,..} functions at an object within an array of "params" or "result".

  The JSON RPC must have been already parsed, for instance in a
  received request (service side) or a received response/event (client side).

  Limitation: there might be only one level of array, e.g."result":{ "ary":[{"a":5,"b":8},{"a":-1,"c":"foo"}]}

  \param json_rpc	handle to a JSON RPC, be it a received request (service side) or a received response/event (client side)
  \param[in] name	key name within a "params"/"result" containing an array of objects. A NULL pointer resets pointing wihtin "params"/"result".
  \param[in] idx	array index to point at, starting from 0
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_get_point_at(json_rpc_t *json_rpc, const char *name, int idx)
{
	json_val_t *json_val;

	if (json_rpc->parsing_status != PARSING_OK)
		return -1;

	if (!name) {
		/* reset pointing */
		json_rpc->pointed_htab = NULL;
		return 0;
	}

	if (!hfind(json_rpc->params_htab, name, strlen(name)))
		return -2;	/* not found */

	json_val = hstuff(json_rpc->params_htab);

	if (json_val->type != JSON_ARRAY_HTAB)
		return -3;	/* not an array type */

	if (idx >= json_val->length)
		return -4;	/* out of bound */

	json_rpc->pointed_htab = json_val->u.htab_array[idx];

	return 0;
}

/* Request specific */

int json_rpc_req_init(json_rpc_t *json_rpc, const char *service_name, const char *method_name, unsigned id)
{
	int ret;
	char idbuf[12];

	ret = json_rpc_init(json_rpc);
	if (ret)
		return ret;

	json_rpc->msgbuf = malloc(JSONRPC_REQ_SZ_MAX);
	if (!json_rpc->msgbuf)
		return JSON_ERROR_NO_MEMORY;

	sprintf(idbuf, "%u", id);

	json_rpc->msgbufsz = JSONRPC_REQ_SZ_MAX;

	/* FIXME */
	if (id == (unsigned)-1) {
		json_rpc->msglen = snprintf(json_rpc->msgbuf, JSONRPC_REQ_SZ_MAX,
					"{\"jsonrpc\":\"2.0\",\"method\":\"%s.%s\",\"params\":{",
					service_name, method_name);
	} else {
		json_rpc->msglen = snprintf(json_rpc->msgbuf, JSONRPC_REQ_SZ_MAX,
					"{\"jsonrpc\":\"2.0\",\"method\":\"%s.%s\",\"id\":%s,\"params\":{",
					service_name, method_name, idbuf);
		json_val_set_string(&json_rpc->id, strdup(idbuf), strlen(idbuf));
	}

	json_rpc->service_name = strdup(service_name);
	json_rpc->method_name = strdup(method_name);

	return 0;
}

int json_rpc_req_finalize(json_rpc_t *json_rpc)
{
	json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc), "}}");

	return 0;
}

/*!
	Set the error code and message in a RPC before response

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] error_code integer value of the error, as per JSON/XML convention
  \param[in] message pointer to a string describing the error, may be NULL
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_set_error(json_rpc_t *json_rpc, int error_code, const char *message)
{
	struct json_val error_msg_val;
	const char *error_msg;
	int len;

	json_rpc->error_code = error_code;

	if (json_rpc->error_code == 0)
		return 0;

	/* Overwrite unfinished response with "error" tag */
	json_rpc->msglen = 0;
	json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc),
					"{\"jsonrpc\":\"2.0\",\"error\":{\"code\":%d,\"message\":",
					json_rpc->error_code);

	error_msg = message ? message : json_rpc_strerror(json_rpc->error_code);
	json_val_set_string(&error_msg_val, (char *)error_msg, strlen(error_msg));

	len = json_print_val(msg_p(json_rpc), msg_rem(json_rpc), &error_msg_val);
	if (len < 0)
		return len;

	json_rpc->msglen += len;

	return 0;
}

static int json_rpc_is_comma_needed(const json_rpc_t *json_rpc)
{
	char prev_c;
	
	if (json_rpc->msglen <= 0)
		return 0;

	prev_c = json_rpc->msgbuf[json_rpc->msglen-1];

	return prev_c != '{' && prev_c != '[' && prev_c != ':';
}

/*!
	Append to a RPC a new parameter and its value of type integer

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" that's going to designate the integer value
  \param[in] val the integer value
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_append_int(json_rpc_t *json_rpc, const char *name, int val)
{
	if (json_rpc_is_comma_needed(json_rpc) && msg_rem(json_rpc) >= 1)
		json_rpc->msgbuf[json_rpc->msglen++] = ',';

	json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc),
					"\"%s\":%d", name, val);
	return 0;
}

/*!
	Append to a RPC a new parameter and its value of type long long integer

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" that's going to designate the long long integer value
  \param[in] val the integer value
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_append_llint(json_rpc_t *json_rpc, const char *name, long long val)
{
	if (json_rpc_is_comma_needed(json_rpc) && msg_rem(json_rpc) >= 1)
		json_rpc->msgbuf[json_rpc->msglen++] = ',';

	json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc),
					"\"%s\":%"PRId64, name, val);
	return 0;
}

/*!
	Append to a RPC a new parameter and its value of type boolean

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" that's going to designate the boolean value
  \param[in] val the boolean value
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_append_bool(json_rpc_t *json_rpc, const char *name, bool val)
{
	if (json_rpc_is_comma_needed(json_rpc) && msg_rem(json_rpc) >= 1)
		json_rpc->msgbuf[json_rpc->msglen++] = ',';

	json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc),
					"\"%s\":%s", name, val ? "true" : "false");
	return 0;
}

/*!
	Append to a RPC a new parameter and its value of type double float

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" that's going to designate the double float value
  \param[in] val the double float value
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_append_double(json_rpc_t *json_rpc, const char *name, double val)
{
	char s[24];

	if (json_rpc_is_comma_needed(json_rpc) && msg_rem(json_rpc) >= 1)
		json_rpc->msgbuf[json_rpc->msglen++] = ',';

	snprintf(s, sizeof(s)-3, "%.16g", val);
	if (!strchr(s, '.'))
		strcat(s, ".0");

	json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc),
					"\"%s\":%s", name, s);
	return 0;
}

/*!
	Append to a RPC a new parameter and its value of type null

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" that's going to designate the null value
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_append_null(json_rpc_t *json_rpc, const char *name)
{
	if (json_rpc_is_comma_needed(json_rpc) && msg_rem(json_rpc) >= 1)
		json_rpc->msgbuf[json_rpc->msglen++] = ',';

	json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc),
					"\"%s\":null", name);
	return 0;
}

/*!
	Append to a RPC a new parameter and its value of type string

  The string will be escaped appropriately regarding JSON and UTF-8 formats.
  Binary content is allowed.

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] name key name of a "params"/"result" that's going to designate the string value
  \param[in] val pointer to the string
  \param[in] n length of the string to append
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_append_strn(json_rpc_t *json_rpc, const char *name, const char *val, size_t n)
{
	struct json_val str_val;
	int ret;

	if (json_rpc_is_comma_needed(json_rpc) && msg_rem(json_rpc) >= 1)
		json_rpc->msgbuf[json_rpc->msglen++] = ',';

	json_rpc->msglen += snprintf(msg_p(json_rpc), msg_rem(json_rpc),
					"\"%s\":", name);

	json_val_set_string(&str_val, (char *)val, n);

	/* appropriate escaping */
	ret = json_print_val(msg_p(json_rpc), msg_rem(json_rpc), &str_val);
	if (ret < 0)
		return ret;

	json_rpc->msglen += ret;

	return 0;
}

/*!
  \fn int json_rpc_append_str(json_rpc_t *json_rpc, const char *name, const char *s)
  \brief Append to a RPC a new parameter and its value of type string (nul terminated)
*/

/*!
	Append to a RPC a va_list of JSON atoms

 * json_rpc_append_vargs takes multiple json_type's and pass them to JSON RPC message
 * Array, object and constants doesn't take a string and length argument.
 * int, float, key, string need to be followed by a pointer to char and then a length.
 * If the length argument is -1, then the strlen() function will be used on the string argument.
 * The argument list should always be terminated by -1
 * NB: json_rpc_append_vargs() is a low-level JSON-RPC formating primitive, and does not guarantee
 * JSON-RPC syntax. Use at your own risk.

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \param[in] ap the variadic argument
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_append_vargs(json_rpc_t *json_rpc, va_list ap)
{
	json_printer printer;
	struct jsprint_cb_user jsprint_user;
	int ret, rem;
	char *p;
	int prev_c;
	
	p = msg_p(json_rpc);
	rem = msg_rem(json_rpc);

	if (rem == 0)
		return -ENOSPC;

	jsprint_user.dest = p;
	jsprint_user.remaining = rem;

	ret = json_print_init(&printer, json_print_val_callback, &jsprint_user);
	if (ret)
		return ret;

	if (json_rpc->msglen > 0)
		prev_c = json_rpc->msgbuf[json_rpc->msglen-1];
	else
		prev_c = -1;

	printer.enter_object = prev_c == '{' || prev_c == '[';
	printer.afterkey = prev_c == ':';

	ret = json_vprint_args(&printer, json_print_raw, ap);

	json_print_free(&printer);

	if (ret == rem) {
		json_rpc->msglen += ret;
		return -ENOSPC;
	}
	if (ret < 0)
		return ret;

	p[ret] = '\0';
	json_rpc->msglen += ret;

	return 0;
}

/*!
 * Variable arg wrapper for json_rpc_append_vargs()

  \param json_rpc pointer to an opaque handle of a JSON RPC
  \return	0 if successful, non nul value otherwise
 */
int json_rpc_append_args(json_rpc_t *json_rpc, ...)
{
	int ret;
	va_list ap;

	va_start(ap, json_rpc);
	ret = json_rpc_append_vargs(json_rpc, ap);
	va_end(ap);

	return ret;
}

/*! @} */
