/**
 * \file conf.c
 * \ingroup Configuration
 * \brief Configuration helper functions
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 2000-2001
 *
 * Tree based, full nesting configuration functions.
 *
 * See the \ref conf page for more details.
 */
/*
 *  Configuration helper functions
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>,
 *			  Jaroslav Kysela <perex@perex.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*! \page conf Configuration files

<P>Configuration files use a simple format allowing modern
data description like nesting and array assignments.</P>

\section conf_whitespace Whitespace

Whitespace is the collective name given to spaces (blanks), horizontal and
vertical tabs, newline characters, and comments. Whitespace can
indicate where configuration tokens start and end, but beyond this function,
any surplus whitespace is discarded. For example, the two sequences

\code
  a 1 b 2
\endcode

and

\code
  a 1 
     b 2
\endcode

are lexically equivalent and parse identically to give the four tokens:

\code
a
1
b
2
\endcode

The ASCII characters representing whitespace can occur within literal
strings, in which case they are protected from the normal parsing process
(they remain as part of the string). For example:

\code
  name "John Smith"
\endcode

parses to two tokens, including the single literal-string token "John
Smith".

\section conf_linesplicing Line continuation with \

A special case occurs if a newline character in a string is preceded
by a backslash (\). The backslash and the new line are both discarded,
allowing two physical lines of text to be treated as one unit.

\code
"John \
Smith"
\endcode

is parsed as "John Smith".

\section conf_comments Comments

A single-line comment begins with the character #. The comment can start
at any position, and extends to the end of the line.

\code
  a 1  # this is a comment
\endcode

\section conf_include Including configuration files

To include another configuration file, write the file name in angle brackets.
The prefix \c confdir: will reference the global configuration directory.

\code
</etc/alsa1.conf>
<confdir:pcm/surround.conf>
\endcode

\section conf_punctuators Punctuators

The configuration punctuators (also known as separators) are:

\code
  {} [] , ; = . ' " new-line form-feed carriage-return whitespace
\endcode

\subsection conf_braces Braces

Opening and closing braces { } indicate the start and end of a compound
statement:

\code
a {
  b 1
}
\endcode

\subsection conf_brackets Brackets

Opening and closing brackets indicate a single array definition. The
identifiers are automatically generated starting with zero.

\code
a [
  "first"
  "second"
]
\endcode

The above code is equal to

\code
a.0 "first"
a.1 "second"
\endcode

\subsection conf_comma_semicolon Comma and semicolon

The comma (,) or semicolon (;) can separate value assignments. It is not
strictly required to use these separators because whitespace suffices to
separate tokens.

\code
a 1;
b 1,
\endcode

\subsection conf_equal Equal sign

The equal sign (=) can separate variable declarations from
initialization lists:

\code
a=1
b=2
\endcode

Using equal signs is not required because whitespace suffices to separate
tokens.

\section conf_assigns Assignments

The configuration file defines id (key) and value pairs. The id (key) can be
composed from ASCII digits, characters from a to z and A to Z, and the
underscore (_). The value can be either a string, an integer, a real number,
or a compound statement.

\subsection conf_single Single assignments

\code
a 1	# is equal to
a=1	# is equal to
a=1;	# is equal to
a 1,
\endcode

\subsection conf_compound Compound assignments (definitions using braces)

\code
a {
  b = 1
}
a={
  b 1,
}
\endcode

\section conf_compound1 Compound assignments (one key definitions)

\code
a.b 1
a.b=1
\endcode

\subsection conf_array Array assignments (definitions using brackets)

\code
a [
  "first"
  "second"
]
\endcode

\subsection conf_array1 Array assignments (one key definitions)

\code
a.0 "first"
a.1 "second"
\endcode

\section conf_mode Operation modes for parsing nodes

By default, the node operation mode is 'merge+create', i.e., if
a configuration node is not present a new one is created, otherwise
the latest assignment is merged (if possible - type checking). The
'merge+create' operation mode is specified with the prefix character plus (+).

The operation mode 'merge' merges the node with the old one (which must
exist). Type checking is done, so strings cannot be assigned to integers
and so on. This mode is specified with the prefix character minus (-).

The operation mode 'do not override' ignores a new configuration node
if a configuration node with the same name exists. This mode is specified with
the prefix character question mark (?).

The operation mode 'override' always overrides the old configuration node
with new contents. This mode is specified with the prefix character
exclamation mark (!).

\code
defaults.pcm.!device 1
\endcode

\section conf_syntax_summary Syntax summary

\code
# Configuration file syntax

# Include a new configuration file
<filename>

# Simple assignment
name [=] value [,|;]

# Compound assignment (first style)
name [=] {
        name1 [=] value [,|;]
        ...
}

# Compound assignment (second style)
name.name1 [=] value [,|;]

# Array assignment (first style)
name [
        value0 [,|;]
        value1 [,|;]
        ...
]

# Array assignment (second style)
name.0 [=] value0 [,|;]
name.1 [=] value1 [,|;]
\endcode

\section conf_syntax_ref References

\ref confarg
\ref conffunc
\ref confhooks

*/

/*! \page confarg Runtime arguments in configuration files

<P>The ALSA library can accept runtime arguments for some configuration
blocks. This extension is built on top of the basic configuration file
syntax.<P>

\section confarg_define Defining arguments

Arguments are defined using the id (key) \c \@args and array values containing
the string names of the arguments:

\code
@args [ CARD ]	# or
@args.0 CARD
\endcode

\section confarg_type Defining argument types and default values

An argument's type is specified with the id (key) \c \@args and the argument
name. The type and the default value are specified in the compound block:

\code
@args.CARD {
  type string
  default "abcd"
}
\endcode

\section confarg_refer Referring to arguments

Arguments are referred to with a dollar-sign ($) and the name of the argument:

\code
  card $CARD
\endcode

\section confarg_math simple math expressions

The simple math expressions are identified using a unix shell like expression syntax
with a dollar-sign ($) and bracket ([):

\code
  card "$[$CARD + 1]"
\endcode

\section confarg_usage Usage

To use a block with arguments, write the argument values after the key,
separated with a colon (:). For example, all these names for PCM interfaces
give the same result:

\code
hw:0,1
hw:CARD=0,DEV=1
hw:{CARD 0 DEV 1}
plug:"hw:0,1"
plug:{SLAVE="hw:{CARD 0 DEV 1}"}
\endcode

As you see, arguments can be specified in their proper order or by name.
Note that arguments enclosed in braces are parsed in the same way as in
configuration files, but using the override method by default.

\section confarg_example Example

\code
pcm.demo {
	@args [ CARD DEVICE ]
	@args.CARD {
		type string
		default "supersonic"
	}
	@args.DEVICE {
		type integer
		default 0
	}
	type hw
	card $CARD
	device $DEVICE
}
\endcode


*/

/*! \page conffunc Runtime functions in configuration files

<P>The ALSA library can modify the configuration at runtime.
Several built-in functions are available.</P>

<P>A function is defined with the id \c \@func and the function name. All other
values in the current compound are used as configuration for the function.
If the compound func.\<function_name\> is defined in the root node, then the
library and function from this compound configuration are used, otherwise
'snd_func_' is prefixed to the string and code from the ALSA library is used.
The definition of a function looks like:</P> 

\code
func.remove_first_char {
	lib "/usr/lib/libasoundextend.so"
	func "extend_remove_first_char"
}
\endcode

*/

/*! \page confhooks Hooks in configuration files

<P>The hook extension in the ALSA library allows expansion of configuration
nodes at run-time. The existence of a hook is determined by the
presence of a \@hooks compound node.</P>

<P>This example defines a hook which loads two configuration files at the
beginning:</P>

\code
@hooks [
	{
		func load
		files [
			"/etc/asound.conf"
			"~/.asoundrc"
		]
		errors false
	}
]
\endcode

\section confhooks_ref Function reference

<UL>
  <LI>The function load - \c snd_config_hook_load() - loads and parses the
      given configuration files.
  <LI>The function load_for_all_cards - \c snd_config_hook_load_for_all_cards() -
      loads and parses the given configuration files for each installed sound
      card. The driver name (the type of the sound card) is passed in the
      private configuration node.
</UL>

*/


#include "local.h"
#include <stdarg.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <locale.h>
#ifdef HAVE_LIBPTHREAD
#include <pthread.h>
#endif

#ifndef DOC_HIDDEN

#ifdef HAVE_LIBPTHREAD
static pthread_mutex_t snd_config_update_mutex;
static pthread_once_t snd_config_update_mutex_once = PTHREAD_ONCE_INIT;
#endif

struct _snd_config {
	char *id;
	snd_config_type_t type;
	int refcount; /* default = 0 */
	union {
		long integer;
		long long integer64;
		char *string;
		double real;
		const void *ptr;
		struct {
			struct list_head fields;
			bool join;
		} compound;
	} u;
	struct list_head list;
	snd_config_t *parent;
	int hop;
};

struct filedesc {
	char *name;
	snd_input_t *in;
	unsigned int line, column;
	struct filedesc *next;

	/* list of the include paths (configuration directories),
	 * defined by <searchdir:relative-path/to/top-alsa-conf-dir>,
	 * for searching its included files.
	 */
	struct list_head include_paths;
};

/* path to search included files */
struct include_path {
	char *dir;
	struct list_head list;
};

#define LOCAL_ERROR			(-0x68000000)

#define LOCAL_UNTERMINATED_STRING 	(LOCAL_ERROR - 0)
#define LOCAL_UNTERMINATED_QUOTE	(LOCAL_ERROR - 1)
#define LOCAL_UNEXPECTED_CHAR		(LOCAL_ERROR - 2)
#define LOCAL_UNEXPECTED_EOF		(LOCAL_ERROR - 3)

typedef struct {
	struct filedesc *current;
	int unget;
	int ch;
} input_t;

#ifdef HAVE_LIBPTHREAD

static void snd_config_init_mutex(void)
{
	pthread_mutexattr_t attr;

	pthread_mutexattr_init(&attr);
#ifdef HAVE_PTHREAD_MUTEX_RECURSIVE
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
#endif
	pthread_mutex_init(&snd_config_update_mutex, &attr);
	pthread_mutexattr_destroy(&attr);
}

static inline void snd_config_lock(void)
{
	pthread_once(&snd_config_update_mutex_once, snd_config_init_mutex);
	pthread_mutex_lock(&snd_config_update_mutex);
}

static inline void snd_config_unlock(void)
{
	pthread_mutex_unlock(&snd_config_update_mutex);
}

#else

static inline void snd_config_lock(void) { }
static inline void snd_config_unlock(void) { }

#endif

/*
 * Add a diretory to the paths to search included files.
 * param fd -  File object that owns these paths to search files included by it.
 * param dir - Path of the directory to add. Allocated externally and need to
*              be freed manually later.
 * return - Zero if successful, otherwise a negative error code.
 *
 * The direcotry should be a subdiretory of top configuration directory
 * "/usr/share/alsa/".
 */
static int add_include_path(struct filedesc *fd, const char *dir)
{
	struct include_path *path;
	struct filedesc *fd1;
	struct list_head *pos;

	/* check, if dir is already registered (also in parents) */
	for (fd1 = fd; fd1; fd1 = fd1->next) {
		list_for_each(pos, &fd1->include_paths) {
			path = list_entry(pos, struct include_path, list);
			if (strcmp(path->dir, dir) == 0)
				return 0;
		}
	}

	path = calloc(1, sizeof(*path));
	if (!path)
		return -ENOMEM;

	path->dir = strdup(dir);
	if (path->dir == NULL) {
		free(path);
		return -ENOMEM;
	}

	list_add_tail(&path->list, &fd->include_paths);
	return 0;
}

/*
 * Free all include paths of a file descriptor.
 * param fd - File object that owns these paths to search files included by it.
 */
static void free_include_paths(struct filedesc *fd)
{
	struct list_head *pos, *npos, *base;
	struct include_path *path;

	base = &fd->include_paths;
	list_for_each_safe(pos, npos, base) {
		path = list_entry(pos, struct include_path, list);
		list_del(&path->list);
		if (path->dir)
			free(path->dir);
		free(path);
	}
}

/**
 * \brief Returns the default top-level config directory
 * \return The top-level config directory path string
 *
 * This function returns the string of the top-level config directory path.
 * If the path is specified via the environment variable \c ALSA_CONFIG_DIR
 * and the value is a valid path, it returns this value.  If unspecified, it
 * returns the default value, "/usr/share/alsa".
 */
const char *snd_config_topdir(void)
{
	static char *topdir;

	if (!topdir) {
		topdir = getenv("ALSA_CONFIG_DIR");
		if (!topdir || *topdir != '/' || strlen(topdir) >= PATH_MAX)
			topdir = ALSA_CONFIG_DIR;
	}
	return topdir;
}

static char *_snd_config_path(const char *name)
{
	const char *root = snd_config_topdir();
	char *path = malloc(strlen(root) + strlen(name) + 2);
	if (!path)
		return NULL;
	sprintf(path, "%s/%s", root, name);
	return path;
}

/*
 * Search and open a file, and creates a new input object reading from the file.
 * param inputp - The functions puts the pointer to the new input object
 *               at the address specified by \p inputp.
 * param file - Name of the configuration file.
 * param include_paths - Optional, addtional directories to search the file.
 * return - Zero if successful, otherwise a negative error code.
 *
 * This function will search and open the file in the following order
 * of priority:
 * 1. directly open the file by its name (only if absolute)
 * 2. search for the file name in in additional configuration directories
 *    specified by users, via alsaconf syntax
 *    <searchdir:relative-path/to/user/share/alsa>;
 *    These directories should be subdirectories of /usr/share/alsa.
 */
static int input_stdio_open(snd_input_t **inputp, const char *file,
			    struct filedesc *current)
{
	struct list_head *pos;
	struct include_path *path;
	char full_path[PATH_MAX];
	int err;

	if (file[0] == '/')
		return snd_input_stdio_open(inputp, file, "r");

	/* search file in user specified include paths. These directories
	 * are subdirectories of /usr/share/alsa.
	 */
	err = -ENOENT;
	while (current) {
		list_for_each(pos, &current->include_paths) {
			path = list_entry(pos, struct include_path, list);
			if (!path->dir)
				continue;

			snprintf(full_path, PATH_MAX, "%s/%s", path->dir, file);
			err = snd_input_stdio_open(inputp, full_path, "r");
			if (err == 0)
				return 0;
		}
		current = current->next;
	}

	return err;
}

int _snd_safe_strtoll_base(const char *str, long long *val, int base)
{
	char *end;
	long v;
	if (!*str)
		return -EINVAL;
	errno = 0;
	v = strtoll(str, &end, base);
	if (errno)
		return -errno;
	if (*end)
		return -EINVAL;
	*val = v;
	return 0;
}

int _snd_safe_strtol_base(const char *str, long *val, int base)
{
	char *end;
	long v;
	if (!*str)
		return -EINVAL;
	errno = 0;
	v = strtol(str, &end, base);
	if (errno)
		return -errno;
	if (*end)
		return -EINVAL;
	*val = v;
	return 0;
}

int _snd_safe_strtod(const char *str, double *val)
{
	char *end;
	double v;
#ifdef HAVE_USELOCALE
	locale_t saved_locale, c_locale;
#else
	char *saved_locale;
	char locstr[64]; /* enough? */
#endif
	int err;

	if (!*str)
		return -EINVAL;
#ifdef HAVE_USELOCALE
	c_locale = newlocale(LC_NUMERIC_MASK, "C", 0);
	saved_locale = uselocale(c_locale);
#else
	saved_locale = setlocale(LC_NUMERIC, NULL);
	if (saved_locale) {
		snprintf(locstr, sizeof(locstr), "%s", saved_locale);
		setlocale(LC_NUMERIC, "C");
	}
#endif
	errno = 0;
	v = strtod(str, &end);
	err = -errno;
#ifdef HAVE_USELOCALE
	if (c_locale != (locale_t)0) {
		uselocale(saved_locale);
		freelocale(c_locale);
	}
#else
	if (saved_locale)
		setlocale(LC_NUMERIC, locstr);
#endif
	if (err)
		return err;
	if (*end)
		return -EINVAL;
	*val = v;
	return 0;
}

static int get_char(input_t *input)
{
	int c;
	struct filedesc *fd;
	if (input->unget) {
		input->unget = 0;
		return input->ch;
	}
 again:
	fd = input->current;
	c = snd_input_getc(fd->in);
	switch (c) {
	case '\n':
		fd->column = 0;
		fd->line++;
		break;
	case '\t':
		fd->column += 8 - fd->column % 8;
		break;
	case EOF:
		if (fd->next) {
			snd_input_close(fd->in);
			free(fd->name);
			input->current = fd->next;
			free(fd);
			goto again;
		}
		return LOCAL_UNEXPECTED_EOF;
	default:
		fd->column++;
		break;
	}
	return (unsigned char)c;
}

static void unget_char(int c, input_t *input)
{
	assert(!input->unget);
	input->ch = c;
	input->unget = 1;
}

static int get_delimstring(char **string, int delim, input_t *input);

static int get_char_skip_comments(input_t *input)
{
	int c;
	while (1) {
		c = get_char(input);
		if (c == '<') {
			char *str;
			snd_input_t *in;
			struct filedesc *fd;
			DIR *dirp;
			int err = get_delimstring(&str, '>', input);
			if (err < 0)
				return err;

			if (!strncmp(str, "searchdir:", 10)) {
				/* directory to search included files */
				char *tmp = _snd_config_path(str + 10);
				free(str);
				if (tmp == NULL)
					return -ENOMEM;
				str = tmp;

				dirp = opendir(str);
				if (!dirp) {
					SNDERR("Invalid search dir %s", str);
					free(str);
					return -EINVAL;
				}
				closedir(dirp);

				err = add_include_path(input->current, str);
				if (err < 0) {
					SNDERR("Cannot add search dir %s", str);
					free(str);
					return err;
				}
				free(str);
				continue;
			}

			if (!strncmp(str, "confdir:", 8)) {
				/* file in the specified directory */
				char *tmp = _snd_config_path(str + 8);
				free(str);
				if (tmp == NULL)
					return -ENOMEM;
				str = tmp;
				err = snd_input_stdio_open(&in, str, "r");
			} else { /* absolute or relative file path */
				err = input_stdio_open(&in, str, input->current);
			}

			if (err < 0) {
				SNDERR("Cannot access file %s", str);
				free(str);
				return err;
			}
			fd = malloc(sizeof(*fd));
			if (!fd) {
				free(str);
				return -ENOMEM;
			}
			fd->name = str;
			fd->in = in;
			fd->next = input->current;
			fd->line = 1;
			fd->column = 0;
			INIT_LIST_HEAD(&fd->include_paths);
			input->current = fd;
			continue;
		}
		if (c != '#')
			break;
		while (1) {
			c = get_char(input);
			if (c < 0)
				return c;
			if (c == '\n')
				break;
		}
	}
		
	return c;
}
			

