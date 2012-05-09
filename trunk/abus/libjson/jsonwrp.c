/*
 * Copyright (C) 2011-2012
 * Copyright (C) 2012 Stephane Fillod
 * Derived from code Copyright (C) 2009-2011 Vincent Hanquez <vincent@snarc.org>
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

/* ---------------------------------------------------------------------------
{
    "name": "Object1",
    "field1": 2,
    "array": [
        {
            "name": "Object2",
            "value": "object1_in_array"
        },
        {
            "name": "Object2",
            "value": "object2_in_array"
        }
    ]
}
------------------------------------------------------------------------------
object begin (3 element)
key: name
string: Object1
key: field1
integer: 2
key: array
array begin
object begin (2 element)
key: name
string: Object2
key: value
string: object1_in_array
object end
object begin (2 element)
key: name
string: Object2
key: value
string: object2_in_array
object end
array end
object end
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include "json.h"

#ifdef _JSON_DBG
#define json_dbg_error(a...) printf(a)
#define json_dbg_trace(a...) fprintf(stderr, a)
#else
#define json_dbg_error(a...) do { } while (0)
#define json_dbg_trace(a...) do { } while (0)
#endif


static void* tree_create_structure(int nesting, int is_object)
{
	json_dom_val_t* v = malloc(sizeof(json_dom_val_t));
	if (v)
	{
		/* instead of defining a new enum type, we abuse the
		 * meaning of the json enum type for array and object */
		if (is_object)
		{
			v->type = JSON_OBJECT_BEGIN;
			v->u.object = NULL;
		} else {
			v->type = JSON_ARRAY_BEGIN;
			v->u.array = NULL;
		}
		v->length = 0;
	}

	return v;
}

static char* memalloc_copy_length(const char *src, size_t n)
{
	char *dest;

	dest = calloc(n + 1, sizeof(char));
	if (dest)
		memcpy(dest, src, n);
	return dest;
}

static void* tree_create_data(int type, const char *data, size_t length)
{
	json_dom_val_t* v = NULL;

	v = malloc(sizeof(json_dom_val_t));
	if (v) {
		v->type = type;
		v->length = length;
		v->u.data = memalloc_copy_length(data, length);
		if (!v->u.data) {
			free(v);
			return NULL;
		}
	}
	return v;
}

static int tree_append(void *structure, char *key, size_t key_length, void *obj)
{
	json_dom_val_t *parent = structure;

	if (key)
	{
		struct json_dom_val_elem *objelem;

		if (parent->length == 0)
		{
			parent->u.object = calloc(1 + 1, sizeof(json_dom_val_t *)); /* +1 for null */
			if (!parent->u.object)
				return 1;
		} else {
			size_t newsize = parent->length + 1 + 1; /* +1 for null */
			void *newptr;

			newptr = realloc(parent->u.object, newsize * sizeof(json_dom_val_t *));
			if (!newptr)
				return -ENOMEM;
			parent->u.object = newptr;
		}

		objelem = malloc(sizeof(struct json_dom_val_elem));
		if (!objelem)
			return -ENOMEM;

		objelem->key = memalloc_copy_length(key, key_length);
		objelem->key_length = key_length;
		objelem->val = obj;
		parent->u.object[parent->length++] = objelem;
		parent->u.object[parent->length] = NULL;
	} else {
		if (parent->length == 0) {
			parent->u.array = calloc(1 + 1, sizeof(json_dom_val_t *)); /* +1 for null */
			if (!parent->u.array)
				return -ENOMEM;
		} else {
			size_t newsize = parent->length + 1 + 1; /* +1 for null */
			void *newptr;

			newptr = realloc(parent->u.object, newsize * sizeof(json_dom_val_t *));
			if (!newptr)
				return -ENOMEM;
			parent->u.array = newptr;
		}
		parent->u.array[parent->length++] = obj;
		parent->u.array[parent->length] = NULL;
	}
	return 0;
}

