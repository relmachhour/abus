/*
 * Copyright (C) 2011-2012
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
#include <assert.h>
#include <locale.h>
#include <getopt.h>
#include <errno.h>
#include <stdbool.h>

#include "json.h"

static char* string_of_errors[] =
{
	[JSON_ERROR_NO_MEMORY]							= "out of memory",
	[JSON_ERROR_BAD_CHAR]							= "bad character",
	[JSON_ERROR_POP_EMPTY]							= "stack empty",
	[JSON_ERROR_POP_UNEXPECTED_MODE]				= "pop unexpected mode",
	[JSON_ERROR_NESTING_LIMIT]						= "nesting limit",
	[JSON_ERROR_DATA_LIMIT]							= "data limit",
	[JSON_ERROR_COMMENT_NOT_ALLOWED]				= "comment not allowed by config",
	[JSON_ERROR_UNEXPECTED_CHAR]					= "unexpected char",
	[JSON_ERROR_UNICODE_MISSING_LOW_SURROGATE]		= "missing unicode low surrogate",
	[JSON_ERROR_UNICODE_UNEXPECTED_LOW_SURROGATE]	= "unexpected unicode low surrogate",
	[JSON_ERROR_COMMA_OUT_OF_STRUCTURE] 			= "error comma out of structure",
	[JSON_ERROR_CALLBACK]							= "error in a callback",
	[JSON_ERROR_UTF8]								= "utf8 validation error"
};

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
} /* tree_create_structure */

static char* memalloc_copy_length(const char *src, uint32_t n)
{
	char *dest;

	dest = calloc(n + 1, sizeof(char));
	if (dest)
		memcpy(dest, src, n);
	return dest;
} /* memalloc_copy_length */

static void* tree_create_data(int type, const char *data, uint32_t length)
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
} /* tree_create_data */

static int tree_append(void *structure, char *key, uint32_t key_length, void *obj)
{
	json_dom_val_t *parent = structure;

	if (key)
	{
		struct json_val_elem *objelem;

		if (parent->length == 0)
		{
			parent->u.object = calloc(1 + 1, sizeof(json_dom_val_t *)); /* +1 for null */
			if (!parent->u.object)
				return 1;
		} else {
			uint32_t newsize = parent->length + 1 + 1; /* +1 for null */
			void *newptr;

			newptr = realloc(parent->u.object, newsize * sizeof(json_dom_val_t *));
			if (!newptr)
				return -1;
			parent->u.object = newptr;
		}

		objelem = malloc(sizeof(struct json_val_elem));
		if (!objelem)
			return -1;

		objelem->key = memalloc_copy_length(key, key_length);
		objelem->key_length = key_length;
		objelem->val = obj;
		parent->u.object[parent->length++] = objelem;
		parent->u.object[parent->length] = NULL;
	} else {
		if (parent->length == 0) {
			parent->u.array = calloc(1 + 1, sizeof(json_dom_val_t *)); /* +1 for null */
			if (!parent->u.array)
				return 1;
		} else {
			uint32_t newsize = parent->length + 1 + 1; /* +1 for null */
			void *newptr;

			newptr = realloc(parent->u.object, newsize * sizeof(json_dom_val_t *));
			if (!newptr)
				return -1;
			parent->u.array = newptr;
		}
		parent->u.array[parent->length++] = obj;
		parent->u.array[parent->length] = NULL;
	}
	return 0;
} /* tree_append */