static int get_nonwhite(input_t *input)
{
	int c;
	while (1) {
		c = get_char_skip_comments(input);
		switch (c) {
		case ' ':
		case '\f':
		case '\t':
		case '\n':
		case '\r':
			break;
		default:
			return c;
		}
	}
}

static inline int get_hexachar(input_t *input)
{
	int c, num = 0;

	c = get_char(input);
	if (c >= '0' && c <= '9') num |= (c - '0') << 4;
	else if (c >= 'a' && c <= 'f') num |= (c - 'a') << 4;
	else if (c >= 'A' && c <= 'F') num |= (c - 'A') << 4;
	c = get_char(input);
	if (c >= '0' && c <= '9') num |= (c - '0') << 0;
	else if (c >= 'a' && c <= 'f') num |= (c - 'a') << 0;
	else if (c >= 'A' && c <= 'F') num |= (c - 'A') << 0;
	return num;
}

static int get_quotedchar(input_t *input)
{
	int c;
	c = get_char(input);
	switch (c) {
	case 'n':
		return '\n';
	case 't':
		return '\t';
	case 'v':
		return '\v';
	case 'b':
		return '\b';
	case 'r':
		return '\r';
	case 'f':
		return '\f';
	case 'x':
		return get_hexachar(input);
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
	{
		int num = c - '0';
		int i = 1;
		do {
			c = get_char(input);
			if (c < '0' || c > '7') {
				unget_char(c, input);
				break;
			}
			num = num * 8 + c - '0';
			i++;
		} while (i < 3);
		return num;
	}
	default:
		return c;
	}
}

#define LOCAL_STR_BUFSIZE	64
struct local_string {
	char *buf;
	size_t alloc;
	size_t idx;
	char tmpbuf[LOCAL_STR_BUFSIZE];
};

static void init_local_string(struct local_string *s)
{
	memset(s, 0, sizeof(*s));
	s->buf = s->tmpbuf;
	s->alloc = LOCAL_STR_BUFSIZE;
}

static void free_local_string(struct local_string *s)
{
	if (s->buf != s->tmpbuf)
		free(s->buf);
}

static int add_char_local_string(struct local_string *s, int c)
{
	if (s->idx >= s->alloc) {
		size_t nalloc = s->alloc * 2;
		if (s->buf == s->tmpbuf) {
			s->buf = malloc(nalloc);
			if (s->buf == NULL)
				return -ENOMEM;
			memcpy(s->buf, s->tmpbuf, s->alloc);
		} else {
			char *ptr = realloc(s->buf, nalloc);
			if (ptr == NULL)
				return -ENOMEM;
			s->buf = ptr;
		}
		s->alloc = nalloc;
	}
	s->buf[s->idx++] = c;
	return 0;
}

static char *copy_local_string(struct local_string *s)
{
	char *dst = malloc(s->idx + 1);
	if (dst) {
		memcpy(dst, s->buf, s->idx);
		dst[s->idx] = '\0';
	}
	return dst;
}

static int get_freestring(char **string, int id, input_t *input)
{
	struct local_string str;
	int c;

	init_local_string(&str);
	while (1) {
		c = get_char(input);
		if (c < 0) {
			if (c == LOCAL_UNEXPECTED_EOF) {
				*string = copy_local_string(&str);
				if (! *string)
					c = -ENOMEM;
				else
					c = 0;
			}
			break;
		}
		switch (c) {
		case '.':
			if (!id)
				break;
			/* fall through */
		case ' ':
		case '\f':
		case '\t':
		case '\n':
		case '\r':
		case '=':
		case ',':
		case ';':
		case '{':
		case '}':
		case '[':
		case ']':
		case '\'':
		case '"':
		case '\\':
		case '#':
			*string = copy_local_string(&str);
			if (! *string)
				c = -ENOMEM;
			else {
				unget_char(c, input);
				c = 0;
			}
			goto _out;
		default:
			break;
		}
		if (add_char_local_string(&str, c) < 0) {
			c = -ENOMEM;
			break;
		}
	}
 _out:
	free_local_string(&str);
	return c;
}
			
static int get_delimstring(char **string, int delim, input_t *input)
{
	struct local_string str;
	int c;

	init_local_string(&str);
	while (1) {
		c = get_char(input);
		if (c < 0)
			break;
		if (c == '\\') {
			c = get_quotedchar(input);
			if (c < 0)
				break;
			if (c == '\n')
				continue;
		} else if (c == delim) {
			*string = copy_local_string(&str);
			if (! *string)
				c = -ENOMEM;
			else
				c = 0;
			break;
		}
		if (add_char_local_string(&str, c) < 0) {
			c = -ENOMEM;
			break;
		}
	}
	 free_local_string(&str);
	 return c;
}

/* Return 0 for free string, 1 for delimited string */
static int get_string(char **string, int id, input_t *input)
{
	int c = get_nonwhite(input), err;
	if (c < 0)
		return c;
	switch (c) {
	case '=':
	case ',':
	case ';':
	case '.':
	case '{':
	case '}':
	case '[':
	case ']':
	case '\\':
		return LOCAL_UNEXPECTED_CHAR;
	case '\'':
	case '"':
		err = get_delimstring(string, c, input);
		if (err < 0)
			return err;
		return 1;
	default:
		unget_char(c, input);
		err = get_freestring(string, id, input);
		if (err < 0)
			return err;
		return 0;
	}
}

static int _snd_config_make(snd_config_t **config, char **id, snd_config_type_t type)
{
	snd_config_t *n;
	assert(config);
	n = calloc(1, sizeof(*n));
	if (n == NULL) {
		if (*id) {
			free(*id);
			*id = NULL;
		}
		return -ENOMEM;
	}
	if (id) {
		n->id = *id;
		*id = NULL;
	}
	n->type = type;
	if (type == SND_CONFIG_TYPE_COMPOUND)
		INIT_LIST_HEAD(&n->u.compound.fields);
	*config = n;
	return 0;
}
	

static int _snd_config_make_add(snd_config_t **config, char **id,
				snd_config_type_t type, snd_config_t *parent)
{
	snd_config_t *n;
	int err;
	assert(parent->type == SND_CONFIG_TYPE_COMPOUND);
	err = _snd_config_make(&n, id, type);
	if (err < 0)
		return err;
	n->parent = parent;
	list_add_tail(&n->list, &parent->u.compound.fields);
	*config = n;
	return 0;
}

static int _snd_config_search(snd_config_t *config, 
			      const char *id, int len, snd_config_t **result)
{
	snd_config_iterator_t i, next;
	snd_config_for_each(i, next, config) {
		snd_config_t *n = snd_config_iterator_entry(i);
		if (len < 0) {
			if (strcmp(n->id, id) != 0)
				continue;
		} else if (strlen(n->id) != (size_t) len ||
			   memcmp(n->id, id, (size_t) len) != 0)
				continue;
		if (result)
			*result = n;
		return 0;
	}
	return -ENOENT;
}

static int parse_value(snd_config_t **_n, snd_config_t *parent, input_t *input, char **id, int skip)
{
	snd_config_t *n = *_n;
	char *s;
	int err;

	err = get_string(&s, 0, input);
	if (err < 0)
		return err;
	if (skip) {
		free(s);
		return 0;
	}
	if (err == 0 && ((s[0] >= '0' && s[0] <= '9') || s[0] == '-')) {
		long long i;
		errno = 0;
		err = safe_strtoll(s, &i);
		if (err < 0) {
			double r;
			err = safe_strtod(s, &r);
			if (err >= 0) {
				free(s);
				if (n) {
					if (n->type != SND_CONFIG_TYPE_REAL) {
						SNDERR("%s is not a real", *id);
						return -EINVAL;
					}
				} else {
					err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_REAL, parent);
					if (err < 0)
						return err;
				}
				n->u.real = r;
				*_n = n;
				return 0;
			}
		} else {
			free(s);
			if (n) {
				if (n->type != SND_CONFIG_TYPE_INTEGER && n->type != SND_CONFIG_TYPE_INTEGER64) {
					SNDERR("%s is not an integer", *id);
					return -EINVAL;
				}
			} else {
				if (i <= INT_MAX) 
					err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_INTEGER, parent);
				else
					err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_INTEGER64, parent);
				if (err < 0)
					return err;
			}
			if (n->type == SND_CONFIG_TYPE_INTEGER) 
				n->u.integer = (long) i;
			else 
				n->u.integer64 = i;
			*_n = n;
			return 0;
		}
	}
	if (n) {
		if (n->type != SND_CONFIG_TYPE_STRING) {
			SNDERR("%s is not a string", *id);
			free(s);
			return -EINVAL;
		}
	} else {
		err = _snd_config_make_add(&n, id, SND_CONFIG_TYPE_STRING, parent);
		if (err < 0)
			return err;
	}
	free(n->u.string);
	n->u.string = s;
	*_n = n;
	return 0;
}

static int parse_defs(snd_config_t *parent, input_t *input, int skip, int override);
static int parse_array_defs(snd_config_t *farther, input_t *input, int skip, int override);

static int parse_array_def(snd_config_t *parent, input_t *input, int *idx, int skip, int override)
{
	char *id = NULL;
	int c;
	int err;
	snd_config_t *n = NULL;

	if (!skip) {
		snd_config_t *g;
		char static_id[12];
		while (1) {
			snprintf(static_id, sizeof(static_id), "%i", *idx);
			if (_snd_config_search(parent, static_id, -1, &g) == 0) {
				if (override) {
					snd_config_delete(n);
				} else {
					/* merge */
					(*idx)++;
					continue;
				}
			}
			break;
		}
		id = strdup(static_id);
		if (id == NULL)
			return -ENOMEM;
	}
	c = get_nonwhite(input);
	if (c < 0) {
		err = c;
		goto __end;
	}
	switch (c) {
	case '{':
	case '[':
	{
		char endchr;
		if (!skip) {
			if (n) {
				if (n->type != SND_CONFIG_TYPE_COMPOUND) {
					SNDERR("%s is not a compound", id);
					err = -EINVAL;
					goto __end;
				}
			} else {
				err = _snd_config_make_add(&n, &id, SND_CONFIG_TYPE_COMPOUND, parent);
				if (err < 0)
					goto __end;
			}
		}
		if (c == '{') {
			err = parse_defs(n, input, skip, override);
			endchr = '}';
		} else {
			err = parse_array_defs(n, input, skip, override);
			endchr = ']';
		}
		c = get_nonwhite(input);
		if (c < 0) {
			err = c;
			goto __end;
		}
		if (c != endchr) {
			if (n)
				snd_config_delete(n);
			err = LOCAL_UNEXPECTED_CHAR;
			goto __end;
		}
		break;
	}
	default:
		unget_char(c, input);
		err = parse_value(&n, parent, input, &id, skip);
		if (err < 0)
			goto __end;
		break;
	}
	err = 0;
      __end:
	free(id);
      	return err;
}

static int parse_array_defs(snd_config_t *parent, input_t *input, int skip, int override)
{
	int idx = 0;
	while (1) {
		int c = get_nonwhite(input), err;
		if (c < 0)
			return c;
		unget_char(c, input);
		if (c == ']')
			return 0;
		err = parse_array_def(parent, input, &idx, skip, override);
		if (err < 0)
			return err;
		idx++;
	}
	return 0;
}

static int parse_def(snd_config_t *parent, input_t *input, int skip, int override)
{
	char *id = NULL;
	int c;
	int err;
	snd_config_t *n;
	enum {MERGE_CREATE, MERGE, OVERRIDE, DONT_OVERRIDE} mode;
	while (1) {
		c = get_nonwhite(input);
		if (c < 0)
			return c;
		switch (c) {
		case '+':
			mode = MERGE_CREATE;
			break;
		case '-':
			mode = MERGE;
			break;
		case '?':
			mode = DONT_OVERRIDE;
			break;
		case '!':
			mode = OVERRIDE;
			break;
		default:
			mode = !override ? MERGE_CREATE : OVERRIDE;
			unget_char(c, input);
		}
		err = get_string(&id, 1, input);
		if (err < 0)
			return err;
		c = get_nonwhite(input);
		if (c != '.')
			break;
		if (skip) {
			free(id);
			continue;
		}
		if (_snd_config_search(parent, id, -1, &n) == 0) {
			if (mode == DONT_OVERRIDE) {
				skip = 1;
				free(id);
				continue;
			}
			if (mode != OVERRIDE) {
				if (n->type != SND_CONFIG_TYPE_COMPOUND) {
					SNDERR("%s is not a compound", id);
					return -EINVAL;
				}
				n->u.compound.join = true;
				parent = n;
				free(id);
				continue;
			}
			snd_config_delete(n);
		}
		if (mode == MERGE) {
			SNDERR("%s does not exists", id);
			err = -ENOENT;
			goto __end;
		}
		err = _snd_config_make_add(&n, &id, SND_CONFIG_TYPE_COMPOUND, parent);
		if (err < 0)
			goto __end;
		n->u.compound.join = true;
		parent = n;
	}
	if (c == '=') {
		c = get_nonwhite(input);
		if (c < 0)
			return c;
	}
	if (!skip) {
		if (_snd_config_search(parent, id, -1, &n) == 0) {
			if (mode == DONT_OVERRIDE) {
				skip = 1;
				n = NULL;
			} else if (mode == OVERRIDE) {
				snd_config_delete(n);
				n = NULL;
			}
		} else {
			n = NULL;
			if (mode == MERGE) {
				SNDERR("%s does not exists", id);
				err = -ENOENT;
				goto __end;
			}
		}
	}
	switch (c) {
	case '{':
	case '[':
	{
		char endchr;
		if (!skip) {
			if (n) {
				if (n->type != SND_CONFIG_TYPE_COMPOUND) {
					SNDERR("%s is not a compound", id);
					err = -EINVAL;
					goto __end;
				}
			} else {
				err = _snd_config_make_add(&n, &id, SND_CONFIG_TYPE_COMPOUND, parent);
				if (err < 0)
					goto __end;
			}
		}
		if (c == '{') {
			err = parse_defs(n, input, skip, override);
			endchr = '}';
		} else {
			err = parse_array_defs(n, input, skip, override);
			endchr = ']';
		}
		c = get_nonwhite(input);
		if (c != endchr) {
			if (n)
				snd_config_delete(n);
			err = LOCAL_UNEXPECTED_CHAR;
			goto __end;
		}
		break;
	}
	default:
		unget_char(c, input);
		err = parse_value(&n, parent, input, &id, skip);
		if (err < 0)
			goto __end;
		break;
	}
	c = get_nonwhite(input);
	switch (c) {
	case ';':
	case ',':
		break;
	default:
		unget_char(c, input);
	}
      __end:
	free(id);
	return err;
}
		
static int parse_defs(snd_config_t *parent, input_t *input, int skip, int override)
{
	int c, err;
	while (1) {
		c = get_nonwhite(input);
		if (c < 0)
			return c == LOCAL_UNEXPECTED_EOF ? 0 : c;
		unget_char(c, input);
		if (c == '}')
			return 0;
		err = parse_def(parent, input, skip, override);
		if (err < 0)
			return err;
	}
	return 0;
}

static void string_print(char *str, int id, snd_output_t *out)
{
	int q;
	unsigned char *p = (unsigned char *)str;
	if (!p || !*p) {
		snd_output_puts(out, "''");
		return;
	}
	if (!id) {
		switch (*p) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		case '-':
			goto quoted;
		}
	}
 loop:
	switch (*p) {
	case 0:
		goto nonquoted;
	case ' ':
	case '=':
	case ';':
	case ',':
	case '.':
	case '{':
	case '}':
	case '[':
	case ']':
	case '\'':
	case '"':
	case '*':
	case '#':
		goto quoted;
	default:
		if (*p <= 31 || *p >= 127)
			goto quoted;
		p++;
		goto loop;
	}
 nonquoted:
	snd_output_puts(out, str);
	return;
 quoted:
	q = strchr(str, '\'') ? '"' : '\'';
	snd_output_putc(out, q);
	p = (unsigned char *)str;
	while (*p) {
		int c;
		c = *p;
		switch (c) {
		case '\n':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 'n');
			break;
		case '\t':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 't');
			break;
		case '\v':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 'v');
			break;
		case '\b':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 'b');
			break;
		case '\r':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 'r');
			break;
		case '\f':
			snd_output_putc(out, '\\');
			snd_output_putc(out, 'f');
			break;
		default:
			if (c == q) {
				snd_output_putc(out, '\\');
				snd_output_putc(out, c);
			} else {
				if (c >= 32 && c <= 126)
					snd_output_putc(out, c);
				else
					snd_output_printf(out, "\\%04o", c);
			}
			break;
		}
		p++;
	}
	snd_output_putc(out, q);
}

static void level_print(snd_output_t *out, unsigned int level)
{
	char a[level + 1];
	memset(a, '\t', level);
	a[level] = '\0';
	snd_output_puts(out, a);
}

static int _snd_config_save_children(snd_config_t *config, snd_output_t *out,
				     unsigned int level, unsigned int joins,
				     int array);