static FILE *open_filename(const char *filename, const char *opt)
{
	FILE *input;
	if (strcmp(filename, "-") == 0)
	{
		input = stdin;
	} else {
		input = fopen(filename, opt);
		if (!input)
		{
			json_dbg_error("error: cannot open %s: %s", filename, strerror(errno));
			return NULL;
		}
	}
	return input;
}

static void close_filename(const char *filename, FILE *file)
{
	if (strcmp(filename, "-") != 0)
		fclose(file);
}

static int process_file(json_parser *parser, FILE *input, int *retlines, int *retcols)
{
	char buffer[4096];
	int ret = 0;
	size_t read;
	int lines, col, i;

	lines = 1;
	col   = 0;
	while (1) {
		size_t processed;
		read = fread(buffer, 1, 4096, input);
		if (read <= 0)
			break;
		ret = json_parser_string(parser, buffer, read, &processed);
		for (i = 0; i < (int)processed; i++)
		{
			if (buffer[i] == '\n') { col = 0; lines++; } else col++;
		}
		if (ret)
			break;
	}
	if (retlines) *retlines = lines;
	if (retcols) *retcols = col;
	return ret;
}

static int _do_tree(json_config *config, const char *filename, json_dom_val_t** root_structure)
{
	FILE *input;
	json_parser parser;
	json_parser_dom dom;
	int ret;
	int col, lines;

	input = open_filename(filename, "r");
	if (!input)
		return -errno;

	ret = json_parser_dom_init(&dom, tree_create_structure, tree_create_data, tree_append);
	if (ret)
	{
		json_dbg_error("error: initializing helper failed: [code=%d] %s\n", ret, json_strerror(ret));
		close_filename(filename, input);
		return ret;
	}

	ret = json_parser_init(&parser, config, json_parser_dom_callback, &dom);
	if (ret)
	{
		json_dbg_error("error: initializing parser failed: [code=%d] %s\n", ret, json_strerror(ret));
		json_parser_dom_free(&dom);
		close_filename(filename, input);
		return ret;
	}

	ret = process_file(&parser, input, &lines, &col);
	if (ret)
	{
		json_dbg_error("line %d, col %d: [code=%d] %s\n", lines, col, ret, json_strerror(ret));
		json_parser_dom_free(&dom);
		json_parser_free(&parser);
		close_filename(filename, input);
		return ret;
	}

	ret = json_parser_is_done(&parser);
	if (!ret)
	{
		json_dbg_error("syntax error\n");
		json_parser_dom_free(&dom);
		json_parser_free(&parser);
		close_filename(filename, input);
		return ret;
	}

	if (root_structure)
		*root_structure = dom.root_structure;

	/* cleanup */
	json_parser_dom_free(&dom);
	json_parser_free(&parser);
	close_filename(filename, input);
	return 0;
}

// ----------------------------------------------------------------------------

/**
 * @brief  Function that load and analyse a json file
 * @param  path + json file to parse
 * @return dom like of the json file
 */
json_dom_val_t* json_config_open(const char* szJsonFilename)
{
	json_dom_val_t*	root_structure = NULL;
	json_config config;

	/*
	 * Initialize the json configuration
	 */
	memset(&config, 0, sizeof(json_config));
	config.max_nesting         = 0;
	config.max_data            = 0;
	config.allow_c_comments    = 1;
	config.allow_yaml_comments = 1;

	/*
	 * Come and parse the json file
	 */
	_do_tree(&config, szJsonFilename, &root_structure);

	return(root_structure);
}

/**
 * @brief  Function that clean a json file
 * @param  json object
 * @return none
 */
