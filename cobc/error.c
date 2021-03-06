/*
   Copyright (C) 2001-2012, 2014-2016 Free Software Foundation, Inc.
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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#include "cobc.h"
#include "tree.h"

static char		*errnamebuff = NULL;
static struct cb_label	*last_section = NULL;
static struct cb_label	*last_paragraph = NULL;

static int conf_error_displayed = 0;
static int last_error_line = 0;
static const char	*last_error_file = "Unknown";


size_t				cb_msg_style;

static void
print_error (const char *file, int line, const char *prefix,
	     const char *fmt, va_list ap)
{
	char			errmsg[BUFSIZ];
	struct list_error	*err;
	struct list_files	*cfile;
	
	if (!file) {
		file = cb_source_file;
	}
	if (!line) {
		line = cb_source_line;
	}

	/* Print section and/or paragraph name */
	if (current_section != last_section) {
		if (current_section && !current_section->flag_dummy_section) {
			if(file)
				fprintf (stderr, "%s: ", file);
			fputs (_("In section"), stderr);
			fprintf (stderr, " '%s':\n",
				(char *)current_section->name);
		}
		last_section = current_section;
	}
	if (current_paragraph != last_paragraph) {
		if (current_paragraph && !current_paragraph->flag_dummy_paragraph) {
			if(file)
				fprintf (stderr, "%s: ", file);
			fputs (_("In paragraph"), stderr);
			fprintf (stderr, " '%s':\n",
				(char *)current_paragraph->name);
		}
		last_paragraph = current_paragraph;
	}

	/* Print the error */
	if (file) {
		if (cb_msg_style == CB_MSG_STYLE_MSC) {
			fprintf (stderr, "%s (%d): ", file, line);
		} else {
			fprintf (stderr, "%s: %d: ", file, line);
		}
	}
	if (prefix) {
		fprintf (stderr, "%s", prefix);
	}
	vsprintf (errmsg, fmt, ap);
	fprintf (stderr, "%s\n", errmsg);

	if (cb_src_list_file) {
		err = cobc_malloc (sizeof (struct list_error));
		memset (err, 0, sizeof (struct list_error));
		err->line = line;
		strcpy (err->prefix, prefix);
		strcpy (err->msg, errmsg);
		cfile = cb_current_file;
		if (strcmp (cfile->name, file)) {
			cfile = cfile->copy_head;
			while (cfile) {
				if (!strcmp (cfile->name, file)) {
					break;
				}
				cfile = cfile->next;
			}
		}
		if (cfile) {
			if (cfile->err_tail) {
				cfile->err_tail->next = err;
			}
			if (!cfile->err_head) {
				cfile->err_head = err;
			}
			cfile->err_tail = err;
		}
	}
}

void
cb_warning (const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	print_error (NULL, 0, _("Warning: "), fmt, ap);
	va_end (ap);
	warningcount++;
}

void
cb_error (const char *fmt, ...)
{
	va_list ap;

	cobc_in_repository = 0;
#if	0	/* RXWRXW - Is this right? */
	cobc_cs_check = 0;
#endif
	va_start (ap, fmt);
	print_error (NULL, 0, _("Error: "), fmt, ap);
	va_end (ap);
	if (++errorcount > 100) {
		cobc_too_many_errors ();
	}
}

/* Warning/error for pplex.l input routine */
/* At this stage we have not parsed the newline so that */
/* cb_source_line needs to be adjusted by newline_count in pplex.l */

void
cb_plex_warning (const size_t sline, const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	print_error (NULL, (int)(cb_source_line + sline), _("Warning: "), fmt, ap);
	va_end (ap);
	warningcount++;
}

void
cb_plex_error (const size_t sline, const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	print_error (NULL, (int)(cb_source_line + sline), _("Error: "), fmt, ap);
	va_end (ap);
	if (++errorcount > 100) {
		cobc_too_many_errors ();
	}
}