int _snd_config_save_node_value(snd_config_t *n, snd_output_t *out,
				unsigned int level)
{
	int err, array;
	switch (n->type) {
	case SND_CONFIG_TYPE_INTEGER:
		snd_output_printf(out, "%ld", n->u.integer);
		break;
	case SND_CONFIG_TYPE_INTEGER64:
		snd_output_printf(out, "%lld", n->u.integer64);
		break;
	case SND_CONFIG_TYPE_REAL:
		snd_output_printf(out, "%-16g", n->u.real);
		break;
	case SND_CONFIG_TYPE_STRING:
		string_print(n->u.string, 0, out);
		break;
	case SND_CONFIG_TYPE_POINTER:
		SNDERR("cannot save runtime pointer type");
		return -EINVAL;
	case SND_CONFIG_TYPE_COMPOUND:
		array = snd_config_is_array(n);
		snd_output_putc(out, array ? '[' : '{');
		snd_output_putc(out, '\n');
		err = _snd_config_save_children(n, out, level + 1, 0, array);
		if (err < 0)
			return err;
		level_print(out, level);
		snd_output_putc(out, array ? ']' : '}');
		break;
	}
	return 0;
}

static void id_print(snd_config_t *n, snd_output_t *out, unsigned int joins)
{
	if (joins > 0) {
		assert(n->parent);
		id_print(n->parent, out, joins - 1);
		snd_output_putc(out, '.');
	}
	string_print(n->id, 1, out);
}

static int _snd_config_save_children(snd_config_t *config, snd_output_t *out,
				     unsigned int level, unsigned int joins,
				     int array)
{
	int err;
	snd_config_iterator_t i, next;
	assert(config && out);
	snd_config_for_each(i, next, config) {
		snd_config_t *n = snd_config_iterator_entry(i);
		if (n->type == SND_CONFIG_TYPE_COMPOUND &&
		    n->u.compound.join) {
			err = _snd_config_save_children(n, out, level, joins + 1, 0);
			if (err < 0)
				return err;
			continue;
		}
		level_print(out, level);
		if (!array) {
			id_print(n, out, joins);
			snd_output_putc(out, ' ');
#if 0
			snd_output_putc(out, '=');
#endif
		}
		err = _snd_config_save_node_value(n, out, level);
		if (err < 0)
			return err;
#if 0
		snd_output_putc(out, ';');
#endif
		snd_output_putc(out, '\n');
	}
	return 0;
}
#endif


/**
 * \brief Substitutes one configuration node to another.
 * \param dst Handle to the destination node.
 * \param src Handle to the source node. Must not be the same as \a dst.
 * \return Zero if successful, otherwise a negative error code.
 *
 * If both nodes are compounds, the source compound node members will
 * be moved to the destination compound node. The original destination
 * compound node members will be deleted (overwritten).
 *
 * If the destination node is a compound and the source node is
 * an ordinary type, the compound members are deleted (including
 * their contents).
 *
 * Otherwise, the source node's value replaces the destination node's
 * value.
 *
 * In any case, a successful call to this function frees the source
 * node.
 */
int snd_config_substitute(snd_config_t *dst, snd_config_t *src)
{
	assert(dst && src);
	if (dst->type == SND_CONFIG_TYPE_COMPOUND) {
		int err = snd_config_delete_compound_members(dst);
		if (err < 0)
			return err;
	}
	if (dst->type == SND_CONFIG_TYPE_COMPOUND &&
	    src->type == SND_CONFIG_TYPE_COMPOUND) {	/* overwrite */
		snd_config_iterator_t i, next;
		snd_config_for_each(i, next, src) {
			snd_config_t *n = snd_config_iterator_entry(i);
			n->parent = dst;
		}
		src->u.compound.fields.next->prev = &dst->u.compound.fields;
		src->u.compound.fields.prev->next = &dst->u.compound.fields;
	}
	free(dst->id);
	if (dst->type == SND_CONFIG_TYPE_STRING)
		free(dst->u.string);
	dst->id = src->id;
	dst->type = src->type;
	dst->u = src->u;
	free(src);
	return 0;
}

/**
 * \brief Converts an ASCII string to a configuration node type.
 * \param[in] ascii A string containing a configuration node type.
 * \param[out] type The node type corresponding to \a ascii.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function recognizes at least the following node types:
 * <dl>
 * <dt>integer<dt>#SND_CONFIG_TYPE_INTEGER
 * <dt>integer64<dt>#SND_CONFIG_TYPE_INTEGER64
 * <dt>real<dt>#SND_CONFIG_TYPE_REAL
 * <dt>string<dt>#SND_CONFIG_TYPE_STRING
 * <dt>compound<dt>#SND_CONFIG_TYPE_COMPOUND
 * </dl>
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>Unknown note type in \a type.
 * </dl>
 */
int snd_config_get_type_ascii(const char *ascii, snd_config_type_t *type)
{
	assert(ascii && type);
	if (!strcmp(ascii, "integer")) {
		*type = SND_CONFIG_TYPE_INTEGER;
		return 0;
	}
	if (!strcmp(ascii, "integer64")) {
		*type = SND_CONFIG_TYPE_INTEGER64;
		return 0;
	}
	if (!strcmp(ascii, "real")) {
		*type = SND_CONFIG_TYPE_REAL;
		return 0;
	}
	if (!strcmp(ascii, "string")) {
		*type = SND_CONFIG_TYPE_STRING;
		return 0;
	}
	if (!strcmp(ascii, "compound")) {
		*type = SND_CONFIG_TYPE_COMPOUND;
		return 0;
	}
	return -EINVAL;
}

/**
 * \brief Returns the type of a configuration node.
 * \param config Handle to the configuration node.
 * \return The node's type.
 *
 * \par Conforming to:
 * LSB 3.2
 */
snd_config_type_t snd_config_get_type(const snd_config_t *config)
{
	return config->type;
}

static int check_array_item(const char *id, int index)
{
	const char *p;
	long val;

	for (p = id; *p; p++) {
		if (*p < '0' || *p > '9')
			return 0;
	}

	if (safe_strtol(id, &val))
		return 0;
	return val == index;
}

/**
 * \brief Returns if the compound is an array (and count of items).
 * \param config Handle to the configuration node.
 * \return A count of items in array, zero when the compound is not an array,
 *         otherwise a negative error code.
 */
int snd_config_is_array(const snd_config_t *config)
{
	int idx;
	snd_config_iterator_t i, next;
	snd_config_t *node;

	assert(config);
	if (config->type != SND_CONFIG_TYPE_COMPOUND)
		return -EINVAL;
	idx = 0;
	snd_config_for_each(i, next, config) {
		node = snd_config_iterator_entry(i);
		if (!check_array_item(node->id, idx))
			return 0;
		idx++;
	}
	return idx;
}

/**
 * \brief Returns if the compound has no fields (is empty).
 * \param config Handle to the configuration node.
 * \return A positive value when true, zero when false, otherwise a negative error code.
 */
int snd_config_is_empty(const snd_config_t *config)
{
	assert(config);
	if (config->type != SND_CONFIG_TYPE_COMPOUND)
		return -EINVAL;
	return list_empty(&config->u.compound.fields);
}

/**
 * \brief Returns the id of a configuration node.
 * \param[in] config Handle to the configuration node.
 * \param[out] id The function puts the pointer to the id string at the
 *                address specified by \a id.
 * \return Zero if successful, otherwise a negative error code.
 *
 * The returned string is owned by the configuration node; the application
 * must not modify or delete it, and the string becomes invalid when the
 * node's id changes or when the node is freed.
 *
 * If the node does not have an id, \a *id is set to \c NULL.
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_get_id(const snd_config_t *config, const char **id)
{
	assert(config && id);
	*id = config->id;
	return 0;
}

/**
 * \brief Sets the id of a configuration node.
 * \param config Handle to the configuration node.
 * \param id The new node id, must not be \c NULL.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function stores a copy of \a id in the node.
 *
 * \par Errors:
 * <dl>
 * <dt>-EEXIST<dd>One of \a config's siblings already has the id \a id.
 * <dt>-EINVAL<dd>The id of a node with a parent cannot be set to \c NULL.
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 */
int snd_config_set_id(snd_config_t *config, const char *id)
{
	snd_config_iterator_t i, next;
	char *new_id;
	assert(config);
	if (id) {
		if (config->parent) {
			snd_config_for_each(i, next, config->parent) {
				snd_config_t *n = snd_config_iterator_entry(i);
				if (n != config && strcmp(id, n->id) == 0)
					return -EEXIST;
			}
		}
		new_id = strdup(id);
		if (!new_id)
			return -ENOMEM;
	} else {
		if (config->parent)
			return -EINVAL;
		new_id = NULL;
	}
	free(config->id);
	config->id = new_id;
	return 0;
}

/**
 * \brief Creates a top level configuration node.
 * \param[out] config Handle to the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * The returned node is an empty compound node without a parent and
 * without an id.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_top(snd_config_t **config)
{
	assert(config);
	return _snd_config_make(config, 0, SND_CONFIG_TYPE_COMPOUND);
}

#ifndef DOC_HIDDEN
int _snd_config_load_with_include(snd_config_t *config, snd_input_t *in,
				  int override, const char * const *include_paths)
{
	int err;
	input_t input;
	struct filedesc *fd, *fd_next;

	assert(config && in);
	fd = malloc(sizeof(*fd));
	if (!fd)
		return -ENOMEM;
	fd->name = NULL;
	fd->in = in;
	fd->line = 1;
	fd->column = 0;
	fd->next = NULL;
	INIT_LIST_HEAD(&fd->include_paths);
	if (include_paths) {
		for (; *include_paths; include_paths++) {
			err = add_include_path(fd, *include_paths);
			if (err < 0)
				goto _end;
		}
	} else {
		err = add_include_path(fd, snd_config_topdir());
		if (err < 0)
			goto _end;
	}
	input.current = fd;
	input.unget = 0;
	err = parse_defs(config, &input, 0, override);
	fd = input.current;
	if (err < 0) {
		const char *str;
		switch (err) {
		case LOCAL_UNTERMINATED_STRING:
			str = "Unterminated string";
			err = -EINVAL;
			break;
		case LOCAL_UNTERMINATED_QUOTE:
			str = "Unterminated quote";
			err = -EINVAL;
			break;
		case LOCAL_UNEXPECTED_CHAR:
			str = "Unexpected char";
			err = -EINVAL;
			break;
		case LOCAL_UNEXPECTED_EOF:
			str = "Unexpected end of file";
			err = -EINVAL;
			break;
		default:
			str = strerror(-err);
			break;
		}
		SNDERR("%s:%d:%d:%s", fd->name ? fd->name : "_toplevel_", fd->line, fd->column, str);
		goto _end;
	}
	err = get_char(&input);
	fd = input.current;
	if (err != LOCAL_UNEXPECTED_EOF) {
		SNDERR("%s:%d:%d:Unexpected }", fd->name ? fd->name : "", fd->line, fd->column);
		err = -EINVAL;
		goto _end;
	}
	err = 0;
 _end:
	while (fd->next) {
		fd_next = fd->next;
		snd_input_close(fd->in);
		free(fd->name);
		free_include_paths(fd);
		free(fd);
		fd = fd_next;
	}

	free_include_paths(fd);
	free(fd);
	return err;
}
#endif

/**
 * \brief Loads a configuration tree.
 * \param config Handle to a top level configuration node.
 * \param in Input handle to read the configuration from.
 * \return Zero if successful, otherwise a negative error code.
 *
 * The definitions loaded from the input are added to \a config, which
 * must be a compound node.
 *
 * \par Errors:
 * Any errors encountered when parsing the input or returned by hooks or
 * functions.
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_load(snd_config_t *config, snd_input_t *in)
{
	return _snd_config_load_with_include(config, in, 0, NULL);
}

/**
 * \brief Loads a configuration tree from a string.
 * \param[out] The function puts the handle to the configuration
 *	       node loaded from the file(s) at the address specified
 *             by \a config.
 * \param[in] s String with the ASCII configuration
 * \param[in] size String size, if zero, a C string is expected (with termination)
 * \return Zero if successful, otherwise a negative error code.
 *
 * The definitions loaded from the string are put to \a config, which
 * is created as a new top node.
 *
 * \par Errors:
 * Any errors encountered when parsing the input or returned by hooks or
 * functions.
 */
int snd_config_load_string(snd_config_t **config, const char *s, size_t size)
{
	snd_input_t *input;
	snd_config_t *dst;
	int err;

	assert(config && s);
	if (size == 0)
		size = strlen(s);
	err = snd_input_buffer_open(&input, s, size);
	if (err < 0)
		return err;
	err = snd_config_top(&dst);
	if (err < 0) {
		snd_input_close(input);
		return err;
	}
	err = snd_config_load(dst, input);
	snd_input_close(input);
	if (err < 0) {
		snd_config_delete(dst);
		return err;
	}
	*config = dst;
	return 0;
}

/**
 * \brief Loads a configuration tree and overrides existing configuration nodes.
 * \param config Handle to a top level configuration node.
 * \param in Input handle to read the configuration from.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function loads definitions from \a in into \a config like
 * #snd_config_load, but the default mode for input nodes is 'override'
 * (!) instead of 'merge+create' (+).
 */
int snd_config_load_override(snd_config_t *config, snd_input_t *in)
{
	return _snd_config_load_with_include(config, in, 1, NULL);
}

/**
 * \brief Adds a child to a compound configuration node.
 * \param parent Handle to a compound configuration node.
 * \param child Handle to the configuration node to be added.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function makes the node \a child a child of the node \a parent.
 *
 * The parent node then owns the child node, i.e., the child node gets
 * deleted together with its parent.
 *
 * \a child must have an id.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a child does not have an id.
 * <dt>-EINVAL<dd>\a child already has a parent.
 * <dt>-EEXIST<dd>\a parent already contains a child node with the same
 *                id as \a child.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_add(snd_config_t *parent, snd_config_t *child)
{
	snd_config_iterator_t i, next;
	assert(parent && child);
	if (!child->id || child->parent)
		return -EINVAL;
	snd_config_for_each(i, next, parent) {
		snd_config_t *n = snd_config_iterator_entry(i);
		if (strcmp(child->id, n->id) == 0)
			return -EEXIST;
	}
	child->parent = parent;
	list_add_tail(&child->list, &parent->u.compound.fields);
	return 0;
}

/**
 * \brief Adds a child after another child configuration node.
 * \param after Handle to the start configuration node.
 * \param child Handle to the configuration node to be added.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function makes the node \a child a child of the parent of
 * the node \a after.
 *
 * The parent node then owns the child node, i.e., the child node gets
 * deleted together with its parent.
 *
 * \a child must have an id.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a child does not have an id.
 * <dt>-EINVAL<dd>\a child already has a parent.
 * <dt>-EEXIST<dd>\a parent already contains a child node with the same
 *                id as \a child.
 * </dl>
 */
int snd_config_add_after(snd_config_t *after, snd_config_t *child)
{
	snd_config_iterator_t i, next;
	snd_config_t *parent;
	assert(after && child);
	parent = after->parent;
	assert(parent);
	if (!child->id || child->parent)
		return -EINVAL;
	snd_config_for_each(i, next, parent) {
		snd_config_t *n = snd_config_iterator_entry(i);
		if (strcmp(child->id, n->id) == 0)
			return -EEXIST;
	}
	child->parent = parent;
	list_insert(&child->list, &after->list, after->list.next);
	return 0;
}

/**
 * \brief Adds a child before another child configuration node.
 * \param before Handle to the start configuration node.
 * \param child Handle to the configuration node to be added.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function makes the node \a child a child of the parent of
 * the node \a before.
 *
 * The parent node then owns the child node, i.e., the child node gets
 * deleted together with its parent.
 *
 * \a child must have an id.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a child does not have an id.
 * <dt>-EINVAL<dd>\a child already has a parent.
 * <dt>-EEXIST<dd>\a parent already contains a child node with the same
 *                id as \a child.
 * </dl>
 */
int snd_config_add_before(snd_config_t *before, snd_config_t *child)
{
	snd_config_iterator_t i, next;
	snd_config_t *parent;
	assert(before && child);
	parent = before->parent;
	assert(parent);
	if (!child->id || child->parent)
		return -EINVAL;
	snd_config_for_each(i, next, parent) {
		snd_config_t *n = snd_config_iterator_entry(i);
		if (strcmp(child->id, n->id) == 0)
			return -EEXIST;
	}
	child->parent = parent;
	list_insert(&child->list, before->list.prev, &before->list);
	return 0;
}

/*
 * append all src items to the end of dst arrray
 */
static int _snd_config_array_merge(snd_config_t *dst, snd_config_t *src, int index)
{
	snd_config_iterator_t si, snext;
	int err;

	snd_config_for_each(si, snext, src) {
		snd_config_t *sn = snd_config_iterator_entry(si);
		char id[16];
		snd_config_remove(sn);
		snprintf(id, sizeof(id), "%d", index++);
		err = snd_config_set_id(sn, id);
		if (err < 0) {
			snd_config_delete(sn);
			return err;
		}
		sn->parent = dst;
		list_add_tail(&sn->list, &dst->u.compound.fields);
	}
	snd_config_delete(src);
	return 0;
}

/**
 * \brief In-place merge of two config handles
 * \param dst[out] Config handle for the merged contents
 * \param src[in] Config handle to merge into dst (may be NULL)
 * \param override[in] Override flag
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function merges all fields from the source compound to the destination compound.
 * When the \a override flag is set, the related subtree in \a dst is replaced from \a src.
 *
 * When \a override is not set, the child compounds are traversed and merged.
 *
 * The configuration elements other than compounds are always substituted (overwritten)
 * from the \a src config handle.
 *
 * The src handle is deleted.
 *
 * Note: On error, config handles may be modified.
 *
 * \par Errors:
 * <dl>
 * <dt>-EEXIST<dd>identifier already exists (!override)
 * <dt>-ENOMEM<dd>not enough memory
 * </dl>
 */
int snd_config_merge(snd_config_t *dst, snd_config_t *src, int override)
{
	snd_config_iterator_t di, si, dnext, snext;
	bool found;
	int err, array;

	assert(dst);
	if (src == NULL)
		return 0;
	if (dst->type != SND_CONFIG_TYPE_COMPOUND || src->type != SND_CONFIG_TYPE_COMPOUND)
		return snd_config_substitute(dst, src);
	array = snd_config_is_array(dst);
	if (array && snd_config_is_array(src))
		return _snd_config_array_merge(dst, src, array);
	snd_config_for_each(si, snext, src) {
		snd_config_t *sn = snd_config_iterator_entry(si);
		found = false;
		snd_config_for_each(di, dnext, dst) {
			snd_config_t *dn = snd_config_iterator_entry(di);
			if (strcmp(sn->id, dn->id) == 0) {
				if (override ||
				    sn->type != SND_CONFIG_TYPE_COMPOUND ||
				    dn->type != SND_CONFIG_TYPE_COMPOUND) {
					snd_config_remove(sn);
					err = snd_config_substitute(dn, sn);
					if (err < 0)
						return err;
				} else {
					err = snd_config_merge(dn, sn, 0);
					if (err < 0)
						return err;
				}
				found = true;
				break;
			}
		}
		if (!found) {
			/* move config from src to dst */
			snd_config_remove(sn);
			sn->parent = dst;
			list_add_tail(&sn->list, &dst->u.compound.fields);
		}
	}
	snd_config_delete(src);
	return 0;
}

