/**
 * \file dlmisc.c
 * \brief dynamic loader helpers
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2001
 *
 * Dynamic loader helpers
 */
/*
 *  Dynamic loader helpers
 *  Copyright (c) 2000 by Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details.
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <dlfcn.h>
#include "local.h"

/**
 * \brief Verify dynamically loaded symbol
 * \param handle dlopen handle
 * \param name name of symbol
 * \param version version of symbol
 * \return zero is success, otherwise a negative error code
 */
int snd_dlsym_verify(void *handle, const char *name, const char *version)
{
	int res;
	char *vname;
	
	if (handle == NULL)
		return -EINVAL;
	vname = alloca(1 + strlen(name) + strlen(version) + 1);
	vname[0] = '_';
	strcpy(vname + 1, name);
	strcat(vname, version);
	res = dlsym(handle, vname) == NULL ? -ENOENT : 0;
	printf("dlsym verify: %i, vname = '%s'\n", res, vname);
	if (res < 0)
		SNDERR("unable to verify version for symbol %s", name);
	return res;
}