static FILE *open_filename(const char *filename, const char *opt, int is_input)
{
	FILE *input;
	if (strcmp(filename, "-") == 0)
	{
		input = (is_input) ? stdin : stdout;
	} else {
		input = fopen(filename, opt);
		if (!input)
		{
#ifdef _JSON_DBG
			fprintf(stderr, "error: cannot open %s: %s", filename, strerror(errno));
#endif
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
	int32_t read;
	int lines, col, i;

	lines = 1;
	col   = 0;
	while (1) {
		uint32_t processed;
		read = fread(buffer, 1, 4096, input);
		if (read <= 0)
			break;
		ret = json_parser_string(parser, buffer, read, &processed);
		for (i = 0; i < processed; i++)
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

	input = open_filename(filename, "r", 1);
	if (!input)
		return 2;

	ret = json_parser_dom_init(&dom, tree_create_structure, tree_create_data, tree_append);
	if (ret)
	{
#ifdef _JSON_DBG
		fprintf(stderr, "error: initializing helper failed: [code=%d] %s\n", ret, string_of_errors[ret]);
#endif
		return ret;
	}

	ret = json_parser_init(&parser, config, json_parser_dom_callback, &dom);
	if (ret)
	{
#ifdef _JSON_DBG
		fprintf(stderr, "error: initializing parser failed: [code=%d] %s\n", ret, string_of_errors[ret]);
#endif
		return ret;
	}

	ret = process_file(&parser, input, &lines, &col);
	if (ret)
	{
#ifdef _JSON_DBG
		fprintf(stderr, "line %d, col %d: [code=%d] %s\n", lines, col, ret, string_of_errors[ret]);
#endif
		return 1;
	}

	ret = json_parser_is_done(&parser);
	if (!ret)
	{
#ifdef _JSON_DBG
		fprintf(stderr, "syntax error\n");
#endif
		return 1;
	}

	if (root_structure)
		*root_structure = dom.root_structure;

	/* cleanup */
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
	int         ret            = 0;

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
} /* json_config_init */

/**
 * @brief  Function that clean a json file
 * @param  json object
 * @return none
 */
void json_config_cleanup(json_dom_val_t* element)
{
	if (NULL != element)
	{
		free(element);
		element = NULL;
	} /* IF */
} /* json_config_cleanup */

// ----------------------------------------------------------------------------

/**
 * @brief  Function that locate a main directory
 * @param  pointer to the main json dom
 * @return pointer to the element that contain
 */
json_dom_val_t* json_config_lookup(json_dom_val_t* element, const char* szDirectoryNane)
{
	int    i;
	static int  indent  = 0;
	static bool isFound = false;

	//printf("\n %d ", indent);
	if (-1 == indent) {
		indent = 0;
		isFound = false;
	}

	/* Check the parameter */
	if (NULL != element)
	{
#ifdef _JSON_DBG
		printf("\n Current JSON element : %p ", element );
#endif

		/* Check if the object have been already found in the json dom */
		/* If found, memorize the pointer to the item				   */
		if (true != isFound)
		{
			switch (element->type)
			{
				case JSON_OBJECT_BEGIN:
#ifdef _JSON_DBG
					printf("- Object begin (%d items) ", element->length);
#endif
					for ( i=0 ; i<element->length ; i++ )
					{
#ifdef _JSON_DBG
						printf("\n\t -> key: %s ", element->u.object[i]->key);
#endif
						if (0 == strcmp(element->u.object[i]->key, szDirectoryNane))
						{
#ifdef _JSON_DBG
							printf("\n\t ---- '%s' json object well found (%p)----", szDirectoryNane, element->u.object[i]->val);
#endif
							isFound = true;
							indent--;
							return(element->u.object[i]->val);
						} else {
							indent++;
							json_config_lookup(element->u.object[i]->val, szDirectoryNane);
						} /* IF */
					} /* FOR */
#ifdef _JSON_DBG
					printf("\n <-- object end ");
#endif
					break;

				case JSON_ARRAY_BEGIN:
#ifdef _JSON_DBG
					printf("- Array begin (%d items in the array) ", element->length);
#endif
					for (i = 0; i < element->length; i++)
					{
						indent++;
						json_config_lookup(element->u.array[i], szDirectoryNane);
					}
#ifdef _JSON_DBG
					printf("\n <- array end ");
#endif
					break;

				case JSON_FALSE:
				case JSON_TRUE:
				case JSON_NULL:
#ifdef _JSON_DBG
					printf("- constant ");
#endif
					break;
				case JSON_INT:
#ifdef _JSON_DBG
					printf("- integer: %s ", element->u.data);
#endif
					break;
				case JSON_STRING:
#ifdef _JSON_DBG
					printf("- string: %s ", element->u.data);
#endif
					break;
				case JSON_FLOAT:
#ifdef _JSON_DBG
					printf(" - float: %s ", element->u.data);
#endif
					break;

				default:
					break;
			} /* SWITCH */
		} /* IF */
	} /* IF */

	indent--;

	return (element);
} /* json_config_lookup */

// ----------------------------------------------------------------------------

/*
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
#ifdef _JSON_DBG
		printf("\n [DBG] Integer value : '%s' (%p) ", element->u.data, element);
#endif
		*val = atoi(element->u.data);
	} else {
		ret = -1;
#ifdef _JSON_DBG
		printf("\n [ERROR] NULL parameter given !!");
#endif
	} /* IF */

	return (ret);
} /* json_config_get_int */

/*
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
#ifdef _JSON_DBG
		printf("\n [DBG] Boolean value : '%s' (%p) ", element->u.data, element);
#endif
		*val = (bool)atoi(element->u.data);
	} else {
		ret = -1;
#ifdef _JSON_DBG
		printf("\n [ERROR] NULL parameter given !!");
#endif
	} /* IF */

	return (ret);
} /* json_config_get_bool */

/*
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
#ifdef _JSON_DBG
		printf("\n [DBG] String value : '%s' (%p) ", element->u.data, element);
#endif
		*val = strdup(element->u.data);
	} else {
		ret = -1;
#ifdef _JSON_DBG
		printf("\n [ERROR] NULL parameter given !!");
#endif
	} /* IF */

	return (ret);
} /* json_config_get_string */

