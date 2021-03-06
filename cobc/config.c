/*
   Copyright (C) 2003-2012, 2014-2016 Free Software Foundation, Inc.
   Written by Keisuke Nishida, Roger While, Simon Sobisch

   This file is part of GnuCOBOL.

   The GnuCOBOL compiler is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   GnuCOBOL is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GnuCOBOL.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "config.h"
#include "defaults.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>

#include "cobc.h"

enum cb_config_type {
	CB_ANY = 0,
	CB_INT,			/* integer */
	CB_STRING,		/* "..." */
	CB_BOOLEAN,		/* 'yes', 'no' */
	CB_SUPPORT		/* 'ok', 'archaic', 'obsolete',
				   'skip', 'ignore', 'unconformable' */
};

/* Global variables */

#undef	CB_CONFIG_ANY
#undef	CB_CONFIG_INT
#undef	CB_CONFIG_STRING
#undef	CB_CONFIG_BOOLEAN
#undef	CB_CONFIG_SUPPORT

#define CB_CONFIG_ANY(type,var,name)	type		var = (type)0;
#define CB_CONFIG_INT(var,name)		unsigned int		var = 0;
#define CB_CONFIG_STRING(var,name)	const char	*var = NULL;
#define CB_CONFIG_BOOLEAN(var,name)	unsigned int		var = 0;
#define CB_CONFIG_SUPPORT(var,name)	enum cb_support	var = CB_OK;

#include "config.def"

#undef	CB_CONFIG_ANY
#undef	CB_CONFIG_INT
#undef	CB_CONFIG_STRING
#undef	CB_CONFIG_BOOLEAN
#undef	CB_CONFIG_SUPPORT

#define CB_CONFIG_ANY(type,var,name)	, {CB_ANY, name, (void *)&var, NULL}
#define CB_CONFIG_INT(var,name)		, {CB_INT, name, (void *)&var, NULL}
#define CB_CONFIG_STRING(var,name)	, {CB_STRING, name, (void *)&var, NULL}
#define CB_CONFIG_BOOLEAN(var,name)	, {CB_BOOLEAN, name, (void *)&var, NULL}
#define CB_CONFIG_SUPPORT(var,name)	, {CB_SUPPORT, name, (void *)&var, NULL}

/* Local variables */

static struct config_struct {
	const enum cb_config_type	type;
	const char			*name;
	void				*var;
	char				*val;
} config_table[] = {
	{CB_STRING, "include", NULL, NULL},
	{CB_STRING, "includeif", NULL, NULL},
	{CB_STRING, "not-reserved", NULL, NULL},
	{CB_STRING, "reserved", NULL, NULL}
#include "config.def"
};

/* Configuration includes */
static struct includelist {
	struct includelist	*next;
	const char		*name;
} *conf_includes = NULL;

#undef	CB_CONFIG_ANY
#undef	CB_CONFIG_INT
#undef	CB_CONFIG_STRING
#undef	CB_CONFIG_BOOLEAN
#undef	CB_CONFIG_SUPPORT

#define	CB_CONFIG_SIZE	sizeof(config_table) / sizeof(struct config_struct)

/* Local functions */

static char *
read_string (const char *text)
{
	char			*p;
	char			*s;

	s = cobc_main_strdup (text);
	if (*s == '\"') {
		s++;
	}
	for (p = s; *p; p++) {
		if (*p == '\"') {
			*p = '\0';
		}
	}
	return s;
}

static void
invalid_value (const char *fname, const int line, const char *name, const char *val,
			   const char *str, const int max, const int min)
{
	configuration_error (fname, line, 0,
		_("Invalid value '%s' for configuration tag '%s'"), val, name);
	if (str) {
		configuration_error (fname, line, 1,
			_("should be one of the following values: %s"), str);
	} else if (max == min && max == 0) {
		configuration_error (fname, line, 1, _("must be numeric"));
	} else if (max) {
		configuration_error (fname, line, 1, _("maximum value: %lu"), (unsigned long)max);
	} else {
		configuration_error (fname, line, 1, _("minimum value: %lu"), (unsigned long)min);
	}
}