void json_config_cleanup(json_dom_val_t* element)
{
	int i;

	if (NULL == element)
		return;

	switch (element->type)
	{
		case JSON_OBJECT_BEGIN:
			for (i = 0; i < element->length; i++) {
				free(element->u.object[i]->key);
				json_config_cleanup(element->u.object[i]->val);
				free(element->u.object[i]);
			}
			free(element->u.object);
			break;

		case JSON_ARRAY_BEGIN:
			for (i = 0; i < element->length; i++) {
				json_config_cleanup(element->u.array[i]);
			}
			free(element->u.array);
			break;

		case JSON_FALSE:
		case JSON_TRUE:
		case JSON_NULL:
			//break;

		case JSON_INT:
		case JSON_STRING:
		case JSON_FLOAT:
			free(element->u.data);
			break;
		default:
			json_dbg_error("%s unknown type %d", __func__, element->type);
			break;
	}

	free(element);
}

// ----------------------------------------------------------------------------

/**
 * @brief  Function that locate a main directory
 * @param  pointer to the main json dom
 * @return pointer to the element that contain
 * @todo   make the function reentrant (esp. static indent/isFound)
 */
json_dom_val_t *json_config_lookup(json_dom_val_t* element, const char *name)
{
	int    i;
	static int  indent  = 0;
	static bool isFound = false;

	//json_dbg_trace("\n %d ", indent);
	if (-1 == indent) {
		indent = 0;
		isFound = false;
	}

	/* Check the parameter */
	if (NULL != element)
	{
		json_dbg_trace("\n Current JSON element : %p ", element);

		/* Check if the object have been already found in the json dom */
		/* If found, memorize the pointer to the item				   */
		if (true != isFound)
		{
			if (0 == strcmp("", name)) {
				json_dbg_trace("\n\t ---- '%s' json object found (%p)----", name, element);
				isFound = true;
				indent--;
				return element;
			}

			switch (element->type)
			{
				case JSON_OBJECT_BEGIN:
					json_dbg_trace("- Object begin (%d items) ", element->length);
					for ( i=0 ; i<element->length ; i++ )
					{
						json_dbg_trace("\n\t -> key: %s ", element->u.object[i]->key);
						if (0 == strcmp(element->u.object[i]->key, name))
						{
							json_dbg_trace("\n\t ---- '%s' json object well found (%p)----", name, element->u.object[i]->val);
							isFound = true;
							indent--;
							return(element->u.object[i]->val);
						} else {
							indent++;
							/* FIXME: no return ? */
							json_config_lookup(element->u.object[i]->val, name);
						}
					}
					json_dbg_trace("\n <-- object end ");
					break;

				case JSON_ARRAY_BEGIN:
					json_dbg_trace("- Array begin (%d items in the array) ", element->length);
					for (i = 0; i < element->length; i++)
					{
						indent++;
						/* FIXME: no return ? */
						json_config_lookup(element->u.array[i], name);
					}
					json_dbg_trace("\n <- array end ");
					break;

				case JSON_FALSE:
				case JSON_TRUE:
				case JSON_NULL:
					json_dbg_trace("- constant ");
					break;
				case JSON_INT:
					json_dbg_trace("- integer: %s ", element->u.data);
					break;
				case JSON_STRING:
					json_dbg_trace("- string: %s ", element->u.data);
					break;
				case JSON_FLOAT:
					json_dbg_trace(" - float: %s ", element->u.data);
					break;

				default:
					break;
			}
		}
	}

	indent--;

	return isFound ? element : NULL;
}

/*
 * Helper for json_config_query(), with nesting handling and writable query string
 * NB: this function is recursive
 */