/**
 * \brief Removes a configuration node from its tree.
 * \param config Handle to the configuration node to be removed.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function makes \a config a top-level node, i.e., if \a config
 * has a parent, then \a config is removed from the list of the parent's
 * children.
 *
 * This functions does \e not free the removed node.
 *
 * \sa snd_config_delete
 */
int snd_config_remove(snd_config_t *config)
{
	assert(config);
	if (config->parent)
		list_del(&config->list);
	config->parent = NULL;
	return 0;
}

/**
 * \brief Frees a configuration node.
 * \param config Handle to the configuration node to be deleted.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function frees a configuration node and all its resources.
 *
 * If the node is a child node, it is removed from the tree before being
 * deleted.
 *
 * If the node is a compound node, its descendants (the whole subtree)
 * are deleted recursively.
 *
 * The function is supposed to be called only for locally copied config
 * trees.  For the global tree, take the reference via #snd_config_update_ref
 * and free it via #snd_config_unref.
 *
 * \par Conforming to:
 * LSB 3.2
 *
 * \sa snd_config_remove
 */
int snd_config_delete(snd_config_t *config)
{
	assert(config);
	if (config->refcount > 0) {
		config->refcount--;
		return 0;
	}
	switch (config->type) {
	case SND_CONFIG_TYPE_COMPOUND:
	{
		int err;
		struct list_head *i;
		i = config->u.compound.fields.next;
		while (i != &config->u.compound.fields) {
			struct list_head *nexti = i->next;
			snd_config_t *child = snd_config_iterator_entry(i);
			err = snd_config_delete(child);
			if (err < 0)
				return err;
			i = nexti;
		}
		break;
	}
	case SND_CONFIG_TYPE_STRING:
		free(config->u.string);
		break;
	default:
		break;
	}
	if (config->parent)
		list_del(&config->list);
	free(config->id);
	free(config);
	return 0;
}

/**
 * \brief Deletes the children of a node.
 * \param config Handle to the compound configuration node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function removes and frees all children of a configuration node.
 *
 * Any compound nodes among the children of \a config are deleted
 * recursively.
 *
 * After a successful call to this function, \a config is an empty
 * compound node.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a compound node.
 * </dl>
 */
int snd_config_delete_compound_members(const snd_config_t *config)
{
	int err;
	struct list_head *i;

	assert(config);
	if (config->type != SND_CONFIG_TYPE_COMPOUND)
		return -EINVAL;
	i = config->u.compound.fields.next;
	while (i != &config->u.compound.fields) {
		struct list_head *nexti = i->next;
		snd_config_t *child = snd_config_iterator_entry(i);
		err = snd_config_delete(child);
		if (err < 0)
			return err;
		i = nexti;
	}
	return 0;
}

/**
 * \brief Creates a configuration node.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \param[in] type The type of the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This functions creates a new node of the specified type.
 * The new node has id \a id, which may be \c NULL.
 *
 * The value of the new node is zero (for numbers), or \c NULL (for
 * strings and pointers), or empty (for compound nodes).
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 */
int snd_config_make(snd_config_t **config, const char *id,
		    snd_config_type_t type)
{
	char *id1;
	assert(config);
	if (id) {
		id1 = strdup(id);
		if (!id1)
			return -ENOMEM;
	} else
		id1 = NULL;
	return _snd_config_make(config, &id1, type);
}

/**
 * \brief Creates an integer configuration node.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new node of type #SND_CONFIG_TYPE_INTEGER and
 * with value \c 0.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 *
 * \sa snd_config_imake_integer
 */
int snd_config_make_integer(snd_config_t **config, const char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_INTEGER);
}

/**
 * \brief Creates a 64-bit-integer configuration node.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new node of type #SND_CONFIG_TYPE_INTEGER64
 * and with value \c 0.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 *
 * \sa snd_config_imake_integer64
 */
int snd_config_make_integer64(snd_config_t **config, const char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_INTEGER64);
}

/**
 * \brief Creates a real number configuration node.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new node of type #SND_CONFIG_TYPE_REAL and
 * with value \c 0.0.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \sa snd_config_imake_real
 */
int snd_config_make_real(snd_config_t **config, const char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_REAL);
}

/**
 * \brief Creates a string configuration node.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new node of type #SND_CONFIG_TYPE_STRING and
 * with value \c NULL.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 *
 * \sa snd_config_imake_string
 */
int snd_config_make_string(snd_config_t **config, const char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_STRING);
}

/**
 * \brief Creates a pointer configuration node.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new node of type #SND_CONFIG_TYPE_POINTER and
 * with value \c NULL.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \sa snd_config_imake_pointer
 */
int snd_config_make_pointer(snd_config_t **config, const char *id)
{
	return snd_config_make(config, id, SND_CONFIG_TYPE_POINTER);
}

/**
 * \brief Creates an empty compound configuration node.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \param[in] join Join flag.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new empty node of type
 * #SND_CONFIG_TYPE_COMPOUND.
 *
 * \a join determines how the compound node's id is printed when the
 * configuration is saved to a text file.  For example, if the join flag
 * of compound node \c a is zero, the output will look as follows:
 * \code
 * a {
 *     b "hello"
 *     c 42
 * }
 * \endcode
 * If, however, the join flag of \c a is nonzero, its id will be joined
 * with its children's ids, like this:
 * \code
 * a.b "hello"
 * a.c 42
 * \endcode
 * An \e empty compound node with its join flag set would result in no
 * output, i.e., after saving and reloading the configuration file, that
 * compound node would be lost.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_make_compound(snd_config_t **config, const char *id,
			     int join)
{
	int err;
	err = snd_config_make(config, id, SND_CONFIG_TYPE_COMPOUND);
	if (err < 0)
		return err;
	(*config)->u.compound.join = join;
	return 0;
}

/**
 * \brief Creates an empty compound configuration node in the path.
 * \param[out] config The function puts the handle to the new or
 *		      existing compound node at the address specified
 *		      by \a config.
 * \param[in] root The id of the new node.
 * \param[in] key The id of the new node.
 * \param[in] join Join flag.
 * \param[in] override Override flag.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new empty node of type
 * #SND_CONFIG_TYPE_COMPOUND if the path does not exist. Otherwise,
 * the node from the current configuration tree is returned without
 * any modification. The \a join argument is ignored in this case.
 *
 * \a join determines how the compound node's id is printed when the
 * configuration is saved to a text file.  For example, if the join flag
 * of compound node \c a is zero, the output will look as follows:
 * \code
 * a {
 *     b "hello"
 *     c 42
 * }
 * \endcode
 * If, however, the join flag of \c a is nonzero, its id will be joined
 * with its children's ids, like this:
 * \code
 * a.b "hello"
 * a.c 42
 * \endcode
 * An \e empty compound node with its join flag set would result in no
 * output, i.e., after saving and reloading the configuration file, that
 * compound node would be lost.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * <dt>-EACCESS<dd>Path exists, but it's not a compound (!override)
 * </dl>
 */
int snd_config_make_path(snd_config_t **config, snd_config_t *root,
			 const char *key, int join, int override)
{
	snd_config_t *n;
	const char *p;
	int err;

	while (1) {
		p = strchr(key, '.');
		if (p) {
			err = _snd_config_search(root, key, p - key, &n);
			if (err < 0) {
				size_t l = p - key;
				char *s = malloc(l + 1);
				if (s == NULL)
					return -ENOMEM;
				strncpy(s, key, l);
				s[l] = '\0';
				err = snd_config_make_compound(&n, s, join);
				free(s);
				if (err < 0)
					return err;
				err = snd_config_add(root, n);
				if (err < 0)
					return err;
			}
			root = n;
			key = p + 1;
		} else {
			err = _snd_config_search(root, key, -1, config);
			if (err == 0) {
				if ((*config)->type != SND_CONFIG_TYPE_COMPOUND) {
					if (override) {
						err = snd_config_delete(*config);
						if (err < 0)
							return err;
						goto __make;
					} else {
						return -EACCES;
					}
				}
				return 0;
			}
__make:
			err = snd_config_make_compound(&n, key, join);
			if (err < 0)
				return err;
			err = snd_config_add(root, n);
			if (err < 0)
				return err;
			*config = n;
			return 0;
		}
	}
}

/**
 * \brief Creates an integer configuration node with the given initial value.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \param[in] value The initial value of the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new node of type #SND_CONFIG_TYPE_INTEGER and
 * with value \a value.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_imake_integer(snd_config_t **config, const char *id, const long value)
{
	int err;
	
	err = snd_config_make(config, id, SND_CONFIG_TYPE_INTEGER);
	if (err < 0)
		return err;
	(*config)->u.integer = value;
	return 0;
}

/**
 * \brief Creates a 64-bit-integer configuration node with the given initial value.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \param[in] value The initial value of the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new node of type #SND_CONFIG_TYPE_INTEGER64
 * and with value \a value.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_imake_integer64(snd_config_t **config, const char *id, const long long value)
{
	int err;
	
	err = snd_config_make(config, id, SND_CONFIG_TYPE_INTEGER64);
	if (err < 0)
		return err;
	(*config)->u.integer64 = value;
	return 0;
}

/**
 * \brief Creates a real number configuration node with the given initial value.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \param[in] value The initial value of the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new node of type #SND_CONFIG_TYPE_REAL and
 * with value \a value.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 */
int snd_config_imake_real(snd_config_t **config, const char *id, const double value)
{
	int err;
	
	err = snd_config_make(config, id, SND_CONFIG_TYPE_REAL);
	if (err < 0)
		return err;
	(*config)->u.real = value;
	return 0;
}

/**
 * \brief Creates a string configuration node with the given initial value.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \param[in] value The initial value of the new node.  May be \c NULL.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new node of type #SND_CONFIG_TYPE_STRING and
 * with a copy of the string \c value.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_imake_string(snd_config_t **config, const char *id, const char *value)
{
	int err;
	snd_config_t *tmp;
	
	err = snd_config_make(&tmp, id, SND_CONFIG_TYPE_STRING);
	if (err < 0)
		return err;
	if (value) {
		tmp->u.string = strdup(value);
		if (!tmp->u.string) {
			snd_config_delete(tmp);
			return -ENOMEM;
		}
	} else {
		tmp->u.string = NULL;
	}
	*config = tmp;
	return 0;
}

int snd_config_imake_safe_string(snd_config_t **config, const char *id, const char *value)
{
	int err;
	snd_config_t *tmp;
	char *c;

	err = snd_config_make(&tmp, id, SND_CONFIG_TYPE_STRING);
	if (err < 0)
		return err;
	if (value) {
		tmp->u.string = strdup(value);
		if (!tmp->u.string) {
			snd_config_delete(tmp);
			return -ENOMEM;
		}

		for (c = tmp->u.string; *c; c++) {
			if (*c == ' ' || *c == '-' || *c == '_' ||
				(*c >= '0' && *c <= '9') ||
				(*c >= 'a' && *c <= 'z') ||
				(*c >= 'A' && *c <= 'Z'))
					continue;
			*c = '_';
		}
	} else {
		tmp->u.string = NULL;
	}
	*config = tmp;
	return 0;
}


/**
 * \brief Creates a pointer configuration node with the given initial value.
 * \param[out] config The function puts the handle to the new node at
 *                    the address specified by \a config.
 * \param[in] id The id of the new node.
 * \param[in] value The initial value of the new node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function creates a new node of type #SND_CONFIG_TYPE_POINTER and
 * with value \c value.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 */
int snd_config_imake_pointer(snd_config_t **config, const char *id, const void *value)
{
	int err;
	
	err = snd_config_make(config, id, SND_CONFIG_TYPE_POINTER);
	if (err < 0)
		return err;
	(*config)->u.ptr = value;
	return 0;
}

/**
 * \brief Changes the value of an integer configuration node.
 * \param config Handle to the configuration node.
 * \param value The new value for the node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not an integer node.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_set_integer(snd_config_t *config, long value)
{
	assert(config);
	if (config->type != SND_CONFIG_TYPE_INTEGER)
		return -EINVAL;
	config->u.integer = value;
	return 0;
}

/**
 * \brief Changes the value of a 64-bit-integer configuration node.
 * \param config Handle to the configuration node.
 * \param value The new value for the node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a 64-bit-integer node.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_set_integer64(snd_config_t *config, long long value)
{
	assert(config);
	if (config->type != SND_CONFIG_TYPE_INTEGER64)
		return -EINVAL;
	config->u.integer64 = value;
	return 0;
}

/**
 * \brief Changes the value of a real-number configuration node.
 * \param config Handle to the configuration node.
 * \param value The new value for the node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a real-number node.
 * </dl>
 */
int snd_config_set_real(snd_config_t *config, double value)
{
	assert(config);
	if (config->type != SND_CONFIG_TYPE_REAL)
		return -EINVAL;
	config->u.real = value;
	return 0;
}

/**
 * \brief Changes the value of a string configuration node.
 * \param config Handle to the configuration node.
 * \param value The new value for the node.  May be \c NULL.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function deletes the old string in the node and stores a copy of
 * \a value string in the node.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a string node.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_set_string(snd_config_t *config, const char *value)
{
	char *new_string;
	assert(config);
	if (config->type != SND_CONFIG_TYPE_STRING)
		return -EINVAL;
	if (value) {
		new_string = strdup(value);
		if (!new_string)
			return -ENOMEM;
	} else {
		new_string = NULL;
	}
	free(config->u.string);
	config->u.string = new_string;
	return 0;
}

/**
 * \brief Changes the value of a pointer configuration node.
 * \param config Handle to the configuration node.
 * \param value The new value for the node.  May be \c NULL.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function does not free the old pointer in the node.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a pointer node.
 * </dl>
 */
int snd_config_set_pointer(snd_config_t *config, const void *value)
{
	assert(config);
	if (config->type != SND_CONFIG_TYPE_POINTER)
		return -EINVAL;
	config->u.ptr = value;
	return 0;
}

/**
 * \brief Changes the value of a configuration node.
 * \param config Handle to the configuration node.
 * \param ascii The new value for the node, as an ASCII string.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function changes the node's value to a new value that is parsed
 * from the string \a ascii.  \a ascii must not be \c NULL, not even for
 * a string node.
 *
 * The node's type does not change, i.e., the string must contain a
 * valid value with the same type as the node's type.  For a string
 * node, the node's new value is a copy of \a ascii.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a number or string node.
 * <dt>-EINVAL<dd>The value in \a ascii cannot be parsed.
 * <dt>-ERANGE<dd>The value in \a ascii is too big for the node's type.
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_set_ascii(snd_config_t *config, const char *ascii)
{
	assert(config && ascii);
	switch (config->type) {
	case SND_CONFIG_TYPE_INTEGER:
		{
			long i;
			int err = safe_strtol(ascii, &i);
			if (err < 0)
				return err;
			config->u.integer = i;
		}
		break;
	case SND_CONFIG_TYPE_INTEGER64:
		{
			long long i;
			int err = safe_strtoll(ascii, &i);
			if (err < 0)
				return err;
			config->u.integer64 = i;
		}
		break;
	case SND_CONFIG_TYPE_REAL:
		{
			double d;
			int err = safe_strtod(ascii, &d);
			if (err < 0)
				return err;
			config->u.real = d;
			break;
		}
	case SND_CONFIG_TYPE_STRING:
		{
			char *ptr = strdup(ascii);
			if (ptr == NULL)
				return -ENOMEM;
			free(config->u.string);
			config->u.string = ptr;
		}
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

/**
 * \brief Returns the value of an integer configuration node.
 * \param[in] config Handle to the configuration node.
 * \param[out] ptr The node's value.
 * \return Zero if successful, otherwise a negative error code.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not an integer node.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_get_integer(const snd_config_t *config, long *ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_INTEGER)
		return -EINVAL;
	*ptr = config->u.integer;
	return 0;
}

/**
 * \brief Returns the value of a 64-bit-integer configuration node.
 * \param[in] config Handle to the configuration node.
 * \param[out] ptr The node's value.
 * \return Zero if successful, otherwise a negative error code.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a 64-bit-integer node.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_get_integer64(const snd_config_t *config, long long *ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_INTEGER64)
		return -EINVAL;
	*ptr = config->u.integer64;
	return 0;
}

/**
 * \brief Returns the value of a real-number configuration node.
 * \param[in] config Handle to the configuration node.
 * \param[out] ptr The node's value.
 * \return Zero if successful, otherwise a negative error code.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a real-number node.
 * </dl>
 */
int snd_config_get_real(const snd_config_t *config, double *ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_REAL)
		return -EINVAL;
	*ptr = config->u.real;
	return 0;
}

/**
 * \brief Returns the value of a real or integer configuration node.
 * \param[in] config Handle to the configuration node.
 * \param[out] ptr The node's value.
 * \return Zero if successful, otherwise a negative error code.
 *
 * If the node's type is integer or integer64, the value is converted
 * to the \c double type on the fly.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a number node.
 * </dl>
 */
int snd_config_get_ireal(const snd_config_t *config, double *ptr)
{
	assert(config && ptr);
	if (config->type == SND_CONFIG_TYPE_REAL)
		*ptr = config->u.real;
	else if (config->type == SND_CONFIG_TYPE_INTEGER)
		*ptr = config->u.integer;
	else if (config->type == SND_CONFIG_TYPE_INTEGER64)
		*ptr = config->u.integer64;
	else
		return -EINVAL;
	return 0;
}