static void
unsupported_value (const char *fname, const int line, const char *name, const char *val)
{
	configuration_error (fname, line, 1, 
		_("Unsupported value '%s' for configuration tag '%s'"), val, name);
}

/* Global functions */

int
cb_load_std (const char *name)
{
	return cb_load_conf (name, 1);
}

int
cb_config_entry (char *buff, const char *fname, const int line)
{
	char			*s;
	const char		*name;
	char			*e;
	const char		*val;
	void			*var;
	size_t			i;
	size_t			j;
	int				v;

	/* Get tag */
	s = strpbrk (buff, " \t:=");
	if (!s) {
		for (j=strlen(buff); buff[j-1] == '\r' || buff[j-1] == '\n'; )	/* Remove CR LF */
			buff[--j] = 0;
		configuration_error (fname, line, 1,
			_("Invalid configuration tag '%s'"), buff);
		return -1;
	}
	*s = 0;
	
	/* Find entry */
	for (i = 0; i < CB_CONFIG_SIZE; i++) {
		if (strcmp (buff, config_table[i].name) == 0) {
			break;
		}
	}
	if (i == CB_CONFIG_SIZE) {
		configuration_error (fname, line, 1, _("Unknown configuration tag '%s'"), buff);
		return -1;
	}

	/* Get value */
	/* Move pointer to beginning of value */
	for (s++; *s && strchr (" \t:=", *s); s++) {
		;
	}
	/* Set end pointer to first # (comment) or end of value */
	for (e = s + 1; *e && !strchr ("#", *e); e++) {
		;
	}
	/* Remove trailing white-spaces */
	for (--e; e >= s && strchr (" \t\r\n", *e); e--) {
		;
	}
	e[1] = 0;
	config_table[i].val = s;

	/* Set value */
	name = config_table[i].name;
	var = config_table[i].var;
	val = config_table[i].val;
	switch (config_table[i].type) {
		case CB_ANY:
			if (strcmp (name, "assign-clause") == 0) {
				if (strcmp (val, "cobol2002") == 0) {
					unsupported_value (fname, line, name, val);
					return -1;
				} else if (strcmp (val, "mf") == 0) {
					cb_assign_clause = CB_ASSIGN_MF;
				} else if (strcmp (val, "ibm") == 0) {
					cb_assign_clause = CB_ASSIGN_IBM;
				} else {
					invalid_value (fname, line, name, val, "cobol2002, mf, ibm", 0, 0);
					return -1;
				}
			} else if (strcmp (name, "binary-size") == 0) {
				if (strcmp (val, "2-4-8") == 0) {
					cb_binary_size = CB_BINARY_SIZE_2_4_8;
				} else if (strcmp (val, "1-2-4-8") == 0) {
					cb_binary_size = CB_BINARY_SIZE_1_2_4_8;
				} else if (strcmp (val, "1--8") == 0) {
					cb_binary_size = CB_BINARY_SIZE_1__8;
				} else {
					invalid_value (fname, line, name, val, "2-4-8, 1-2-4-8, 1--8", 0, 0);
					return -1;
				}
			} else if (strcmp (name, "binary-byteorder") == 0) {
				if (strcmp (val, "native") == 0) {
					cb_binary_byteorder = CB_BYTEORDER_NATIVE;
				} else if (strcmp (val, "big-endian") == 0) {
					cb_binary_byteorder = CB_BYTEORDER_BIG_ENDIAN;
				} else {
					invalid_value (fname, line, name, val, "native, big-endian", 0, 0);
					return -1;
				}
			}
			break;
		case CB_INT:
			for (j = 0; val[j]; j++) {
				if (val[j] < '0' || val[j] > '9') {
					invalid_value (fname, line, name, val, NULL, 0, 0);
					return -1;
					break;
				}
			}
			v = atoi (val);
			if (strcmp (name, "tab-width") == 0) {
				if (v < 1) {
					invalid_value (fname, line, name, val, NULL, 1, 0);
					return -1;
				}
				if (v > 8) {
					invalid_value (fname, line, name, val, NULL, 0, 8);
					return -1;
				}
			} else if (strcmp (name, "text-column") == 0) {
				if (v < 72) {
					invalid_value (fname, line, name, val, NULL, 72, 0);
					return -1;
				}
				if (v > 255) {
					invalid_value (fname, line, name, val, NULL, 0, 255);
					return -1;
				}
			} else if (strcmp (name, "word-length") == 0) {
				if (v < 1) {
					invalid_value (fname, line, name, val, NULL, 1, 0);
					return -1;
				}
				if (v > COB_MAX_WORDLEN) {
					invalid_value (fname, line, name, val, NULL, 0, COB_MAX_WORDLEN);
					return -1;
				}
			}
			*((int *)var) = v;
			break;
		case CB_STRING:
			val = read_string (val);

			if (strcmp (name, "include") == 0 ||
				strcmp (name, "includeif") == 0) {
				if (fname) {
					/* Include another conf file */
					s = cob_expand_env_string((char *)val);
					strcpy (buff, s);
					cob_free (s);
					if (strcmp (name, "includeif") == 0) {
						return 3;
					} else {
						return 1;
					}
				} else {
					configuration_error (NULL, 0, 1,
					      _("'%s' not supported with -cb_conf"), name);
					return -1;
				}
			} else if (strcmp (name, "not-reserved") == 0) {
				remove_reserved_word (val);
			} else if (strcmp (name, "reserved") == 0) {
			        add_reserved_word (val, fname, line);
			} else {
				*((const char **)var) = val;
			}
			
			break;
		case CB_BOOLEAN:
			if (strcmp (val, "yes") == 0) {
				*((int *)var) = 1;
			} else if (strcmp (val, "no") == 0) {
				*((int *)var) = 0;
			} else {
				invalid_value (fname, line, name, val, "yes, no", 0, 0);
				return -1;
			}
			break;
		case CB_SUPPORT:
			if (strcmp (val, "ok") == 0) {
				*((enum cb_support *)var) = CB_OK;
			} else if (strcmp (val, "warning") == 0) {
				*((enum cb_support *)var) = CB_WARNING;
			} else if (strcmp (val, "archaic") == 0) {
				*((enum cb_support *)var) = CB_ARCHAIC;
			} else if (strcmp (val, "obsolete") == 0) {
				*((enum cb_support *)var) = CB_OBSOLETE;
			} else if (strcmp (val, "skip") == 0) {
				*((enum cb_support *)var) = CB_SKIP;
			} else if (strcmp (val, "ignore") == 0) {
				*((enum cb_support *)var) = CB_IGNORE;
			} else if (strcmp (val, "error") == 0) {
				*((enum cb_support *)var) = CB_ERROR;
			} else if (strcmp (val, "unconformable") == 0) {
				*((enum cb_support *)var) = CB_UNCONFORMABLE;
			} else {
				invalid_value (fname, line, name, val, 
					"ok, warning, archaic, obsolete, skip, ignore, error, unconformable", 0, 0);
				return -1;
			}
			break;
		default:
			configuration_error (fname, line, 1, _("Invalid type for '%s'"), name);
			return -1;
	}
	return 0;
}