static json_dom_val_t *json_config_query_helper(json_dom_val_t *root, json_dom_val_t *element, char *query, char **endptr, int level)
{
	char *p, *q, c;
    json_dom_val_t *obj, *key, *item;

    *endptr = query;

    /* max nesting level reached */
    if (level <= 0 || !query || !root || !element)
        return NULL;

	if (query[0] == '\0')
        return element;

    /* TODO: operator = and literal ' */

	p = strpbrk(query, ".{}[]");

    if (!p) {
        *endptr = query+strlen(query);
        return json_config_lookup(root, query);
    }

    c = *p;
    *p++ = '\0';

    obj = query+1 == p ? element : json_config_lookup(root, query);
    if (!obj)
        return NULL;

    switch (c) {
        case '}':
        case ']':
            *endptr = p;
            return obj;

        case '.':
  	        q = strpbrk(p, ".{}[]");
            if (q) {
                c = *q;
                *q = '\0';
            } else {
                c = '\0';
  	            q = p + strlen(p);
            }
            item = json_config_lookup(obj, p);
            *q = c;

            return json_config_query_helper(root, item, q, endptr, level-1);

        case '{':
            key = json_config_query_helper(root, element, p, endptr, level-1);
            if (!key)
                return NULL;
            p = *endptr;
            /* TODO check key type */
            item = json_config_lookup(obj, key->u.data);
            return json_config_query_helper(root, item, p, endptr, level-1);

        case '[':
            /* TODO array access */
            return NULL;
    }

	return NULL;
}

/**
  Basic Json Query Language processor
 json_config_query(root, "channelProfile{studioProfile{studioProfileID}.channelProfileID}.rtpPort")
 */
json_dom_val_t *json_config_query(json_dom_val_t *element, const char *query)
{
    char *query_dup, *endptr;
    json_dom_val_t *item;

#define MAX_NESTING_LEVEL 16
    query_dup = strdup(query);
    if (!query_dup)
        return NULL;

	item = json_config_query_helper(element, element, query_dup, &endptr, MAX_NESTING_LEVEL);
    free(query_dup);

    return item;
}


// ----------------------------------------------------------------------------

/**
 * @brief Extract the integer value from a data
 *
 * @param current item
 * @param pointer to the integer value expected
 *
 * @return integer value
 */
int json_config_get_int(json_dom_val_t* element, int* val)
{
	int ret = 0;

	/* Check the parameter */
	if (NULL != element)
	{
		json_dbg_trace("\n [DBG] Integer value : '%s' (%p) ", element->u.data, element);
		*val = atoi(element->u.data);
	} else {
		ret = -1;
		json_dbg_trace("\n [ERROR] NULL parameter given !!");
	}

	return (ret);
}

/**
 * @brief Extract the boolean value from a data
 *
 * @param current item
 * @param pointer to the boolean value expected
 *
 * @return boolean value
 */
int json_config_get_bool(json_dom_val_t* element, bool* val)
{
	int ret = 0;

	/* Check the parameter */
	if (NULL != element)
	{
		json_dbg_trace("\n [DBG] Boolean value : '%s' (%p) ", element->u.data, element);
		*val = (bool)atoi(element->u.data);
	} else {
		ret = -1;
		json_dbg_trace("\n [ERROR] NULL parameter given !!");
	}

	return (ret);
}

/**
 * @brief Extract the string value from a data
 *
 * @param current item
 * @param pointer to the string value expected
 *
 * @return string value
 */
int  json_config_get_string(json_dom_val_t* element, char** val)
{
	int ret = 0;

	/* Check the parameter */
	if (NULL != element)
	{
		json_dbg_trace("\n [DBG] String value : '%s' (%p) ", element->u.data, element);
		*val = strdup(element->u.data);
	} else {
		ret = -1;
		json_dbg_trace("\n [ERROR] NULL parameter given !!");
	}

	return (ret);
}

/**
 * @brief Extract the double value from a data
 *
 * @param current item
 * @param pointer to the double value expected
 *
 * @return return code
 */
int json_config_get_double(json_dom_val_t* element, double* val)
{
	int ret = 0;

	/* Check the parameter */
	if (NULL != element)
	{
		json_dbg_trace("\n [DBG] Double value : '%s' (%p) ", element->u.data, element);
		*val = atof(element->u.data);
	} else {
		ret = -1;
		json_dbg_trace("\n [ERROR] NULL parameter given !!");
	}

	return (ret);
}

