/*
  Copyright (c) 2019 Red Hat Inc.
  All rights reserved.

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as
  published by the Free Software Foundation; either version 2.1 of
  the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  Authors: Jaroslav Kysela <perex@perex.cz>
*/

#include "list.h"
#include "tplg_local.h"

/* verbose output detailing each object size and file position */
void tplg_log_(snd_tplg_t *tplg, char type, size_t pos, const char *fmt, ...)
{
	va_list va;

	if (!tplg->verbose)
		return;

	va_start(va, fmt);
	fprintf(stdout, "%c0x%6.6zx/%6.6zd - ", type, pos, pos);
	vfprintf(stdout, fmt, va);
	va_end(va);
	putc('\n', stdout);
}