static int
cb_load_conf_file (const char *conf_file, int isoptional)
{
	char			filename[COB_NORMAL_BUFF];
	struct includelist	*c, *cc;
	const unsigned char	*x;
	FILE			*fp;
	int			sub_ret, ret;
	int			i, line;
	char			buff[COB_SMALL_BUFF];

	for (i=0; conf_file[i] != 0 && conf_file[i] != SLASH_CHAR; i++);
	if (conf_file[i] == 0) {			/* Just a name, No directory */
		if (access(conf_file, F_OK) != 0) {	/* and file does not exist */
			/* check for path of previous configuration file (for includes) */
			c = conf_includes;
			if (c) {
				while (c->next != NULL) {
					c = c->next;
				}
			}
			filename[0] = 0;
			if (c && c->name) {
				strcpy(buff, conf_includes->name);
				for (i = (int)strlen(buff); i != 0 && buff[i] != SLASH_CHAR; i--);
				if (i != 0) {
					buff[i] = 0;
					snprintf(filename, (size_t)COB_NORMAL_MAX, "%s%c%s", buff, SLASH_CHAR, conf_file);
					if (access(filename, F_OK) == 0) {	/* and prefixed file exist */
						conf_file = filename;		/* Prefix last directory */
					} else {
						filename[0] = 0;
					}
				}
			}
			if (filename[0] == 0) {
				/* check for COB_CONFIG_DIR (use default if not in environment) */
				snprintf (filename, (size_t)COB_NORMAL_MAX, "%s%c%s", cob_config_dir, SLASH_CHAR, conf_file);
				filename[COB_NORMAL_MAX] = 0;
				if (access(filename, F_OK) == 0) {	/* and prefixed file exist */
					conf_file = filename;		/* Prefix COB_CONFIG_DIR */
				}
			}
		}
	}

	/* check for recursion */
	c = cc = conf_includes;
	while (c != NULL) {
		if (c->name /* <- silence warnings */ && strcmp(c->name, conf_file) == 0) {
			configuration_error (conf_file, 0, 1, _("Recursive inclusion"));
			return -2;
		}
		cc = c;
		c = c->next;
	}

	/* Open the configuration file */
	fp = fopen (conf_file, "r");
	if (fp == NULL) {
		if (!isoptional) {
			fflush (stderr);
			configuration_error (conf_file, 0, 1, _("No such file or directory"));
			return -1;
		} else {
			return 0;
		}
	}

	/* add current entry to list*/
	c = cob_malloc (sizeof(struct includelist));
	c->next = NULL;
	c->name = conf_file;
	if (cc != NULL) {
		cc->next = c;
	} else {
		conf_includes = c;
	}

	/* Read the configuration file */
	ret = 0;
	line = 0;
	while (fgets (buff, COB_SMALL_BUFF, fp)) {
		line++;

		/* Skip line comments, empty lines */
		if (buff[0] == '#' || buff[0] == '\n') {
			continue;
		}

		/* Skip blank lines */
		for (x = (const unsigned char *)buff; *x; x++) {
			if (isgraph (*x)) {
				break;
			}
		}
		if (!*x) {
			continue;
		}

		sub_ret = cb_config_entry (buff, conf_file, line);
		if (sub_ret == 1 || sub_ret == 3) {
			sub_ret = cb_load_conf_file (buff, sub_ret == 3);
			if (sub_ret < 0) {
				ret = -1;
				configuration_error (conf_file, line, 1,
						    _("Configuration file was included here"));
				break;
			}
		}
		if (sub_ret != 0) ret = sub_ret;
	}
	fclose (fp);

	/* remove current entry from memory and list*/
	if (cc) {
		cc->next = NULL;
	} else {
		conf_includes = NULL;
	}
	cob_free (c);

	return ret;
}