/* Internal helper function */
static int json_config_get_direct_type(json_dom_val_t *root, const char *query, void* val, json_type type)
{
	json_dom_val_t *myItem;
	char *endptr;

	/* Check the parameter */
	if (NULL == root) {
		json_dbg_trace("\n [ERROR] NULL parameter given !!");
		return -EINVAL;
	}

	myItem = json_config_query(root, query);
	if (NULL == myItem)
		return -ENOENT;

	if ((json_type)myItem->type != type && !(myItem->type == JSON_FALSE && type == JSON_TRUE))
		return -ENOTTY;

	switch (type) {
	case JSON_INT:
		json_dbg_trace("\n [DBG] Integer value : '%s' (%p) ", myItem->u.data, myItem);

 		*(int *)val = strtol(myItem->u.data, &endptr, 10);
		if (myItem->u.data == endptr)
			return -ENOTTY;
		break;

	case JSON_STRING:
		json_dbg_trace("\n [DBG] String value : '%s' (%p) ", myItem->u.data, myItem);

 		*(char **)val = myItem->u.data;
		break;

	case JSON_TRUE:
	case JSON_FALSE:
		json_dbg_trace("\n [DBG] Boolean value : '%s' (%p) ", myItem->u.data, myItem);
		*(bool *)val = (myItem->type == JSON_FALSE) ? false : true;
		break;

	case JSON_FLOAT:
		json_dbg_trace("\n [DBG] Float value : '%s' (%p) ", myItem->u.data, myItem);

		*(double*)val = strtod(myItem->u.data, &endptr);
		if (myItem->u.data == endptr)
			return -ENOTTY;
		break;

	default:
		json_dbg_trace("\n [ERROR] %s: invalid type", __func__);
		return -EINVAL;
	}
	return 0;
}


// ----------------------------------------------------------------------------

/**
 * @brief Extract the integer value from a data
 *
 * @param pointer to the dom
 * @param item's name
 * @param the integer value expected
 *
 * @return zero if successful, non nul value otherwise
 */
int json_config_get_direct_int(json_dom_val_t *root, const char *itemName, int* val)
{
    return json_config_get_direct_type(root, itemName, val, JSON_INT);
}

/**
 * @brief Extract the boolean value from a data
 *
 * @param pointer to the dom
 * @param item's name
 * @param the boolean value expected
 *
 * @return zero if successful, non nul value otherwise
 */
int json_config_get_direct_bool(json_dom_val_t *root, const char *itemName, bool* val)
{
    return json_config_get_direct_type(root, itemName, val, JSON_TRUE);
}

/**
 * @brief Extract the string value from a data
 *
 * @param pointer to the dom
 * @param item's name
 * @param the string value expected, dynamically allocated
 *
 * @return zero if successful, non nul value otherwise
 */
int json_config_get_direct_string(json_dom_val_t *root, const char *itemName, char **val)
{
	int ret;
	char *mval;

	ret = json_config_get_direct_type(root, itemName, &mval, JSON_STRING);
	if (ret == 0)
		*val = strdup(mval);

	return ret;
}

/**
 * @brief Extract the string value from a data
 *
 * @param pointer to the dom
 * @param item's name
 * @param the string value expected, not dynamically allocated, valid as long as root valid
 * @param size of value
 *
 * @return zero if successful, non nul value otherwise
 */
int json_config_get_direct_strp(json_dom_val_t *root, const char *itemName, const char **val, size_t *n)
{
	int ret;

	ret = json_config_get_direct_type(root, itemName, (void *)val, JSON_STRING);

	if (ret == 0 && n)
		*n = strlen(*val);

	return ret;
}

/**
 * @brief Extract the double float value from a data
 *
 * @param pointer to the dom
 * @param item's name
 * @param the double value expected
 *
 * @return zero if successful, non nul value otherwise
 */
int json_config_get_direct_double(json_dom_val_t *root, const char *itemName, double* val)
{
    return json_config_get_direct_type(root, itemName, val, JSON_FLOAT);
}

