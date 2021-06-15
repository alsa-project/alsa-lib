/**
 * \file control/control_empty.c
 * \ingroup Control_Plugins
 * \brief Control Empty Plugin Interface
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 2021
 */
/*
 *  Control - Empty plugin
 *  Copyright (c) 2021 by Jaroslav Kysela <perex@perex.cz>
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

#include "control_local.h"

#ifndef PIC
/* entry for static linking */
const char *_snd_module_control_empty = "";
#endif

/*! \page control_plugins

\section control_plugins_empty Plugin: Empty

This plugin just redirects the control device to another plugin.

\code
ctl.name {
	type empty              # Empty Control
	child STR               # Slave name
	# or
	child {                 # Child definition
		...
	}
}
\endcode

\subsection control_plugins_empty_funcref Function reference

<UL>
  <LI>_snd_ctl_empty_open()
</UL>

*/

/**
 * \brief Creates a new Empty Control
 * \param handlep Returns created Control handle
 * \param name Name of Control
 * \param root Root configuration node
 * \param conf Configuration node with empty Control description
 * \param mode Control mode
 * \retval zero on success otherwise a negative error code
 * \warning Using of this function might be dangerous in the sense
 *          of compatibility reasons. The prototype might be freely
 *          changed in future.
 */
int _snd_ctl_empty_open(snd_ctl_t **handlep, const char *name ATTRIBUTE_UNUSED,
			snd_config_t *root, snd_config_t *conf,  int mode)
{
	snd_config_t *child = NULL;
	snd_config_iterator_t i, next;

	snd_config_for_each(i, next, conf) {
		snd_config_t *n = snd_config_iterator_entry(i);
		const char *id;
		if (snd_config_get_id(n, &id) < 0)
			continue;
		if (_snd_conf_generic_id(id))
			continue;
		if (strcmp(id, "child") == 0) {
			child = n;
			continue;
		}
		SNDERR("Unknown field %s", id);
		return -EINVAL;
	}
	if (!child) {
		SNDERR("child is not defined");
		return -EINVAL;
	}
	return _snd_ctl_open_named_child(handlep, name, root, child, mode, conf);
}
#ifndef DOC_HIDDEN
SND_DLSYM_BUILD_VERSION(_snd_ctl_empty_open, SND_CONTROL_DLSYM_VERSION);
#endif