int
cb_load_conf (const char *fname, const int prefix_dir)
{
	const char	*name;
	int		ret;
	size_t		i;
	char		buff[COB_NORMAL_BUFF];

	/* Warn if we drop the configuration read already */
	if (unlikely(cb_config_name != NULL)) {
		configuration_warning (fname, 0,
			_("The previous loaded configuration '%s' will be discarded"), 
			cb_config_name);
	}

	/* Initialize the configuration table */
	for (i = 0; i < CB_CONFIG_SIZE; i++) {
		config_table[i].val = NULL;
	}

	/* Get the name for the configuration file */
	if (prefix_dir) {
		snprintf (buff, (size_t)COB_NORMAL_MAX,
			  "%s%c%s", cob_config_dir, SLASH_CHAR, fname);
		name = buff;
	} else {
		name = fname;
	}

	ret = cb_load_conf_file (name, 0);

	/* Checks for missing definitions */
	if (ret == 0) {
		for (i = 4U; i < CB_CONFIG_SIZE; i++) {
			if (config_table[i].val == NULL) {
				/* as there are likely more than one definition missing group it */
				if (ret == 0) {
					configuration_error (fname, 0, 1, _("Missing definitions:"));
				}
				configuration_error (fname, 0, 1, _("\tNo definition of '%s'"),
						config_table[i].name);
				ret = -1;
			}
		}
	}

	return ret;
}