/* Warning/Error for config.c */
void
configuration_warning (const char *fname, const int line, const char *fmt, ...)
{
	va_list args;

	conf_error_displayed = 0;
	fputs (_("Configuration Warning"), stderr);
	fputs (": ", stderr);


	/* Prefix */
	if (fname != last_error_file
		|| line != last_error_line) {
		last_error_file = fname;
		last_error_line = line;
		if (fname) {
			fprintf (stderr, "%s: ", fname);
		} else {
			fputs ("cb_conf: ", stderr);
		}
		if (line) {
			fprintf (stderr, "%d: ", line);
		}
	}

	/* Body */
	va_start(args, fmt);
	vfprintf (stderr, fmt, args);
	va_end(args);

	/* Postfix */
	putc('\n', stderr);
	fflush(stderr);
}
void
configuration_error (const char *fname, const int line, const int finish_error, const char *fmt, ...)
{
	va_list args;

	if (!conf_error_displayed) {
		conf_error_displayed = 1;
		fputs (_("Configuration Error"), stderr);
		putc ('\n', stderr);
	}


	/* Prefix */
	if (fname != last_error_file
		|| line != last_error_line) {
		last_error_file = fname;
		last_error_line = line;
		if (fname) {
			fprintf (stderr, "%s: ", fname);
		} else {
			fputs ("cb_conf: ", stderr);
		}
		if (line) {
			fprintf (stderr, "%d: ", line);
		}
	}

	/* Body */
	va_start(args, fmt);
	vfprintf (stderr, fmt, args);
	va_end(args);

	/* Postfix */
	if (!finish_error) {
		putc(';', stderr);
		putc('\n', stderr);
		putc('\t', stderr);
	} else {
		putc('\n', stderr);
		fflush(stderr);
	}
}

/* Generic warning/error routines */
void
cb_warning_x (cb_tree x, const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	print_error (x->source_file, x->source_line, _("Warning: "), fmt, ap);
	va_end (ap);
	warningcount++;
}

void
cb_error_x (cb_tree x, const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	print_error (x->source_file, x->source_line, _("Error: "), fmt, ap);
	va_end (ap);
	if (++errorcount > 100) {
		cobc_too_many_errors ();
	}
}

unsigned int
cb_verify (const enum cb_support tag, const char *feature)
{
	switch (tag) {
	case CB_OK:
		return 1;
	case CB_WARNING:
		cb_warning (_("%s used"), feature);
		return 1;
	case CB_ARCHAIC:
		if (cb_warn_archaic) {
			cb_warning (_("%s is archaic in %s"), feature, cb_config_name);
		}
		return 1;
	case CB_OBSOLETE:
		if (cb_warn_obsolete) {
			cb_warning (_("%s is obsolete in %s"), feature, cb_config_name);
		}
		return 1;
	case CB_SKIP:
		return 0;
	case CB_IGNORE:
		if (warningopt) {
			cb_warning (_("%s ignored"), feature);
		}
		return 0;
	case CB_ERROR:
		cb_error (_("%s used"), feature);
		return 0;
	case CB_UNCONFORMABLE:
		cb_error (_("%s does not conform to %s"), feature, cb_config_name);
		return 0;
	default:
		break;
	}
	return 0;
}

void
redefinition_error (cb_tree x)
{
	struct cb_word	*w;

	w = CB_REFERENCE (x)->word;
	cb_error_x (x, _("Redefinition of '%s'"), w->name);
	if (w->items) {
		cb_error_x (CB_VALUE (w->items),
			    _("'%s' previously defined here"), w->name);
	}
}

void
redefinition_warning (cb_tree x, cb_tree y)
{
	struct cb_word	*w;
	cb_tree		z;

	w = CB_REFERENCE (x)->word;
	cb_warning_x (x, _("Redefinition of '%s'"), w->name);
	z = NULL;
	if (y) {
		z = y;
	} else if (w->items) {
		z = CB_VALUE (w->items);
	}

	if (z) {
		cb_warning_x (z, _("'%s' previously defined here"), w->name);
	}
}

