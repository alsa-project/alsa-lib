/*
 *  Get full filename
 *  Copyright (c) 2003 by Jaroslav Kysela <perex@suse.cz>
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
  
#include <string.h>
#include <errno.h>
#include <wordexp.h>
#include <assert.h>

int snd_user_file(const char *file, char **result)
{
	wordexp_t we;
	int err;
	
	assert(file && result);
	err = wordexp(file, &we, WRDE_NOCMD);
	switch (err) {
	case WRDE_NOSPACE:
		return -ENOMEM;
	case 0:
		if (we.we_wordc == 1)
			break;
		/* fall thru */
	default:
		wordfree(&we);
		return -EINVAL;
	}
	*result = strdup(we.we_wordv[0]);
	if (*result == NULL)
		return -ENOMEM;
	wordfree(&we);
	return 0;
}