/*
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
#ifdef _JSON_DBG
		printf("\n [DBG] Double value : '%s' (%p) ", element->u.data, element);
#endif
		*val = atof(element->u.data);
	} else {
		ret = -1;
#ifdef _JSON_DBG
		printf("\n [ERROR] NULL parameter given !!");
#endif
	} /* IF */

	return (ret);
} /* json_config_get_double */

// ----------------------------------------------------------------------------

/*
 * @brief Extract the integer value from a data
 *
 * @param pointer to the dom
 * @param directory name
 * @param attribute's name
 * @paral the integer value expected
 *
 * @return return code
 */
int json_config_get_direct_int(json_dom_val_t* root, const char* directoryName, const char* attributeName, int* val)
{
	int  ret                        = 0;
	json_dom_val_t* myParentItem	= NULL;
	json_dom_val_t* myItem			= NULL;

	/* Check the parameter */
	if (NULL != root)
	{
		if (NULL != (myParentItem = json_config_lookup(root, directoryName)))
		{
			if (NULL != (myItem = json_config_lookup(myParentItem, attributeName)))
			{
#ifdef _JSON_DBG
				printf("\n [DBG] Integer value : '%s' (%p) ", myItem->u.data, myItem);
#endif
				*val = atoi(myItem->u.data);
			} else {
				ret = -1;
			} /* IF */
		} else {
			ret = -2;
		} /* IF */
	} else {
		ret = -3;
#ifdef _JSON_DBG
		printf("\n [ERROR] NULL parameter given !!");
#endif
	} /* IF */

	return (ret);
} /* json_config_get_int */

/*
 * @brief Extract the boolean value from a data
 *
 * @param pointer to the dom
 * @param directory name
 * @param attribute's name
 * @paral the boolean value expected
 *
 * @return return code
 */
bool json_config_get_direct_bool(json_dom_val_t* root, const char* directoryName, const char* attributeName, bool* val)
{
	int  ret                        = 0;
	json_dom_val_t* myParentItem	= NULL;
	json_dom_val_t* myItem			= NULL;

	/* Check the parameter */
	if (NULL != root)
	{
		if (NULL != (myParentItem = json_config_lookup(root, directoryName)))
		{
			if (NULL != (myItem = json_config_lookup(myParentItem, attributeName)))
			{
#ifdef _JSON_DBG
				printf("\n [DBG] Boolean value : '%s' (%p) ", myItem->u.data, myItem);
#endif
				*val = (bool)atoi(myItem->u.data);
			} else {
				ret = -1;
			} /* IF */
		} else {
			ret = -2;
		} /* IF */
	} else {
		ret = -3;
#ifdef _JSON_DBG
		printf("\n [ERROR] NULL parameter given !!");
#endif
	} /* IF */

	return (ret);
} /* json_config_get_bool */

