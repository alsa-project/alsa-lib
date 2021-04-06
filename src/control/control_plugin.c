/**
 * \file control/control_plugin.c
 * \ingroup Control
 * \brief Control Interface
 * \author Jaroslav Kysela <perex@perex.cz>
 * \date 2021
 */
/*
 *  Control - Common plugin code
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

/*!

\page control_plugins Primitive control plugins

Control plugins extends functionality and features of control devices.
The plugins take care about various control mapping or so.

The child configuration (in one compound):

\code
ctl.test {
	type remap
	child "hw:0"
	... map/remap configuration ...
}
\endcode

The child may be defined as compound containing the full specification:

\code
ctl.test {
	type remap
	child {
		type hw
		card 0
	}
	... map/remap configuration ...
}
\endcode

*/

#include "control_local.h"
#include "control_plugin.h"

/* move the common plugin code from control_remap.c here on demand */