/**
 * \brief Returns the value of a string configuration node.
 * \param[in] config Handle to the configuration node.
 * \param[out] ptr The function puts the node's value at the address
 *                 specified by \a ptr.
 * \return Zero if successful, otherwise a negative error code.
 *
 * The returned string is owned by the configuration node; the
 * application must not modify or delete it, and the string becomes
 * invalid when the node's value changes or when the node is freed.
 *
 * The string may be \c NULL.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a string node.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_get_string(const snd_config_t *config, const char **ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_STRING)
		return -EINVAL;
	*ptr = config->u.string;
	return 0;
}

/**
 * \brief Returns the value of a pointer configuration node.
 * \param[in] config Handle to the configuration node.
 * \param[out] ptr The function puts the node's value at the address
 *                 specified by \a ptr.
 * \return Zero if successful, otherwise a negative error code.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a string node.
 * </dl>
 */
int snd_config_get_pointer(const snd_config_t *config, const void **ptr)
{
	assert(config && ptr);
	if (config->type != SND_CONFIG_TYPE_POINTER)
		return -EINVAL;
	*ptr = config->u.ptr;
	return 0;
}

/**
 * \brief Returns the value of a configuration node as a string.
 * \param[in] config Handle to the configuration node.
 * \param[out] ascii The function puts the pointer to the returned
 *                   string at the address specified by \a ascii.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function dynamically allocates the returned string.  The
 * application is responsible for deleting it with \c free() when it is
 * no longer used.
 *
 * For a string node with \c NULL value, the returned string is \c NULL.
 *
 * Supported node types are #SND_CONFIG_TYPE_INTEGER,
 * #SND_CONFIG_TYPE_INTEGER64, #SND_CONFIG_TYPE_REAL, and
 * #SND_CONFIG_TYPE_STRING.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>\a config is not a (64-bit) integer or real number or
 *                string node.
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_get_ascii(const snd_config_t *config, char **ascii)
{
	assert(config && ascii);
	switch (config->type) {
	case SND_CONFIG_TYPE_INTEGER:
		{
			char res[12];
			int err;
			err = snprintf(res, sizeof(res), "%li", config->u.integer);
			if (err < 0 || err == sizeof(res)) {
				assert(0);
				return -ENOMEM;
			}
			*ascii = strdup(res);
		}
		break;
	case SND_CONFIG_TYPE_INTEGER64:
		{
			char res[32];
			int err;
			err = snprintf(res, sizeof(res), "%lli", config->u.integer64);
			if (err < 0 || err == sizeof(res)) {
				assert(0);
				return -ENOMEM;
			}
			*ascii = strdup(res);
		}
		break;
	case SND_CONFIG_TYPE_REAL:
		{
			char res[32];
			int err;
			err = snprintf(res, sizeof(res), "%-16g", config->u.real);
			if (err < 0 || err == sizeof(res)) {
				assert(0);
				return -ENOMEM;
			}
			if (res[0]) {		/* trim the string */
				char *ptr;
				ptr = res + strlen(res) - 1;
				while (ptr != res && *ptr == ' ')
					ptr--;
				if (*ptr != ' ')
					ptr++;
				*ptr = '\0';
			}
			*ascii = strdup(res);
		}
		break;
	case SND_CONFIG_TYPE_STRING:
		if (config->u.string)
			*ascii = strdup(config->u.string);
		else {
			*ascii = NULL;
			return 0;
		}
		break;
	default:
		return -EINVAL;
	}
	if (*ascii == NULL)
		return -ENOMEM;
	return 0;
}

/**
 * \brief Compares the id of a configuration node to a given string.
 * \param config Handle to the configuration node.
 * \param id ASCII id.
 * \return The same value as the result of the \c strcmp function, i.e.,
 *         less than zero if \a config's id is lexicographically less
 *         than \a id, zero if \a config's id is equal to id, greater
 *         than zero otherwise.
 */
int snd_config_test_id(const snd_config_t *config, const char *id)
{
	assert(config && id);
	if (config->id)
		return strcmp(config->id, id);
	else
		return -1;
}

/**
 * \brief Dumps the contents of a configuration node or tree.
 * \param config Handle to the (root) configuration node.
 * \param out Output handle.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function writes a textual representation of \a config's value to
 * the output \a out.
 *
 * \par Errors:
 * <dl>
 * <dt>-EINVAL<dd>A node in the tree has a type that cannot be printed,
 *                i.e., #SND_CONFIG_TYPE_POINTER.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_save(snd_config_t *config, snd_output_t *out)
{
	assert(config && out);
	if (config->type == SND_CONFIG_TYPE_COMPOUND) {
		int array = snd_config_is_array(config);
		return _snd_config_save_children(config, out, 0, 0, array);
	} else {
		return _snd_config_save_node_value(config, out, 0);
	}
}

/*
 *  *** search macros ***
 */

#ifndef DOC_HIDDEN

#define SND_CONFIG_SEARCH(config, key, result, extra_code) \
{ \
	snd_config_t *n; \
	int err; \
	const char *p; \
	assert(config && key); \
	while (1) { \
		if (config->type != SND_CONFIG_TYPE_COMPOUND) \
			return -ENOENT; \
		{ extra_code ; } \
		p = strchr(key, '.'); \
		if (p) { \
			err = _snd_config_search(config, key, p - key, &n); \
			if (err < 0) \
				return err; \
			config = n; \
			key = p + 1; \
		} else \
			return _snd_config_search(config, key, -1, result); \
	} \
}

#define SND_CONFIG_SEARCHA(root, config, key, result, fcn, extra_code) \
{ \
	snd_config_t *n; \
	int err; \
	const char *p; \
	assert(config && key); \
	while (1) { \
		if (config->type != SND_CONFIG_TYPE_COMPOUND) { \
			if (snd_config_get_string(config, &p) < 0) \
				return -ENOENT; \
			err = fcn(root, root, p, &config); \
			if (err < 0) \
				return err; \
		} \
		{ extra_code ; } \
		p = strchr(key, '.'); \
		if (p) { \
			err = _snd_config_search(config, key, p - key, &n); \
			if (err < 0) \
				return err; \
			config = n; \
			key = p + 1; \
		} else \
			return _snd_config_search(config, key, -1, result); \
	} \
}

#define SND_CONFIG_SEARCHV(config, result, fcn) \
{ \
	snd_config_t *n; \
	va_list arg; \
	assert(config); \
	va_start(arg, result); \
	while (1) { \
		const char *k = va_arg(arg, const char *); \
		int err; \
		if (!k) \
			break; \
		err = fcn(config, k, &n); \
		if (err < 0) { \
			va_end(arg); \
			return err; \
		} \
		config = n; \
	} \
	va_end(arg); \
	if (result) \
		*result = n; \
	return 0; \
}

#define SND_CONFIG_SEARCHVA(root, config, result, fcn) \
{ \
	snd_config_t *n; \
	va_list arg; \
	assert(config); \
	va_start(arg, result); \
	while (1) { \
		const char *k = va_arg(arg, const char *); \
		int err; \
		if (!k) \
			break; \
		err = fcn(root, config, k, &n); \
		if (err < 0) { \
			va_end(arg); \
			return err; \
		} \
		config = n; \
	} \
	va_end(arg); \
	if (result) \
		*result = n; \
	return 0; \
}

#define SND_CONFIG_SEARCH_ALIAS(config, base, key, result, fcn1, fcn2) \
{ \
	snd_config_t *res = NULL; \
	char *old_key; \
	int err, first = 1, maxloop = 1000; \
	assert(config && key); \
	while (1) { \
		old_key = strdup(key); \
		if (old_key == NULL) { \
			err = -ENOMEM; \
			res = NULL; \
			break; \
		} \
		err = first && base ? -EIO : fcn1(config, config, key, &res); \
		if (err < 0) { \
			if (!base) \
				break; \
			err = fcn2(config, config, &res, base, key, NULL); \
			if (err < 0) \
				break; \
		} \
		if (snd_config_get_string(res, &key) < 0) \
			break; \
		assert(key); \
		if (!first && (strcmp(key, old_key) == 0 || maxloop <= 0)) { \
			if (maxloop == 0) \
				SNDERR("maximum loop count reached (circular configuration?)"); \
			else \
				SNDERR("key %s refers to itself", key); \
			err = -EINVAL; \
			res = NULL; \
			break; \
		} \
		free(old_key); \
		first = 0; \
		maxloop--; \
	} \
	free(old_key); \
	if (!res) \
		return err; \
	if (result) \
		*result = res; \
	return 0; \
}

#endif /* DOC_HIDDEN */

/**
 * \brief Searches for a node in a configuration tree.
 * \param[in] config Handle to the root of the configuration (sub)tree to search.
 * \param[in] key Search key: one or more node ids, separated with dots.
 * \param[out] result When \a result != \c NULL, the function puts the
 *                    handle to the node found at the address specified
 *                    by \a result.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function searches for a child node of \a config that is
 * identified by \a key, which contains either the id of a direct child
 * node of \a config, or a series of ids, separated with dots, where
 * each id specifies a node that is contained in the previous compound
 * node.
 *
 * In the following example, the comment after each node shows the
 * search key to find that node, assuming that \a config is a handle to
 * the compound node with id \c config:
 * \code
 * config {
 *     a 42               # "a"
 *     b {                # "b"
 *         c "cee"        # "b.c"
 *         d {            # "b.d"
 *             e 2.71828  # "b.d.e"
 *         }
 *     }
 * }
 * \endcode
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOENT<dd>An id in \a key does not exist.
 * <dt>-ENOENT<dd>\a config or one of its child nodes to be searched is
 *                not a compound node.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_search(snd_config_t *config, const char *key, snd_config_t **result)
{
	SND_CONFIG_SEARCH(config, key, result, );
}

/**
 * \brief Searches for a node in a configuration tree, expanding aliases.
 * \param[in] root Handle to the root configuration node containing
 *                 alias definitions.
 * \param[in] config Handle to the root of the configuration (sub)tree to search.
 * \param[in] key Search key: one or more node keys, separated with dots.
 * \param[out] result When \a result != \c NULL, the function puts the
 *                    handle to the node found at the address specified
 *                    by \a result.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This functions searches for a child node of \a config like
 * #snd_config_search.  However, any compound node can also be
 * identified by an alias, which is a string node whose value is taken
 * as the id of a compound node below \a root.
 *
 * \a root must be a compound node.
 * \a root and \a config may be the same node.
 *
 * For example, with the following configuration, the call
 * \code
 * snd_config_searcha(root, config, "a.b.c.d", &result);
 * \endcode
 * would return the node with id \c d:
 * \code
 * config {
 *     a {
 *         b bb
 *     }
 * }
 * root {
 *     bb {
 *         c cc
 *     }
 *     cc ccc
 *     ccc {
 *         d {
 *             x "icks"
 *         }
 *     }
 * }
 * \endcode
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOENT<dd>An id in \a key or an alias id does not exist.
 * <dt>-ENOENT<dd>\a config or one of its child nodes to be searched is
 *                not a compound or string node.
 * </dl>
 */
int snd_config_searcha(snd_config_t *root, snd_config_t *config, const char *key, snd_config_t **result)
{
	SND_CONFIG_SEARCHA(root, config, key, result, snd_config_searcha, );
}

/**
 * \brief Searches for a node in a configuration tree.
 * \param[in] config Handle to the root of the configuration (sub)tree to search.
 * \param[out] result When \a result != \c NULL, the function puts the
 *                    handle to the node found at the address specified
 *                    by \a result.
 * \param[in] ... One or more concatenated dot-separated search keys,
 *                terminated with \c NULL.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This functions searches for a child node of \a config like
 * #snd_config_search, but the search key is the concatenation of all
 * passed search key strings.  For example, the call
 * \code
 * snd_config_searchv(cfg, &res, "a", "b.c", "d.e", NULL);
 * \endcode
 * is equivalent to the call
 * \code
 * snd_config_search(cfg, "a.b.c.d.e", &res);
 * \endcode
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOENT<dd>An id in a search key does not exist.
 * <dt>-ENOENT<dd>\a config or one of its child nodes to be searched is
 *                not a compound node.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_searchv(snd_config_t *config, snd_config_t **result, ...)
{
	SND_CONFIG_SEARCHV(config, result, snd_config_search);
}

/**
 * \brief Searches for a node in a configuration tree, expanding aliases.
 * \param[in] root Handle to the root configuration node containing
 *                 alias definitions.
 * \param[in] config Handle to the root of the configuration (sub)tree to search.
 * \param[out] result When \a result != \c NULL, the function puts the
 *                    handle to the node found at the address specified
 *                    by \a result.
 * \param[in] ... One or more concatenated dot separated search keys,
 *                terminated with \c NULL.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function searches for a child node of \a config, allowing
 * aliases, like #snd_config_searcha, but the search key is the
 * concatenation of all passed seach key strings, like with
 * #snd_config_searchv.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOENT<dd>An id in a search key does not exist.
 * <dt>-ENOENT<dd>\a config or one of its child nodes to be searched is
 *                not a compound or string node.
 * </dl>
 */
int snd_config_searchva(snd_config_t *root, snd_config_t *config, snd_config_t **result, ...)
{
	SND_CONFIG_SEARCHVA(root, config, result, snd_config_searcha);
}

/**
 * \brief Searches for a node in a configuration tree, expanding aliases.
 * \param[in] config Handle to the root of the configuration (sub)tree to search.
 * \param[in] base Search key base, or \c NULL.
 * \param[in] key Search key suffix.
 * \param[out] result When \a result != \c NULL, the function puts the
 *                    handle to the node found at the address specified
 *                    by \a result.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This functions searches for a child node of \a config, allowing
 * aliases, like #snd_config_searcha.  However, alias definitions are
 * searched below \a config (there is no separate \a root parameter),
 * and \a base specifies a seach key that identifies a compound node
 * that is used to search for an alias definitions that is not found
 * directly below \a config and that does not contain a period.  In
 * other words, when \c "id" is not found in \a config, this function
 * also tries \c "base.id".
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOENT<dd>An id in \a key or an alias id does not exist.
 * <dt>-ENOENT<dd>\a config or one of its child nodes to be searched is
 *                not a compound or string node.
 * </dl>
 */
int snd_config_search_alias(snd_config_t *config,
			    const char *base, const char *key,
			    snd_config_t **result)
{
	SND_CONFIG_SEARCH_ALIAS(config, base, key, result,
				snd_config_searcha, snd_config_searchva);
}

static int snd_config_hooks(snd_config_t *config, snd_config_t *private_data);

/**
 * \brief Searches for a node in a configuration tree and expands hooks.
 * \param[in,out] config Handle to the root of the configuration
 *                       (sub)tree to search.
 * \param[in] key Search key: one or more node keys, separated with dots.
 * \param[out] result The function puts the handle to the node found at
 *                    the address specified by \a result.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This functions searches for a child node of \a config like
 * #snd_config_search, but any compound nodes to be searched that
 * contain hooks are modified by the respective hook functions.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOENT<dd>An id in \a key does not exist.
 * <dt>-ENOENT<dd>\a config or one of its child nodes to be searched is
 *                not a compound node.
 * </dl>
 * Additionally, any errors encountered when parsing the hook
 * definitions or returned by the hook functions.
 */
int snd_config_search_hooks(snd_config_t *config, const char *key, snd_config_t **result)
{
	SND_CONFIG_SEARCH(config, key, result, \
					err = snd_config_hooks(config, NULL); \
					if (err < 0) \
						return err; \
			 );
}

/**
 * \brief Searches for a node in a configuration tree, expanding aliases and hooks.
 * \param[in] root Handle to the root configuration node containing
 *                 alias definitions.
 * \param[in,out] config Handle to the root of the configuration
 *                       (sub)tree to search.
 * \param[in] key Search key: one or more node keys, separated with dots.
 * \param[out] result The function puts the handle to the node found at
 *                    the address specified by \a result.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function searches for a child node of \a config, allowing
 * aliases, like #snd_config_searcha, and expanding hooks, like
 * #snd_config_search_hooks.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOENT<dd>An id in \a key or an alias id does not exist.
 * <dt>-ENOENT<dd>\a config or one of its child nodes to be searched is
 *                not a compound node.
 * </dl>
 * Additionally, any errors encountered when parsing the hook
 * definitions or returned by the hook functions.
 */
int snd_config_searcha_hooks(snd_config_t *root, snd_config_t *config, const char *key, snd_config_t **result)
{
	SND_CONFIG_SEARCHA(root, config, key, result,
					snd_config_searcha_hooks,
					err = snd_config_hooks(config, NULL); \
					if (err < 0) \
						return err; \
			 );
}

/**
 * \brief Searches for a node in a configuration tree, expanding aliases and hooks.
 * \param[in] root Handle to the root configuration node containing
 *                 alias definitions.
 * \param[in,out] config Handle to the root of the configuration
 *                       (sub)tree to search.
 * \param[out] result The function puts the handle to the node found at
 *                    the address specified by \a result.
 * \param[in] ... One or more concatenated dot separated search keys,
 *                terminated with \c NULL.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function searches for a child node of \a config, allowing
 * aliases and expanding hooks like #snd_config_searcha_hooks, but the
 * search key is the concatenation of all passed seach key strings, like
 * with #snd_config_searchv.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOENT<dd>An id in \a key or an alias id does not exist.
 * <dt>-ENOENT<dd>\a config or one of its child nodes to be searched is
 *                not a compound node.
 * </dl>
 * Additionally, any errors encountered when parsing the hook
 * definitions or returned by the hook functions.
 */
int snd_config_searchva_hooks(snd_config_t *root, snd_config_t *config,
			      snd_config_t **result, ...)
{
	SND_CONFIG_SEARCHVA(root, config, result, snd_config_searcha_hooks);
}

/**
 * \brief Searches for a node in a configuration tree, using an alias and expanding hooks.
 * \param[in] config Handle to the root of the configuration (sub)tree
 *                   to search.
 * \param[in] base Search key base, or \c NULL.
 * \param[in] key Search key suffix.
 * \param[out] result The function puts the handle to the node found at
 *                    the address specified by \a result.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This functions searches for a child node of \a config, allowing
 * aliases, like #snd_config_search_alias, and expanding hooks, like
 * #snd_config_search_hooks.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOENT<dd>An id in \a key or an alias id does not exist.
 * <dt>-ENOENT<dd>\a config or one of its child nodes to be searched is
 *                not a compound node.
 * </dl>
 * Additionally, any errors encountered when parsing the hook
 * definitions or returned by the hook functions.
 */