/*
 * @brief Extract the string value from a data
 *
 * @param pointer to the dom
 * @param directory name
 * @param attribute's name
 * @paral the string value expected
 *
 * @return return code
 */
int json_config_get_direct_string(json_dom_val_t* root, const char* directoryName, const char* attributeName, char** val)
{
	int  ret                        = 0;
	json_dom_val_t* myParentItem	= NULL;
	json_dom_val_t* myItem			= NULL;

	/* Check the parameter */
	if (NULL != root)
	{
		if (NULL != (myParentItem = json_config_lookup(root, directoryName)))
		{
			if (NULL != (myItem = json_config_lookup(myParentItem, attributeName)))
			{
#ifdef _JSON_DBG
				printf("\n [DBG] String value : '%s' (%p) ", myItem->u.data, myItem);
#endif
				*val = strdup(myItem->u.data);
			} else {
				ret = -1;
			} /* IF */
		} else {
			ret = -2;
		} /* IF */
	} else {
		ret = -3;
#ifdef _JSON_DBG
		printf("\n [ERROR] NULL parameter given !!");
#endif
	} /* IF */

	return (ret);
} /* json_config_get_direct_string */

/*
 * @brief Extract the double value from a data
 *
 * @param pointer to the dom
 * @param directory name
 * @param attribute's name
 * @paral the double value expected
 *
 * @return return code
 */
int json_config_get_direct_double(json_dom_val_t* root, const char* directoryName, const char* attributeName, double* val)
{
	int  ret                        = 0;
	json_dom_val_t* myParentItem	= NULL;
	json_dom_val_t* myItem			= NULL;

	/* Check the parameter */
	if (NULL != root)
	{
		if (NULL != (myParentItem = json_config_lookup(root, directoryName)))
		{
			if (NULL != (myItem = json_config_lookup(myParentItem, attributeName)))
			{
#ifdef _JSON_DBG
				printf("\n [DBG] String value : '%s' (%p) ", myItem->u.data, myItem);
#endif
				*val = atof(myItem->u.data);
			} else {
				ret = -1;
			} /* IF */
		} else {
			ret = -2;
		} /* IF */
	} else {
		ret = -3;
#ifdef _JSON_DBG
		printf("\n [ERROR] NULL parameter given !!");
#endif
	} /* IF */

	return (ret);
} /* json_config_get_double */

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
				} /* IF */

				if (NULL != (myItem = json_config_lookup(myParentItem, "gateway")))
				{
					printf("\n [DBG] Child item 'gateway' (%p) well found ", myItem);
					if (-1 != (ret = json_config_get_string(myItem, &valItem2)))
						printf("\n [DBG] Item's value = '%s' ", valItem2);
					else
						printf("\n [ERROR] Problem to get the item's value ");
				} else {
					printf("\n [ERROR] Child item not found ");
				} /* IF */
			} else {
				printf("\n [ERROR] Parent item not found ");
			} /* IF */

			/* --------------------------------------------------------- */
			/* Second example                                            */
			/* --------------------------------------------------------- */
			json_config_get_direct_string(json_dom, "networking", "ipaddress", &valItem3);
			printf("\n [DBG] Item's value = '%s' ", valItem3);

			/* Cleaning the dom */
			if (NULL != json_dom)
				json_config_cleanup(json_dom);
		} else {
			printf("\n [ERROR] JSON init and converted into a DOM : NOK ");
		} /* IF */
		printf("\n");
	} else {
		printf("\n [ERROR] Expected parameter to %s !!", argv[0]);
		printf("\n [ERROR] Usage: %s <json file> ", argv[0]);
	} /* IF */

	return(ret);
}
#endif



