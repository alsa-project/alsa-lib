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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <dlfcn.h>
#include "local.h"

#ifndef DOC_HIDDEN
#ifndef PIC
struct snd_dlsym_link *snd_dlsym_start = NULL;
#endif
#endif

/**
 * \brief Open the dynamic library, with ALSA extension
 * \param name name, similar to dlopen
 * \param mode mode, similar to dlopen
 * \return pointer to handle
 *
 * The extension is a special code for the static build of
 * the alsa-lib library.
 */
void *snd_dlopen(const char *name, int mode)
{
#ifndef PIC
	if (name == NULL)
		return &snd_dlsym_start;
#endif
	return dlopen(name, mode);
}

/**
 * \brief Close the dynamic library, with ALSA extension
 * \param handle handle, similar to dlclose
 * \return zero if success, otherwise an error code
 *
 * The extension is a special code for the static build of
 * the alsa-lib library.
 */
int snd_dlclose(void *handle)
{
#ifndef PIC
	if (handle == &snd_dlsym_start)
		return 0;
#endif
	return dlclose(handle);
}

/**
 * \brief Verify dynamically loaded symbol
 * \param handle dlopen handle
 * \param name name of symbol
 * \param version version of symbol
 * \return zero is success, otherwise a negative error code
 */
static int snd_dlsym_verify(void *handle, const char *name, const char *version)
{
	int res;
	char *vname;
	
	if (handle == NULL)
		return -EINVAL;
	vname = alloca(1 + strlen(name) + strlen(version) + 1);
	if (vname == NULL)
		return -ENOMEM;
	vname[0] = '_';
	strcpy(vname + 1, name);
	strcat(vname, version);
	res = dlsym(handle, vname) == NULL ? -ENOENT : 0;
	// printf("dlsym verify: %i, vname = '%s'\n", res, vname);
	if (res < 0)
		SNDERR("unable to verify version for symbol %s", name);
	return res;
}

/**
 * \brief Resolve the symbol, with ALSA extension
 * \param handle handle, similar to dlsym
 * \param name symbol name
 * \param version symbol version
 *
 * This special version of dlsym function checks also
 * the version of symbol. The version of a symbol should
 * be defined using #SND_DLSYM_BUILD_VERSION macro.
 */
void *snd_dlsym(void *handle, const char *name, const char *version)
{
	int err;

#ifndef PIC
	if (handle == &snd_dlsym_start) {
		/* it's the funny part: */
		/* we are looking for a symbol in a static library */
		struct snd_dlsym_link *link = snd_dlsym_start;
		while (link) {
			if (!strcmp(name, link->dlsym_name))
				return (void *)link->dlsym_ptr;
			link = link->next;
		}
		return NULL;
	}
#endif
	err = snd_dlsym_verify(handle, name, version);
	if (err < 0)
		return NULL;
	return dlsym(handle, name);
}