int snd_config_search_alias_hooks(snd_config_t *config,
				  const char *base, const char *key,
				  snd_config_t **result)
{
	SND_CONFIG_SEARCH_ALIAS(config, base, key, result,
				snd_config_searcha_hooks,
				snd_config_searchva_hooks);
}

/** The name of the environment variable containing the files list for #snd_config_update. */
#define ALSA_CONFIG_PATH_VAR "ALSA_CONFIG_PATH"

/**
 * \ingroup Config
 * \brief Configuration top-level node (the global configuration).
 *
 * This variable contains a handle to the top-level configuration node,
 * as loaded from global configuration file.
 *
 * This variable is initialized or updated by #snd_config_update.
 * Functions like #snd_pcm_open (that use a device name from the global
 * configuration) automatically call #snd_config_update.  Before the
 * first call to #snd_config_update, this variable is \c NULL.
 *
 * The global configuration files are specified in the environment
 * variable \c ALSA_CONFIG_PATH.  If this is not set, the default value
 * is "/usr/share/alsa/alsa.conf".
 *
 * \warning Whenever the configuration tree is updated, all string
 * pointers and configuration node handles previously obtained from this
 * variable may become invalid.
 *
 * \par Conforming to:
 * LSB 3.2
 */
snd_config_t *snd_config = NULL;

#ifndef DOC_HIDDEN
struct finfo {
	char *name;
	dev_t dev;
	ino64_t ino;
	time_t mtime;
};

struct _snd_config_update {
	unsigned int count;
	struct finfo *finfo;
};
#endif /* DOC_HIDDEN */

static snd_config_update_t *snd_config_global_update = NULL;

static int snd_config_hooks_call(snd_config_t *root, snd_config_t *config, snd_config_t *private_data)
{
	void *h = NULL;
	snd_config_t *c, *func_conf = NULL;
	char *buf = NULL, errbuf[256];
	const char *lib = NULL, *func_name = NULL;
	const char *str;
	int (*func)(snd_config_t *root, snd_config_t *config, snd_config_t **dst, snd_config_t *private_data) = NULL;
	int err;

	err = snd_config_search(config, "func", &c);
	if (err < 0) {
		SNDERR("Field func is missing");
		return err;
	}
	err = snd_config_get_string(c, &str);
	if (err < 0) {
		SNDERR("Invalid type for field func");
		return err;
	}
	assert(str);
	err = snd_config_search_definition(root, "hook_func", str, &func_conf);
	if (err >= 0) {
		snd_config_iterator_t i, next;
		if (snd_config_get_type(func_conf) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Invalid type for func %s definition", str);
			err = -EINVAL;
			goto _err;
		}
		snd_config_for_each(i, next, func_conf) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = n->id;
			if (strcmp(id, "comment") == 0)
				continue;
			if (strcmp(id, "lib") == 0) {
				err = snd_config_get_string(n, &lib);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			if (strcmp(id, "func") == 0) {
				err = snd_config_get_string(n, &func_name);
				if (err < 0) {
					SNDERR("Invalid type for %s", id);
					goto _err;
				}
				continue;
			}
			SNDERR("Unknown field %s", id);
		}
	}
	if (!func_name) {
		int len = 16 + strlen(str) + 1;
		buf = malloc(len);
		if (! buf) {
			err = -ENOMEM;
			goto _err;
		}
		snprintf(buf, len, "snd_config_hook_%s", str);
		buf[len-1] = '\0';
		func_name = buf;
	}
	h = INTERNAL(snd_dlopen)(lib, RTLD_NOW, errbuf, sizeof(errbuf));
	func = h ? snd_dlsym(h, func_name, SND_DLSYM_VERSION(SND_CONFIG_DLSYM_VERSION_HOOK)) : NULL;
	err = 0;
	if (!h) {
		SNDERR("Cannot open shared library %s (%s)", lib, errbuf);
		err = -ENOENT;
	} else if (!func) {
		SNDERR("symbol %s is not defined inside %s", func_name, lib);
		snd_dlclose(h);
		err = -ENXIO;
	}
	_err:
	if (func_conf)
		snd_config_delete(func_conf);
	if (err >= 0) {
		snd_config_t *nroot;
		err = func(root, config, &nroot, private_data);
		if (err < 0)
			SNDERR("function %s returned error: %s", func_name, snd_strerror(err));
		snd_dlclose(h);
		if (err >= 0 && nroot)
			err = snd_config_substitute(root, nroot);
	}
	free(buf);
	if (err < 0)
		return err;
	return 0;
}

static int snd_config_hooks(snd_config_t *config, snd_config_t *private_data)
{
	snd_config_t *n;
	snd_config_iterator_t i, next;
	int err, hit, idx = 0;

	if ((err = snd_config_search(config, "@hooks", &n)) < 0)
		return 0;
	snd_config_lock();
	snd_config_remove(n);
	do {
		hit = 0;
		snd_config_for_each(i, next, n) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = n->id;
			long i;
			err = safe_strtol(id, &i);
			if (err < 0) {
				SNDERR("id of field %s is not and integer", id);
				err = -EINVAL;
				goto _err;
			}
			if (i == idx) {
				err = snd_config_hooks_call(config, n, private_data);
				if (err < 0)
					goto _err;
				idx++;
				hit = 1;
			}
		}
	} while (hit);
	err = 0;
       _err:
	snd_config_delete(n);
	snd_config_unlock();
	return err;
}

static int config_filename_filter(const struct dirent64 *dirent)
{
	size_t flen;

	if (dirent == NULL)
		return 0;
	if (dirent->d_type == DT_DIR)
		return 0;

	flen = strlen(dirent->d_name);
	if (flen <= 5)
		return 0;

	if (strncmp(&dirent->d_name[flen-5], ".conf", 5) == 0)
		return 1;

	return 0;
}

static int config_file_open(snd_config_t *root, const char *filename)
{
	snd_input_t *in;
	int err;

	err = snd_input_stdio_open(&in, filename, "r");
	if (err >= 0) {
		err = snd_config_load(root, in);
		snd_input_close(in);
		if (err < 0)
			SNDERR("%s may be old or corrupted: consider to remove or fix it", filename);
	} else
		SNDERR("cannot access file %s", filename);

	return err;
}

static int config_file_load(snd_config_t *root, const char *fn, int errors)
{
	struct stat64 st;
	struct dirent64 **namelist;
	int err, n;

	if (!errors && access(fn, R_OK) < 0)
		return 1;
	if (stat64(fn, &st) < 0) {
		SNDERR("cannot stat file/directory %s", fn);
		return 1;
	}
	if (!S_ISDIR(st.st_mode))
		return config_file_open(root, fn);
#ifndef DOC_HIDDEN
#if defined(_GNU_SOURCE) && !defined(__NetBSD__) && !defined(__FreeBSD__) && !defined(__OpenBSD__) && !defined(__DragonFly__) && !defined(__sun) && !defined(__ANDROID__)
#define SORTFUNC	versionsort64
#else
#define SORTFUNC	alphasort64
#endif
#endif
	n = scandir64(fn, &namelist, config_filename_filter, SORTFUNC);
	if (n > 0) {
		int j;
		err = 0;
		for (j = 0; j < n; ++j) {
			if (err >= 0) {
				int sl = strlen(fn) + strlen(namelist[j]->d_name) + 2;
				char *filename = malloc(sl);
				snprintf(filename, sl, "%s/%s", fn, namelist[j]->d_name);
				filename[sl-1] = '\0';

				err = config_file_open(root, filename);
				free(filename);
			}
			free(namelist[j]);
		}
		free(namelist);
		if (err < 0)
			return err;
	}
	return 0;
}

static int config_file_load_user(snd_config_t *root, const char *fn, int errors)
{
	char *fn2;
	int err;

	err = snd_user_file(fn, &fn2);
	if (err < 0)
		return config_file_load(root, fn, errors);
	err = config_file_load(root, fn2, errors);
	free(fn2);
	return err;
}

static int config_file_load_user_all(snd_config_t *_root, snd_config_t *_file, int errors)
{
	snd_config_t *file = _file, *root = _root, *n;
	char *name, *name2, *remain, *rname = NULL;
	int err;

	if (snd_config_get_type(_file) == SND_CONFIG_TYPE_COMPOUND) {
		if ((err = snd_config_search(_file, "file", &file)) < 0) {
			SNDERR("Field file not found");
			return err;
		}
		if ((err = snd_config_search(_file, "root", &root)) >= 0) {
			err = snd_config_get_ascii(root, &rname);
			if (err < 0) {
				SNDERR("Field root is bad");
				return err;
			}
			err = snd_config_make_compound(&root, rname, 0);
			if (err < 0)
				return err;
		}
	}
	if ((err = snd_config_get_ascii(file, &name)) < 0)
		goto _err;
	name2 = name;
	remain = strstr(name, "|||");
	while (1) {
		if (remain) {
			*remain = '\0';
			remain += 3;
		}
		err = config_file_load_user(root, name2, errors);
		if (err < 0)
			goto _err;
		if (err == 0)	/* first hit wins */
			break;
		if (!remain)
			break;
		name2 = remain;
		remain = strstr(remain, "|||");
	}
_err:
	if (root != _root) {
		if (err == 0) {
			if (snd_config_get_type(root) == SND_CONFIG_TYPE_COMPOUND) {
				if (snd_config_is_empty(root))
					goto _del;
			}
			err = snd_config_make_path(&n, _root, rname, 0, 1);
			if (err < 0)
				goto _del;
			err = snd_config_substitute(n, root);
			if (err == 0)
				goto _fin;
		}
_del:
		snd_config_delete(root);
	}
_fin:
	free(name);
	free(rname);
	return err;
}

/**
 * \brief Loads and parses the given configurations files.
 * \param[in] root Handle to the root configuration node.
 * \param[in] config Handle to the configuration node for this hook.
 * \param[out] dst The function puts the handle to the configuration
 *                 node loaded from the file(s) at the address specified
 *                 by \a dst.
 * \param[in] private_data Handle to the private data configuration node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * See \ref confhooks for an example.
 */