void
undefined_error (cb_tree x)
{	
	struct cb_reference	*r = CB_REFERENCE (x);
	cb_tree			c;
        void (* const emit_error_func)(cb_tree, const char *, ...)
		= r->flag_optional ? &cb_warning_x : &cb_error_x;
	const char		*error_message;
	
	if (!errnamebuff) {
		errnamebuff = cobc_main_malloc ((size_t)COB_NORMAL_BUFF);
	}

	/* Get complete variable name */
	snprintf (errnamebuff, (size_t)COB_NORMAL_MAX, "'%s'", CB_NAME (x));
	errnamebuff[COB_NORMAL_MAX] = 0;
	for (c = r->chain; c; c = CB_REFERENCE (c)->chain) {
		strcat (errnamebuff, " in '");
		strcat (errnamebuff, CB_NAME (c));
		strcat (errnamebuff, "'");
	}

	if (is_reserved_word (CB_NAME (x))) {
		error_message = _("%s cannot be used here");
	} else if (is_default_reserved_word (CB_NAME (x))) {
		error_message = _("%s is not defined, but is a reserved word in another dialect");
	} else {
		error_message = _("%s is not defined");
	} 
	
	(*emit_error_func) (x, error_message, errnamebuff);
}

void
ambiguous_error (cb_tree x)
{
	struct cb_word	*w;
	struct cb_field	*p;
	struct cb_label	*l2;
	cb_tree		l;
	cb_tree		y;

	w = CB_REFERENCE (x)->word;
	if (w->error == 0) {
		if (!errnamebuff) {
			errnamebuff = cobc_main_malloc ((size_t)COB_NORMAL_BUFF);
		}
		/* Display error the first time */
		snprintf (errnamebuff, (size_t)COB_NORMAL_MAX, "'%s'", CB_NAME (x));
		errnamebuff[COB_NORMAL_MAX] = 0;
		for (l = CB_REFERENCE (x)->chain; l; l = CB_REFERENCE (l)->chain) {
			strcat (errnamebuff, " in '");
			strcat (errnamebuff, CB_NAME (l));
			strcat (errnamebuff, "'");
		}
		cb_error_x (x, _("%s ambiguous; need qualification"), errnamebuff);
		w->error = 1;

		/* Display all fields with the same name */
		for (l = w->items; l; l = CB_CHAIN (l)) {
			y = CB_VALUE (l);
			snprintf (errnamebuff, (size_t)COB_NORMAL_MAX,
				  "'%s' ", w->name);
			errnamebuff[COB_NORMAL_MAX] = 0;
			switch (CB_TREE_TAG (y)) {
			case CB_TAG_FIELD:
				for (p = CB_FIELD (y)->parent; p; p = p->parent) {
					strcat (errnamebuff, "in '");
					strcat (errnamebuff, cb_name (CB_TREE(p)));
					strcat (errnamebuff, "' ");
				}
				break;
			case CB_TAG_LABEL:
				l2 = CB_LABEL (y);
				if (l2->section) {
					strcat (errnamebuff, "in '");
					strcat (errnamebuff,
						(const char *)(l2->section->name));
					strcat (errnamebuff, "' ");
				}
				break;
			default:
				break;
			}
			strcat (errnamebuff, _("defined here"));
			cb_error_x (y, "%s", errnamebuff);
		}
	}
}

void
group_error (cb_tree x, const char *clause)
{
	cb_error_x (x, _("Group item '%s' cannot have %s clause"),
		    cb_name (x), clause);
}

void
level_redundant_error (cb_tree x, const char *clause)
{
	const char		*s;
	const struct cb_field	*f;

	s = cb_name (x);
	f = CB_FIELD_PTR (x);
	if (f->flag_item_78) {
		cb_error_x (x, _("Constant item '%s' cannot have a %s clause"),
			    s, clause);
	} else {
		cb_error_x (x, _("Level %02d item '%s' cannot have a %s clause"),
			    f->level,
			    s, clause);
	}
}

void
level_require_error (cb_tree x, const char *clause)
{
	const char		*s;
	const struct cb_field	*f;

	s = cb_name (x);
	f = CB_FIELD_PTR (x);
	if (f->flag_item_78) {
		cb_error_x (x, _("Constant item '%s' requires a %s clause"),
			    s, clause);
	} else {
		cb_error_x (x, _("Level %02d item '%s' requires a %s clause"),
			    f->level,
			    s, clause);
	}
}

void
level_except_error (cb_tree x, const char *clause)
{
	const char		*s;
	const struct cb_field	*f;

	s = cb_name (x);
	f = CB_FIELD_PTR (x);
	if (f->flag_item_78) {
		cb_error_x (x, _("Constant item '%s' can only have a %s clause"),
			    s, clause);
	} else {
		cb_error_x (x, _("Level %02d item '%s' can only have a %s clause"),
			    f->level,
			    s, clause);
	}
}