/**
 * @brief  Function that locates an index within an array
 * @param  pointer to the main json dom
 * @param  item's name
 * @param  array index
 */
json_dom_val_t *json_config_get_direct_array(json_dom_val_t *root, const char *arrayName, unsigned idx)
{
	json_dom_val_t *myArray;

	myArray = json_config_query(root, arrayName);
	if (NULL == myArray || JSON_ARRAY_BEGIN != myArray->type || (int)idx >= myArray->length)
		return NULL;

	return myArray->u.array[idx];
}

/**
 * @brief Get array count of element
 *
 * @param pointer to the main json dom
 * @param array's name
 *
 * @return integer value
 */
int json_config_get_direct_array_count(json_dom_val_t *root, const char *arrayName)
{
	json_dom_val_t *myArray;

	myArray = json_config_query(root, arrayName);
	if (NULL == myArray)
		return -ENOENT;

	if (JSON_ARRAY_BEGIN != myArray->type)
		return -ENOTTY;

	return myArray->length;
}

// ----------------------------------------------------------------------------

#ifdef __MAIN__
/**
 * @brief  Main entry used for test
 *
 * @param  number or arguments
 * @param  list of the arguments provided
 * @param  list of the environment variable provided
 *
 * @return main return code
 */
int main(int argc, char* argv[], char* env[])
{
	int             ret				= -1;
	char*		    valItem         = NULL;
	char*			valItem2		= NULL;
	char*			valItem3		= NULL;
	json_dom_val_t* json_dom 		= NULL;
	json_dom_val_t* myParentItem	= NULL;
	json_dom_val_t* myItem			= NULL;

	/* Load and parse the json file */
	if (argc >=2)
	{
		if ( NULL != (json_dom = json_config_open(argv[1])))
		{
			printf("\n [DBG] JSON init and converted into a DOM : OK ");
			printf("\n [DBG] Looking for the parent item ('result') . . .");

			/* --------------------------------------------------------- */
			/* First example                                             */
			/* --------------------------------------------------------- */
			if (NULL != (myParentItem = json_config_lookup(json_dom, "networking")))
			{
				printf("\n [DBG] Parent item 'networking' (%p) well found", myParentItem);
				if (NULL != (myItem = json_config_lookup(myParentItem, "ipaddress")))
				{
					printf("\n [DBG] Child item 'ipaddress' (%p) well found ", myItem);
					if (-1 != (ret = json_config_get_string(myItem, &valItem)))
						printf("\n [DBG] Item's value = '%s' ", valItem);
					else
						printf("\n [ERROR] Problem to get the item's value ");
				} else {
					printf("\n [ERROR] Child item not found ");
				}

				if (NULL != (myItem = json_config_lookup(myParentItem, "gateway")))
				{
					printf("\n [DBG] Child item 'gateway' (%p) well found ", myItem);
					if (-1 != (ret = json_config_get_string(myItem, &valItem2)))
						printf("\n [DBG] Item's value = '%s' ", valItem2);
					else
						printf("\n [ERROR] Problem to get the item's value ");
				} else {
					printf("\n [ERROR] Child item not found ");
				}
			} else {
				printf("\n [ERROR] Parent item not found ");
			}

			/* --------------------------------------------------------- */
			/* Second example                                            */
			/* --------------------------------------------------------- */
			json_config_get_direct_string(json_dom, "networking", "ipaddress", &valItem3);
			printf("\n [DBG] Item's value = '%s' ", valItem3);
			free(valItem3);

			/* Cleaning the dom */
			if (NULL != json_dom)
				json_config_cleanup(json_dom);
		} else {
			printf("\n [ERROR] JSON init and converted into a DOM : NOK ");
		}
		printf("\n");
	} else {
		printf("\n [ERROR] Expected parameter to %s !!", argv[0]);
		printf("\n [ERROR] Usage: %s <json file> ", argv[0]);
	}

	return(ret);
}
#endif