int snd_config_hook_load(snd_config_t *root, snd_config_t *config, snd_config_t **dst, snd_config_t *private_data)
{
	snd_config_t *n;
	snd_config_iterator_t i, next;
	int err, idx = 0, errors = 1, hit;

	assert(root && dst);
	if ((err = snd_config_search(config, "errors", &n)) >= 0) {
		errors = snd_config_get_bool(n);
		if (errors < 0) {
			SNDERR("Invalid bool value in field errors");
			return errors;
		}
	}
	if ((err = snd_config_search(config, "files", &n)) < 0) {
		SNDERR("Unable to find field files in the pre-load section");
		return -EINVAL;
	}
	if ((err = snd_config_expand(n, root, NULL, private_data, &n)) < 0) {
		SNDERR("Unable to expand filenames in the pre-load section");
		return err;
	}
	if (snd_config_get_type(n) != SND_CONFIG_TYPE_COMPOUND) {
		SNDERR("Invalid type for field filenames");
		goto _err;
	}
	do {
		hit = 0;
		snd_config_for_each(i, next, n) {
			snd_config_t *n = snd_config_iterator_entry(i);
			const char *id = n->id;
			long i;
			err = safe_strtol(id, &i);
			if (err < 0) {
				SNDERR("id of field %s is not and integer", id);
				err = -EINVAL;
				goto _err;
			}
			if (i == idx) {
				err = config_file_load_user_all(root, n, errors);
				if (err < 0)
					goto _err;
				idx++;
				hit = 1;
			}
		}
	} while (hit);
	*dst = NULL;
	err = 0;
       _err:
	snd_config_delete(n);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(snd_config_hook_load, SND_CONFIG_DLSYM_VERSION_HOOK);
#endif

#ifndef DOC_HIDDEN
int snd_determine_driver(int card, char **driver);
#endif

snd_config_t *_snd_config_hook_private_data(int card, const char *driver)
{
	snd_config_t *private_data, *v;
	int err;

	err = snd_config_make_compound(&private_data, NULL, 0);
	if (err < 0)
		goto __err;
	err = snd_config_imake_integer(&v, "integer", card);
	if (err < 0)
		goto __err;
	err = snd_config_add(private_data, v);
	if (err < 0) {
		snd_config_delete(v);
		goto __err;
	}
	err = snd_config_imake_string(&v, "string", driver);
	if (err < 0)
		goto __err;
	err = snd_config_add(private_data, v);
	if (err < 0) {
		snd_config_delete(v);
		goto __err;
	}
	return private_data;

__err:
	snd_config_delete(private_data);
	return NULL;
}

static int _snd_config_hook_table(snd_config_t *root, snd_config_t *config, snd_config_t *private_data)
{
	snd_config_t *n, *tn;
	const char *id;
	int err;

	if (snd_config_search(config, "table", &n) < 0)
		return 0;
	if ((err = snd_config_expand(n, root, NULL, private_data, &n)) < 0) {
		SNDERR("Unable to expand table compound");
		return err;
	}
	if (snd_config_search(n, "id", &tn) < 0 ||
	    snd_config_get_string(tn, &id) < 0) {
		SNDERR("Unable to find field table.id");
		snd_config_delete(n);
		return -EINVAL;
	}
	if (snd_config_search(n, "value", &tn) < 0 ||
	    snd_config_get_type(tn) != SND_CONFIG_TYPE_STRING) {
		SNDERR("Unable to find field table.value");
		snd_config_delete(n);
		return -EINVAL;
	}
	snd_config_remove(tn);
	if ((err = snd_config_set_id(tn, id)) < 0) {
		snd_config_delete(tn);
		snd_config_delete(n);
		return err;
	}
	snd_config_delete(n);
	if ((err = snd_config_add(root, tn)) < 0) {
		snd_config_delete(tn);
		return err;
	}
	return 0;
}

/**
 * \brief Loads and parses the given configurations files for each
 *        installed sound card.
 * \param[in] root Handle to the root configuration node.
 * \param[in] config Handle to the configuration node for this hook.
 * \param[out] dst The function puts the handle to the configuration
 *                 node loaded from the file(s) at the address specified
 *                 by \a dst.
 * \param[in] private_data Handle to the private data configuration node.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This function works like #snd_config_hook_load, but the files are
 * loaded once for each sound card.  The driver name is available with
 * the \c private_string function to customize the file name.
 */
int snd_config_hook_load_for_all_cards(snd_config_t *root, snd_config_t *config, snd_config_t **dst, snd_config_t *private_data ATTRIBUTE_UNUSED)
{
	int card = -1, err;
	snd_config_t *loaded;	// trace loaded cards
	
	err = snd_config_top(&loaded);
	if (err < 0)
		return err;
	do {
		err = snd_card_next(&card);
		if (err < 0)
			goto __fin_err;
		if (card >= 0) {
			snd_config_t *n, *m, *private_data = NULL;
			const char *driver;
			char *fdriver = NULL;
			bool load;
			err = snd_determine_driver(card, &fdriver);
			if (err < 0)
				goto __fin_err;
			if (snd_config_search(root, fdriver, &n) >= 0) {
				if (snd_config_get_string(n, &driver) < 0) {
					if (snd_config_get_type(n) == SND_CONFIG_TYPE_COMPOUND) {
						snd_config_get_id(n, &driver);
						goto __std;
					}
					goto __err;
				}
				while (1) {
					char *s = strchr(driver, '.');
					if (s == NULL)
						break;
					driver = s + 1;
				}
				if (snd_config_search(root, driver, &n) >= 0)
					goto __err;
			} else {
				driver = fdriver;
			}
		      __std:
			load = true;
			err = snd_config_imake_integer(&m, driver, 1);
			if (err < 0)
				goto __err;
			err = snd_config_add(loaded, m);
			if (err < 0) {
				if (err == -EEXIST) {
					snd_config_delete(m);
					load = false;
				} else {
					goto __err;
				}
			}
			private_data = _snd_config_hook_private_data(card, driver);
			if (!private_data) {
				err = -ENOMEM;
				goto __err;
			}
			err = _snd_config_hook_table(root, config, private_data);
			if (err < 0)
				goto __err;
			if (load)
				err = snd_config_hook_load(root, config, &n, private_data);
		      __err:
			if (private_data)
				snd_config_delete(private_data);
			free(fdriver);
			if (err < 0)
				goto __fin_err;
		}
	} while (card >= 0);
	snd_config_delete(loaded);
	*dst = NULL;
	return 0;
__fin_err:
	snd_config_delete(loaded);
	return err;
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(snd_config_hook_load_for_all_cards, SND_CONFIG_DLSYM_VERSION_HOOK);
#endif

/** 
 * \brief Updates a configuration tree by rereading the configuration files (if needed).
 * \param[in,out] _top Address of the handle to the top-level node.
 * \param[in,out] _update Address of a pointer to private update information.
 * \param[in] cfgs A list of configuration file names, delimited with ':'.
 *                 If \p cfgs is \c NULL, the default global
 *                 configuration file is used.
 * \return 0 if \a _top was up to date, 1 if the configuration files
 *         have been reread, otherwise a negative error code.
 *
 * The variables pointed to by \a _top and \a _update can be initialized
 * to \c NULL before the first call to this function.  The private
 * update information holds information about all used configuration
 * files that allows this function to detects changes to them; this data
 * can be freed with #snd_config_update_free.
 *
 * The global configuration files are specified in the environment variable
 * \c ALSA_CONFIG_PATH.
 *
 * \warning If the configuration tree is reread, all string pointers and
 * configuration node handles previously obtained from this tree become
 * invalid.
 *
 * \par Errors:
 * Any errors encountered when parsing the input or returned by hooks or
 * functions.
 */
int snd_config_update_r(snd_config_t **_top, snd_config_update_t **_update, const char *cfgs)
{
	int err;
	const char *configs, *c;
	unsigned int k;
	size_t l;
	snd_config_update_t *local;
	snd_config_update_t *update;
	snd_config_t *top;
	
	assert(_top && _update);
	top = *_top;
	update = *_update;
	configs = cfgs;
	if (!configs) {
		configs = getenv(ALSA_CONFIG_PATH_VAR);
		if (!configs || !*configs) {
			const char *topdir = snd_config_topdir();
			char *s = alloca(strlen(topdir) +
					 strlen("alsa.conf") + 2);
			sprintf(s, "%s/alsa.conf", topdir);
			configs = s;
		}
	}
	for (k = 0, c = configs; (l = strcspn(c, ": ")) > 0; ) {
		c += l;
		k++;
		if (!*c)
			break;
		c++;
	}
	if (k == 0) {
		local = NULL;
		goto _reread;
	}
	local = (snd_config_update_t *)calloc(1, sizeof(snd_config_update_t));
	if (!local)
		return -ENOMEM;
	local->count = k;
	local->finfo = calloc(local->count, sizeof(struct finfo));
	if (!local->finfo) {
		free(local);
		return -ENOMEM;
	}
	for (k = 0, c = configs; (l = strcspn(c, ": ")) > 0; ) {
		char name[l + 1];
		memcpy(name, c, l);
		name[l] = 0;
		err = snd_user_file(name, &local->finfo[k].name);
		if (err < 0)
			goto _end;
		c += l;
		k++;
		if (!*c)
			break;
		c++;
	}
	for (k = 0; k < local->count; ++k) {
		struct stat64 st;
		struct finfo *lf = &local->finfo[k];
		if (stat64(lf->name, &st) >= 0) {
			lf->dev = st.st_dev;
			lf->ino = st.st_ino;
			lf->mtime = st.st_mtime;
		} else {
			SNDERR("Cannot access file %s", lf->name);
			free(lf->name);
			memmove(&local->finfo[k], &local->finfo[k+1], sizeof(struct finfo) * (local->count - k - 1));
			k--;
			local->count--;
		}
	}
	if (!update)
		goto _reread;
	if (local->count != update->count)
		goto _reread;
	for (k = 0; k < local->count; ++k) {
		struct finfo *lf = &local->finfo[k];
		struct finfo *uf = &update->finfo[k];
		if (strcmp(lf->name, uf->name) != 0 ||
		    lf->dev != uf->dev ||
		    lf->ino != uf->ino ||
		    lf->mtime != uf->mtime)
			goto _reread;
	}
	err = 0;

 _end:
	if (err < 0) {
		if (top) {
			snd_config_delete(top);
			*_top = NULL;
		}
		if (update) {
			snd_config_update_free(update);
			*_update = NULL;
		}
	}
	if (local)
		snd_config_update_free(local);
	return err;

 _reread:
 	*_top = NULL;
 	*_update = NULL;
 	if (update) {
 		snd_config_update_free(update);
 		update = NULL;
 	}
	if (top) {
		snd_config_delete(top);
		top = NULL;
	}
	err = snd_config_top(&top);
	if (err < 0)
		goto _end;
	if (!local)
		goto _skip;
	for (k = 0; k < local->count; ++k) {
		snd_input_t *in;
		err = snd_input_stdio_open(&in, local->finfo[k].name, "r");
		if (err >= 0) {
			err = snd_config_load(top, in);
			snd_input_close(in);
			if (err < 0) {
				SNDERR("%s may be old or corrupted: consider to remove or fix it", local->finfo[k].name);
				goto _end;
			}
		} else {
			SNDERR("cannot access file %s", local->finfo[k].name);
		}
	}
 _skip:
	err = snd_config_hooks(top, NULL);
	if (err < 0) {
		SNDERR("hooks failed, removing configuration");
		goto _end;
	}
	*_top = top;
	*_update = local;
	return 1;
}

/** 
 * \brief Updates #snd_config by rereading the global configuration files (if needed).
 * \return 0 if #snd_config was up to date, 1 if #snd_config was
 *         updated, otherwise a negative error code.
 *
 * \warning Whenever #snd_config is updated, all string pointers and
 * configuration node handles previously obtained from it may become
 * invalid.
 * For safer operations, use #snd_config_update_ref and release the config
 * via #snd_config_unref.
 *
 * \par Errors:
 * Any errors encountered when parsing the input or returned by hooks or
 * functions.
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_update(void)
{
	int err;

	snd_config_lock();
	err = snd_config_update_r(&snd_config, &snd_config_global_update, NULL);
	snd_config_unlock();
	return err;
}

/**
 * \brief Updates #snd_config and takes its reference.
 * \return 0 if #snd_config was up to date, 1 if #snd_config was
 *         updated, otherwise a negative error code.
 *
 * Unlike #snd_config_update, this function increases a reference counter
 * so that the obtained tree won't be deleted until unreferenced by
 * #snd_config_unref.
 *
 * This function is supposed to be thread-safe.
 */
int snd_config_update_ref(snd_config_t **top)
{
	int err;

	if (top)
		*top = NULL;
	snd_config_lock();
	err = snd_config_update_r(&snd_config, &snd_config_global_update, NULL);
	if (err >= 0) {
		if (snd_config) {
			if (top) {
				snd_config->refcount++;
				*top = snd_config;
			}
		} else {
			err = -ENODEV;
		}
	}
	snd_config_unlock();
	return err;
}

/**
 * \brief Take the reference of the config tree.
 *
 * Increases a reference counter of the given config tree.
 *
 * This function is supposed to be thread-safe.
 */
void snd_config_ref(snd_config_t *cfg)
{
	snd_config_lock();
	if (cfg)
		cfg->refcount++;
	snd_config_unlock();
}

/**
 * \brief Unreference the config tree.
 *
 * Decreases a reference counter of the given config tree, and eventually
 * deletes the tree if all references are gone.  This is the counterpart of
 * #snd_config_unref.
 *
 * Also, the config taken via #snd_config_update_ref must be unreferenced
 * by this function, too.
 *
 * This function is supposed to be thread-safe.
 */
void snd_config_unref(snd_config_t *cfg)
{
	snd_config_lock();
	if (cfg)
		snd_config_delete(cfg);
	snd_config_unlock();
}

/** 
 * \brief Frees a private update structure.
 * \param[in] update The private update structure to free.
 * \return Zero if successful, otherwise a negative error code.
 */
int snd_config_update_free(snd_config_update_t *update)
{
	unsigned int k;

	assert(update);
	for (k = 0; k < update->count; k++)
		free(update->finfo[k].name);
	free(update->finfo);
	free(update);
	return 0;
}

/** 
 * \brief Frees the global configuration tree in #snd_config.
 * \return Zero if successful, otherwise a negative error code.
 *
 * This functions releases all resources of the global configuration
 * tree, and sets #snd_config to \c NULL.
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_update_free_global(void)
{
	snd_config_lock();
	if (snd_config)
		snd_config_delete(snd_config);
	snd_config = NULL;
	if (snd_config_global_update)
		snd_config_update_free(snd_config_global_update);
	snd_config_global_update = NULL;
	snd_config_unlock();
	/* FIXME: better to place this in another place... */
	snd_dlobj_cache_cleanup();

	return 0;
}

/**
 * \brief Returns an iterator pointing to a node's first child.
 * \param[in] config Handle to a configuration node.
 * \return An iterator pointing to \a config's first child.
 *
 * \a config must be a compound node.
 *
 * The returned iterator is valid if it is not equal to the return value
 * of #snd_config_iterator_end on \a config.
 *
 * Use #snd_config_iterator_entry to get the handle of the node pointed
 * to.
 *
 * \par Conforming to:
 * LSB 3.2
 */
snd_config_iterator_t snd_config_iterator_first(const snd_config_t *config)
{
	assert(config->type == SND_CONFIG_TYPE_COMPOUND);
	return config->u.compound.fields.next;
}

/**
 * \brief Returns an iterator pointing to the next sibling.
 * \param[in] iterator An iterator pointing to a child configuration node.
 * \return An iterator pointing to the next sibling of \a iterator.
 *
 * The returned iterator is valid if it is not equal to the return value
 * of #snd_config_iterator_end on the node's parent.
 *
 * Use #snd_config_iterator_entry to get the handle of the node pointed
 * to.
 *
 * \par Conforming to:
 * LSB 3.2
 */
snd_config_iterator_t snd_config_iterator_next(const snd_config_iterator_t iterator)
{
	return iterator->next;
}

/**
 * \brief Returns an iterator that ends a node's children list.
 * \param[in] config Handle to a configuration node.
 * \return An iterator that indicates the end of \a config's children list.
 *
 * \a config must be a compound node.
 *
 * The return value can be understood as pointing past the last child of
 * \a config.
 *
 * \par Conforming to:
 * LSB 3.2
 */
snd_config_iterator_t snd_config_iterator_end(const snd_config_t *config)
{
	assert(config->type == SND_CONFIG_TYPE_COMPOUND);
	return (const snd_config_iterator_t)&config->u.compound.fields;
}

/**
 * \brief Returns the configuration node handle pointed to by an iterator.
 * \param[in] iterator A configuration node iterator.
 * \return The configuration node handle pointed to by \a iterator.
 *
 * \par Conforming to:
 * LSB 3.2
 */
snd_config_t *snd_config_iterator_entry(const snd_config_iterator_t iterator)
{
	return list_entry(iterator, snd_config_t, list);
}

#ifndef DOC_HIDDEN
typedef enum _snd_config_walk_pass {
	SND_CONFIG_WALK_PASS_PRE,
	SND_CONFIG_WALK_PASS_POST,
	SND_CONFIG_WALK_PASS_LEAF,
} snd_config_walk_pass_t;
#endif

/* Return 1 if node needs to be attached to parent */
/* Return 2 if compound is replaced with standard node */
#ifndef DOC_HIDDEN
typedef int (*snd_config_walk_callback_t)(snd_config_t *src,
					  snd_config_t *root,
					  snd_config_t **dst,
					  snd_config_walk_pass_t pass,
					  snd_config_expand_fcn_t fcn,
					  void *private_data);
#endif

static int snd_config_walk(snd_config_t *src,
			   snd_config_t *root,
			   snd_config_t **dst, 
			   snd_config_walk_callback_t callback,
			   snd_config_expand_fcn_t fcn,
			   void *private_data)
{
	int err;
	snd_config_iterator_t i, next;

	switch (snd_config_get_type(src)) {
	case SND_CONFIG_TYPE_COMPOUND:
		err = callback(src, root, dst, SND_CONFIG_WALK_PASS_PRE, fcn, private_data);
		if (err <= 0)
			return err;
		snd_config_for_each(i, next, src) {
			snd_config_t *s = snd_config_iterator_entry(i);
			snd_config_t *d = NULL;

			err = snd_config_walk(s, root, (dst && *dst) ? &d : NULL,
					      callback, fcn, private_data);
			if (err < 0)
				goto _error;
			if (err && d) {
				err = snd_config_add(*dst, d);
				if (err < 0)
					goto _error;
			}
		}
		err = callback(src, root, dst, SND_CONFIG_WALK_PASS_POST, fcn, private_data);
		if (err <= 0) {
		_error:
			if (dst && *dst)
				snd_config_delete(*dst);
		}
		break;
	default:
		err = callback(src, root, dst, SND_CONFIG_WALK_PASS_LEAF, fcn, private_data);
		break;
	}
	return err;
}

static int _snd_config_copy(snd_config_t *src,
			    snd_config_t *root ATTRIBUTE_UNUSED,
			    snd_config_t **dst,
			    snd_config_walk_pass_t pass,
			    snd_config_expand_fcn_t fcn ATTRIBUTE_UNUSED,
			    void *private_data ATTRIBUTE_UNUSED)
{
	int err;
	const char *id = src->id;
	snd_config_type_t type = snd_config_get_type(src);
	switch (pass) {
	case SND_CONFIG_WALK_PASS_PRE:
		err = snd_config_make_compound(dst, id, src->u.compound.join);
		if (err < 0)
			return err;
		break;
	case SND_CONFIG_WALK_PASS_LEAF:
		err = snd_config_make(dst, id, type);
		if (err < 0)
			return err;
		switch (type) {
		case SND_CONFIG_TYPE_INTEGER:
		{
			long v;
			err = snd_config_get_integer(src, &v);
			assert(err >= 0);
			snd_config_set_integer(*dst, v);
			break;
		}
		case SND_CONFIG_TYPE_INTEGER64:
		{
			long long v;
			err = snd_config_get_integer64(src, &v);
			assert(err >= 0);
			snd_config_set_integer64(*dst, v);
			break;
		}
		case SND_CONFIG_TYPE_REAL:
		{
			double v;
			err = snd_config_get_real(src, &v);
			assert(err >= 0);
			snd_config_set_real(*dst, v);
			break;
		}
		case SND_CONFIG_TYPE_STRING:
		{
			const char *s;
			err = snd_config_get_string(src, &s);
			assert(err >= 0);
			err = snd_config_set_string(*dst, s);
			if (err < 0)
				return err;
			break;
		}
		default:
			assert(0);
		}
		break;
	default:
		break;
	}
	return 1;
}

/**
 * \brief Creates a copy of a configuration node.
 * \param[out] dst The function puts the handle to the new configuration
 *                 node at the address specified by \a dst.
 * \param[in] src Handle to the source configuration node.
 * \return A non-negative value if successful, otherwise a negative error code.
 *
 * This function creates a deep copy, i.e., if \a src is a compound
 * node, all children are copied recursively.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOMEM<dd>Out of memory.
 * </dl>
 *
 * \par Conforming to:
 * LSB 3.2
 */
int snd_config_copy(snd_config_t **dst,
		    snd_config_t *src)
{
	return snd_config_walk(src, NULL, dst, _snd_config_copy, NULL, NULL);
}

static int _snd_config_expand_vars(snd_config_t **dst, const char *s, void *private_data)
{
	snd_config_t *val, *vars = private_data;
	if (snd_config_search(vars, s, &val) < 0) {
		*dst = NULL;
		return 0;
	}
	return snd_config_copy(dst, val);
}

static int _snd_config_expand(snd_config_t *src,
			      snd_config_t *root ATTRIBUTE_UNUSED,
			      snd_config_t **dst,
			      snd_config_walk_pass_t pass,
			      snd_config_expand_fcn_t fcn,
			      void *private_data)
{
	int err;
	const char *id = src->id;
	snd_config_type_t type = snd_config_get_type(src);
	switch (pass) {
	case SND_CONFIG_WALK_PASS_PRE:
	{
		if (id && strcmp(id, "@args") == 0)
			return 0;
		err = snd_config_make_compound(dst, id, src->u.compound.join);
		if (err < 0)
			return err;
		break;
	}
	case SND_CONFIG_WALK_PASS_LEAF:
		switch (type) {
		case SND_CONFIG_TYPE_INTEGER:
		{
			long v;
			err = snd_config_get_integer(src, &v);
			assert(err >= 0);
			err = snd_config_imake_integer(dst, id, v);
			if (err < 0)
				return err;
			break;
		}
		case SND_CONFIG_TYPE_INTEGER64:
		{
			long long v;
			err = snd_config_get_integer64(src, &v);
			assert(err >= 0);
			err = snd_config_imake_integer64(dst, id, v);
			if (err < 0)
				return err;
			break;
		}
		case SND_CONFIG_TYPE_REAL:
		{
			double v;
			err = snd_config_get_real(src, &v);
			assert(err >= 0);
			err = snd_config_imake_real(dst, id, v);
			if (err < 0)
				return err;
			break;
		}
		case SND_CONFIG_TYPE_STRING:
		{
			const char *s;
			snd_config_t *vars = private_data;
			snd_config_get_string(src, &s);
			if (s && *s == '$') {
				err = snd_config_evaluate_string(dst, s, fcn, vars);
				if (err < 0)
					return err;
				if (*dst == NULL)
					return 0;
				err = snd_config_set_id(*dst, id);
				if (err < 0) {
					snd_config_delete(*dst);
					return err;
				}
			} else {
				err = snd_config_imake_string(dst, id, s);
				if (err < 0)
					return err;
			}
			break;
		}
		default:
			assert(0);
		}
		break;
	default:
		break;
	}
	return 1;
}

static int _snd_config_evaluate(snd_config_t *src,
				snd_config_t *root,
				snd_config_t **dst ATTRIBUTE_UNUSED,
				snd_config_walk_pass_t pass,
				snd_config_expand_fcn_t fcn ATTRIBUTE_UNUSED,
				void *private_data)
{
	int err;
	if (pass == SND_CONFIG_WALK_PASS_PRE) {
		char *buf = NULL, errbuf[256];
		const char *lib = NULL, *func_name = NULL;
		const char *str;
		int (*func)(snd_config_t **dst, snd_config_t *root,
			    snd_config_t *src, snd_config_t *private_data) = NULL;
		void *h = NULL;
		snd_config_t *c, *func_conf = NULL;
		err = snd_config_search(src, "@func", &c);
		if (err < 0)
			return 1;
		err = snd_config_get_string(c, &str);
		if (err < 0) {
			SNDERR("Invalid type for @func");
			return err;
		}
		assert(str);
		err = snd_config_search_definition(root, "func", str, &func_conf);
		if (err >= 0) {
			snd_config_iterator_t i, next;
			if (snd_config_get_type(func_conf) != SND_CONFIG_TYPE_COMPOUND) {
				SNDERR("Invalid type for func %s definition", str);
				err = -EINVAL;
				goto _err;
			}
			snd_config_for_each(i, next, func_conf) {
				snd_config_t *n = snd_config_iterator_entry(i);
				const char *id = n->id;
				if (strcmp(id, "comment") == 0)
					continue;
				if (strcmp(id, "lib") == 0) {
					err = snd_config_get_string(n, &lib);
					if (err < 0) {
						SNDERR("Invalid type for %s", id);
						goto _err;
					}
					continue;
				}
				if (strcmp(id, "func") == 0) {
					err = snd_config_get_string(n, &func_name);
					if (err < 0) {
						SNDERR("Invalid type for %s", id);
						goto _err;
					}
					continue;
				}
				SNDERR("Unknown field %s", id);
			}
		}
		if (!func_name) {
			int len = 9 + strlen(str) + 1;
			buf = malloc(len);
			if (! buf) {
				err = -ENOMEM;
				goto _err;
			}
			snprintf(buf, len, "snd_func_%s", str);
			buf[len-1] = '\0';
			func_name = buf;
		}
		h = INTERNAL(snd_dlopen)(lib, RTLD_NOW, errbuf, sizeof(errbuf));
		if (h)
			func = snd_dlsym(h, func_name, SND_DLSYM_VERSION(SND_CONFIG_DLSYM_VERSION_EVALUATE));
		err = 0;
		if (!h) {
			SNDERR("Cannot open shared library %s (%s)", lib, errbuf);
			err = -ENOENT;
			goto _errbuf;
		} else if (!func) {
			SNDERR("symbol %s is not defined inside %s", func_name, lib);
			snd_dlclose(h);
			err = -ENXIO;
			goto _errbuf;
		}
	       _err:
		if (func_conf)
			snd_config_delete(func_conf);
		if (err >= 0) {
			snd_config_t *eval;
			err = func(&eval, root, src, private_data);
			if (err < 0)
				SNDERR("function %s returned error: %s", func_name, snd_strerror(err));
			snd_dlclose(h);
			if (err >= 0 && eval)
				err = snd_config_substitute(src, eval);
		}
	       _errbuf:
		free(buf);
		if (err < 0)
			return err;
		return 0;
	}
	return 1;
}

/**
 * \brief Evaluates a configuration node at runtime.
 * \param[in,out] config Handle to the source configuration node.
 * \param[in] root Handle to the root of the source configuration.
 * \param[in] private_data Handle to the private data node for runtime evaluation.
 * \param result Must be \c NULL.
 * \return A non-negative value if successful, otherwise a negative error code.
 *
 * This function evaluates any functions (\c \@func) in \a config and
 * replaces those nodes with the respective function results.
 */
int snd_config_evaluate(snd_config_t *config, snd_config_t *root,
		        snd_config_t *private_data, snd_config_t **result)
{
	/* FIXME: Only in place evaluation is currently implemented */
	assert(result == NULL);
	return snd_config_walk(config, root, result, _snd_config_evaluate, NULL, private_data);
}

static int load_defaults(snd_config_t *subs, snd_config_t *defs)
{
	snd_config_iterator_t d, dnext;
	snd_config_for_each(d, dnext, defs) {
		snd_config_t *def = snd_config_iterator_entry(d);
		snd_config_iterator_t f, fnext;
		if (snd_config_get_type(def) != SND_CONFIG_TYPE_COMPOUND)
			continue;
		snd_config_for_each(f, fnext, def) {
			snd_config_t *fld = snd_config_iterator_entry(f);
			const char *id = fld->id;
			if (strcmp(id, "type") == 0)
				continue;
			if (strcmp(id, "default") == 0) {
				snd_config_t *deflt;
				int err;
				err = snd_config_copy(&deflt, fld);
				if (err < 0)
					return err;
				err = snd_config_set_id(deflt, def->id);
				if (err < 0) {
					snd_config_delete(deflt);
					return err;
				}
				err = snd_config_add(subs, deflt);
				if (err < 0) {
					snd_config_delete(deflt);
					return err;
				}
				continue;
			}
			SNDERR("Unknown field %s", id);
			return -EINVAL;
		}
	}
	return 0;
}

static void skip_blank(const char **ptr)
{
	while (1) {
		switch (**ptr) {
		case ' ':
		case '\f':
		case '\t':
		case '\n':
		case '\r':
			break;
		default:
			return;
		}
		(*ptr)++;
	}
}

static int parse_char(const char **ptr)
{
	int c;
	assert(**ptr == '\\');
	(*ptr)++;
	c = **ptr;
	switch (c) {
	case 'n':
		c = '\n';
		break;
	case 't':
		c = '\t';
		break;
	case 'v':
		c = '\v';
		break;
	case 'b':
		c = '\b';
		break;
	case 'r':
		c = '\r';
		break;
	case 'f':
		c = '\f';
		break;
	case '0': case '1': case '2': case '3':
	case '4': case '5': case '6': case '7':
	{
		int num = c - '0';
		int i = 1;
		(*ptr)++;
		do {
			c = **ptr;
			if (c < '0' || c > '7')
				break;
			num = num * 8 + c - '0';
			i++;
			(*ptr)++;
		} while (i < 3);
		return num;
	}
	default:
		break;
	}
	(*ptr)++;
	return c;
}

static int parse_id(const char **ptr)
{
	if (!**ptr)
		return -EINVAL;
	while (1) {
		switch (**ptr) {
		case '\f':
		case '\t':
		case '\n':
		case '\r':
		case ',':
		case '=':
		case '\0':
			return 0;
		default:
			break;
		}
		(*ptr)++;
	}
}

static int parse_string(const char **ptr, char **val)
{
	const size_t bufsize = 256;
	char _buf[bufsize];
	char *buf = _buf;
	size_t alloc = bufsize;
	char delim = **ptr;
	size_t idx = 0;
	(*ptr)++;
	while (1) {
		int c = **ptr;
		switch (c) {
		case '\0':
			SNDERR("Unterminated string");
			return -EINVAL;
		case '\\':
			c = parse_char(ptr);
			if (c < 0) {
				if (alloc > bufsize)
					free(buf);
				return c;
			}
			break;
		default:
			(*ptr)++;
			if (c == delim) {
				*val = malloc(idx + 1);
				if (!*val)
					return -ENOMEM;
				memcpy(*val, buf, idx);
				(*val)[idx] = 0;
				if (alloc > bufsize)
					free(buf);
				return 0;
			}
		}
		if (idx >= alloc) {
			size_t old_alloc = alloc;
			alloc *= 2;
			if (old_alloc == bufsize) {
				buf = malloc(alloc);
				if (!buf)
					return -ENOMEM;
				memcpy(buf, _buf, old_alloc);
			} else {
				char *buf2 = realloc(buf, alloc);
				if (!buf2) {
					free(buf);
					return -ENOMEM;
				}
				buf = buf2;
			}
		}
		buf[idx++] = c;
	}
}
				

/* Parse var=val or val */
static int parse_arg(const char **ptr, unsigned int *varlen, char **val)
{
	const char *str;
	int err, vallen;
	skip_blank(ptr);
	str = *ptr;
	if (*str == '"' || *str == '\'') {
		err = parse_string(ptr, val);
		if (err < 0)
			return err;
		*varlen = 0;
		return 0;
	}
	err = parse_id(ptr);
	if (err < 0)
		return err;
	vallen = *ptr - str;
	skip_blank(ptr);
	if (**ptr != '=') {
		*varlen = 0;
		goto _value;
	}
	*varlen = vallen;
	(*ptr)++;
	skip_blank(ptr);
	str = *ptr;
	if (*str == '"' || *str == '\'') {
		err = parse_string(ptr, val);
		if (err < 0)
			return err;
		return 0;
	}
	err = parse_id(ptr);
	if (err < 0)
		return err;
	vallen = *ptr - str;
 _value:
	*val = malloc(vallen + 1);
	if (!*val)
		return -ENOMEM;
	memcpy(*val, str, vallen);
	(*val)[vallen] = 0;
	return 0;
}


/* val1, val2, ...
 * var1=val1,var2=val2,...
 * { conf syntax }
 */
static int parse_args(snd_config_t *subs, const char *str, snd_config_t *defs)
{
	int err;
	int arg = 0;
	if (str == NULL)
		return 0;
	skip_blank(&str);
	if (!*str)
		return 0;
	if (*str == '{') {
		int len = strlen(str);
		snd_input_t *input;
		snd_config_iterator_t i, next;
		while (1) {
			switch (str[--len]) {
			case ' ':
			case '\f':
			case '\t':
			case '\n':
			case '\r':
				continue;
			default:
				break;
			}
			break;
		}
		if (str[len] != '}')
			return -EINVAL;
		err = snd_input_buffer_open(&input, str + 1, len - 1);
		if (err < 0)
			return err;
		err = snd_config_load_override(subs, input);
		snd_input_close(input);
		if (err < 0)
			return err;
		snd_config_for_each(i, next, subs) {
			snd_config_t *n = snd_config_iterator_entry(i);
			snd_config_t *d;
			const char *id = n->id;
			err = snd_config_search(defs, id, &d);
			if (err < 0) {
				SNDERR("Unknown parameter %s", id);
				return err;
			}
		}
		return 0;
	}
	
	while (1) {
		char buf[256];
		const char *var = buf;
		unsigned int varlen;
		snd_config_t *def, *sub, *typ;
		const char *new = str;
		const char *tmp;
		char *val = NULL;

		sub = NULL;
		err = parse_arg(&new, &varlen, &val);
		if (err < 0)
			goto _err;
		if (varlen > 0) {
			assert(varlen < sizeof(buf));
			memcpy(buf, str, varlen);
			buf[varlen] = 0;
		} else {
			sprintf(buf, "%d", arg);
		}
		err = snd_config_search_alias(defs, NULL, var, &def);
		if (err < 0) {
			SNDERR("Unknown parameter %s", var);
			goto _err;
		}
		if (snd_config_get_type(def) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("Parameter %s definition is not correct", var);
			err = -EINVAL;
			goto _err;
		}
		var = def->id;
		err = snd_config_search(subs, var, &sub);
		if (err >= 0)
			snd_config_delete(sub);
		sub = NULL;
		err = snd_config_search(def, "type", &typ);
		if (err < 0) {
		_invalid_type:
			SNDERR("Parameter %s definition is missing a valid type info", var);
			goto _err;
		}
		err = snd_config_get_string(typ, &tmp);
		if (err < 0 || !tmp)
			goto _invalid_type;
		if (strcmp(tmp, "integer") == 0) {
			long v;
			err = snd_config_make(&sub, var, SND_CONFIG_TYPE_INTEGER);
			if (err < 0)
				goto _err;
			err = safe_strtol(val, &v);
			if (err < 0) {
				SNDERR("Parameter %s must be an integer", var);
				goto _err;
			}
			err = snd_config_set_integer(sub, v);
			if (err < 0)
				goto _err;
		} else if (strcmp(tmp, "integer64") == 0) {
			long long v;
			err = snd_config_make(&sub, var, SND_CONFIG_TYPE_INTEGER64);
			if (err < 0)
				goto _err;
			err = safe_strtoll(val, &v);
			if (err < 0) {
				SNDERR("Parameter %s must be an integer", var);
				goto _err;
			}
			err = snd_config_set_integer64(sub, v);
			if (err < 0)
				goto _err;
		} else if (strcmp(tmp, "real") == 0) {
			double v;
			err = snd_config_make(&sub, var, SND_CONFIG_TYPE_REAL);
			if (err < 0)
				goto _err;
			err = safe_strtod(val, &v);
			if (err < 0) {
				SNDERR("Parameter %s must be a real", var);
				goto _err;
			}
			err = snd_config_set_real(sub, v);
			if (err < 0)
				goto _err;
		} else if (strcmp(tmp, "string") == 0) {
			err = snd_config_make(&sub, var, SND_CONFIG_TYPE_STRING);
			if (err < 0)
				goto _err;
			err = snd_config_set_string(sub, val);
			if (err < 0)
				goto _err;
		} else {
			err = -EINVAL;
			goto _invalid_type;
		}
		err = snd_config_set_id(sub, var);
		if (err < 0)
			goto _err;
		err = snd_config_add(subs, sub);
		if (err < 0) {
		_err:
			if (sub)
				snd_config_delete(sub);
			free(val);
			return err;
		}
		free(val);
		if (!*new)
			break;
		if (*new != ',')
			return -EINVAL;
		str = new + 1;
		arg++;
	}
	return 0;
}

/**
 * \brief Expands a configuration node, applying arguments and functions.
 * \param[in] config Handle to the configuration node.
 * \param[in] root Handle to the root configuration node.
 * \param[in] fcn Custom function to obtain the referred variable name
 * \param[in] private_data Private data node for the custom function
 * \param[out] result The function puts the handle to the result
 *                    configuration node at the address specified by
 *                    \a result.
 * \return A non-negative value if successful, otherwise a negative error code.
 *
 * If \a config has arguments (defined by a child with id \c \@args),
 * this function replaces any string node beginning with $ with the
 * respective argument value, or the default argument value, or nothing.
 * Furthermore, any functions are evaluated (see #snd_config_evaluate).
 * The resulting copy of \a config is returned in \a result.
 *
 * The new tree is not evaluated (\ref snd_config_evaluate).
 */
int snd_config_expand_custom(snd_config_t *config, snd_config_t *root,
			     snd_config_expand_fcn_t fcn, void *private_data,
			     snd_config_t **result)
{
	snd_config_t *res;
	int err;

	err = snd_config_walk(config, root, &res, _snd_config_expand, fcn, private_data);
	if (err < 0) {
		SNDERR("Expand error (walk): %s", snd_strerror(err));
		return err;
	}
	*result = res;
	return 1;
}

/**
 * \brief Expands a configuration node, applying arguments and functions.
 * \param[in] config Handle to the configuration node.
 * \param[in] root Handle to the root configuration node.
 * \param[in] args Arguments string, can be \c NULL.
 * \param[in] private_data Handle to the private data node for functions.
 * \param[out] result The function puts the handle to the result
 *                    configuration node at the address specified by
 *                    \a result.
 * \return A non-negative value if successful, otherwise a negative error code.
 *
 * If \a config has arguments (defined by a child with id \c \@args),
 * this function replaces any string node beginning with $ with the
 * respective argument value, or the default argument value, or nothing.
 * Furthermore, any functions are evaluated (see #snd_config_evaluate).
 * The resulting copy of \a config is returned in \a result.
 */
int snd_config_expand(snd_config_t *config, snd_config_t *root, const char *args,
		      snd_config_t *private_data, snd_config_t **result)
{
	int err;
	snd_config_t *defs, *subs = NULL, *res;
	err = snd_config_search(config, "@args", &defs);
	if (err < 0) {
		if (args != NULL) {
			SNDERR("Unknown parameters %s", args);
			return -EINVAL;
		}
		err = snd_config_copy(&res, config);
		if (err < 0)
			return err;
	} else {
		err = snd_config_top(&subs);
		if (err < 0)
			return err;
		err = load_defaults(subs, defs);
		if (err < 0) {
			SNDERR("Load defaults error: %s", snd_strerror(err));
			goto _end;
		}
		err = parse_args(subs, args, defs);
		if (err < 0) {
			SNDERR("Parse arguments error: %s", snd_strerror(err));
			goto _end;
		}
		err = snd_config_evaluate(subs, root, private_data, NULL);
		if (err < 0) {
			SNDERR("Args evaluate error: %s", snd_strerror(err));
			goto _end;
		}
		err = snd_config_walk(config, root, &res, _snd_config_expand, _snd_config_expand_vars, subs);
		if (err < 0) {
			SNDERR("Expand error (walk): %s", snd_strerror(err));
			goto _end;
		}
	}
	err = snd_config_evaluate(res, root, private_data, NULL);
	if (err < 0) {
		SNDERR("Evaluate error: %s", snd_strerror(err));
		snd_config_delete(res);
		goto _end;
	}
	*result = res;
	err = 1;
 _end:
 	if (subs)
		snd_config_delete(subs);
	return err;
}

/**
 * \brief Searches for a definition in a configuration tree, using
 *        aliases and expanding hooks and arguments.
 * \param[in] config Handle to the configuration (sub)tree to search.
 * \param[in] base Implicit key base, or \c NULL for none.
 * \param[in] name Key suffix, optionally with arguments.
 * \param[out] result The function puts the handle to the expanded found
 *                    node at the address specified by \a result.
 * \return A non-negative value if successful, otherwise a negative error code.
 *
 * This functions searches for a child node of \a config, allowing
 * aliases and expanding hooks, like #snd_config_search_alias_hooks.
 *
 * If \a name contains a colon (:), the rest of the string after the
 * colon contains arguments that are expanded as with
 * #snd_config_expand.
 *
 * In any case, \a result is a new node that must be freed by the
 * caller.
 *
 * \par Errors:
 * <dl>
 * <dt>-ENOENT<dd>An id in \a key or an alias id does not exist.
 * <dt>-ENOENT<dd>\a config or one of its child nodes to be searched is
 *                not a compound node.
 * </dl>
 * Additionally, any errors encountered when parsing the hook
 * definitions or arguments, or returned by (hook) functions.
 */
int snd_config_search_definition(snd_config_t *config,
				 const char *base, const char *name,
				 snd_config_t **result)
{
	snd_config_t *conf;
	char *key;
	const char *args = strchr(name, ':');
	int err;
	if (args) {
		args++;
		key = alloca(args - name);
		memcpy(key, name, args - name - 1);
		key[args - name - 1] = '\0';
	} else {
		key = (char *) name;
	}
	/*
	 *  if key contains dot (.), the implicit base is ignored
	 *  and the key starts from root given by the 'config' parameter
	 */
	snd_config_lock();
	err = snd_config_search_alias_hooks(config, strchr(key, '.') ? NULL : base, key, &conf);
	if (err < 0) {
		snd_config_unlock();
		return err;
	}
	err = snd_config_expand(conf, config, args, NULL, result);
	snd_config_unlock();
	return err;
}

#ifndef DOC_HIDDEN
void snd_config_set_hop(snd_config_t *conf, int hop)
{
	conf->hop = hop;
}

int snd_config_check_hop(snd_config_t *conf)
{
	if (conf) {
		if (conf->hop >= SND_CONF_MAX_HOPS) {
			SYSERR("Too many definition levels (looped?)\n");
			return -EINVAL;
		}
		return conf->hop;
	}
	return 0;
}
#endif

#if 0
/* Not strictly needed, but useful to check for memory leaks */
void _snd_config_end(void) __attribute__ ((destructor));

static void _snd_config_end(void)
{
	int k;
	if (snd_config)
		snd_config_delete(snd_config);
	snd_config = 0;
	for (k = 0; k < files_info_count; ++k)
		free(files_info[k].name);
	free(files_info);
	files_info = NULL;
	files_info_count = 0;
}
#endif

size_t page_size(void)
{
	long s = sysconf(_SC_PAGE_SIZE);
	assert(s > 0);
	return s;
}

size_t page_align(size_t size)
{
	size_t r;
	long psz = page_size();
	r = size % psz;
	if (r)
		return size + psz - r;
	return size;
}

size_t page_ptr(size_t object_offset, size_t object_size, size_t *offset, size_t *mmap_offset)
{
	size_t r;
	long psz = page_size();
	assert(offset);
	assert(mmap_offset);
	*mmap_offset = object_offset;
	object_offset %= psz;
	*mmap_offset -= object_offset;
	object_size += object_offset;
	r = object_size % psz;
	if (r)
		r = object_size + psz - r;
	else
		r = object_size;
	*offset = object_offset;
	return r;
}
