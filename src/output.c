/*
 *  Output object
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
 *
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <assert.h>
#include <ansidecl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "local.h"
#include "asoundlib.h"

typedef struct _snd_output_ops {
	int (*close)(snd_output_t *output);
	int (*printf)(snd_output_t *output, const char *format, va_list args);
	int (*puts)(snd_output_t *output, const char *str);
	int (*putch)(snd_output_t *output, int c);
	int (*flush)(snd_output_t *output);
} snd_output_ops_t;

struct _snd_output {
	snd_output_type_t type;
	snd_output_ops_t *ops;
	void *private;
};

int snd_output_close(snd_output_t *output)
{
	int err = output->ops->close(output);
	free(output);
	return err;
}

int snd_output_printf(snd_output_t *output, const char *format, ...)
{
	int result;
	va_list args;
	va_start(args, format);
	result = output->ops->printf(output, format, args);
	va_end(args);
	return result;
}

int snd_output_puts(snd_output_t *output, const char *str)
{
	return output->ops->puts(output, str);
}
			
int snd_output_putc(snd_output_t *output, int c)
{
	return output->ops->putch(output, c);
}

int snd_output_flush(snd_output_t *output)
{
	return output->ops->flush(output);
}

typedef struct _snd_output_stdio {
	int close;
	FILE *fp;
} snd_output_stdio_t;

int snd_output_stdio_close(snd_output_t *output ATTRIBUTE_UNUSED)
{
	snd_output_stdio_t *stdio = output->private;
	if (close)
		fclose(stdio->fp);
	free(stdio);
	return 0;
}

int snd_output_stdio_printf(snd_output_t *output, const char *format, va_list args)
{
	snd_output_stdio_t *stdio = output->private;
	return vfprintf(stdio->fp, format, args);
}

int snd_output_stdio_puts(snd_output_t *output, const char *str)
{
	snd_output_stdio_t *stdio = output->private;
	return fputs(str, stdio->fp);
}
			
int snd_output_stdio_putc(snd_output_t *output, int c)
{
	snd_output_stdio_t *stdio = output->private;
	return putc(c, stdio->fp);
}

int snd_output_stdio_flush(snd_output_t *output)
{
	snd_output_stdio_t *stdio = output->private;
	return fflush(stdio->fp);
}

snd_output_ops_t snd_output_stdio_ops = {
	close: snd_output_stdio_close,
	printf: snd_output_stdio_printf,
	puts: snd_output_stdio_puts,
	putch: snd_output_stdio_putc,
	flush: snd_output_stdio_flush,
};

int snd_output_stdio_attach(snd_output_t **outputp, FILE *fp, int close)
{
	snd_output_t *output;
	snd_output_stdio_t *stdio;
	assert(outputp && fp);
	stdio = calloc(1, sizeof(*stdio));
	if (!stdio)
		return -ENOMEM;
	output = calloc(1, sizeof(*output));
	if (!output) {
		free(stdio);
		return -ENOMEM;
	}
	stdio->fp = fp;
	stdio->close = close;
	output->type = SND_OUTPUT_STDIO;
	output->ops = &snd_output_stdio_ops;
	output->private = stdio;
	*outputp = output;
	return 0;
}
	
int snd_output_stdio_open(snd_output_t **outputp, const char *file)
{
	int err;
	FILE *fp = fopen(file, "w");
	if (!fp) {
		SYSERR("fopen");
		return -errno;
	}
	err = snd_output_stdio_attach(outputp, fp, 1);
	if (err < 0)
		fclose(fp);
	return err;
}

typedef struct _snd_output_buffer {
	unsigned char *buf;
	size_t alloc;
	size_t size;
} snd_output_buffer_t;

int snd_output_buffer_close(snd_output_t *output ATTRIBUTE_UNUSED)
{
	snd_output_buffer_t *buffer = output->private;
	free(buffer->buf);
	free(buffer);
	return 0;
}

int snd_output_buffer_need(snd_output_t *output, size_t size)
{
	snd_output_buffer_t *buffer = output->private;
	size_t free = buffer->alloc - buffer->size;
	size_t alloc;
	if (free >= size)
		return free;
	if (buffer->alloc == 0)
		alloc = 256;
	else
		alloc = buffer->alloc * 2;
	buffer->buf = realloc(buffer->buf, alloc);
	if (!buffer->buf)
		return -ENOMEM;
	buffer->alloc = alloc;
	return buffer->alloc - buffer->size;
}

int snd_output_buffer_printf(snd_output_t *output, const char *format, va_list args)
{
	snd_output_buffer_t *buffer = output->private;
	size_t size = 256;
	int result;
	result = snd_output_buffer_need(output, size);
	if (result < 0)
		return result;
	result = vsnprintf(buffer->buf + buffer->size, size, format, args);
	assert(result >= 0);
	if ((size_t)result <= size) {
		buffer->size += result;
		return result;
	}
	size = result;
	result = snd_output_buffer_need(output, size);
	if (result < 0)
		return result;
	result = vsprintf(buffer->buf + buffer->size, format, args);
	assert(result == (int)size);
	return result;
}

int snd_output_buffer_puts(snd_output_t *output, const char *str)
{
	snd_output_buffer_t *buffer = output->private;
	size_t size = strlen(str);
	int err;
	err = snd_output_buffer_need(output, size);
	if (err < 0)
		return err;
	memcpy(buffer->buf + buffer->size, str, size);
	buffer->size += size;
	return size;
}
			
int snd_output_buffer_putc(snd_output_t *output, int c)
{
	snd_output_buffer_t *buffer = output->private;
	int err;
	err = snd_output_buffer_need(output, 1);
	if (err < 0)
		return err;
	buffer->buf[buffer->size++] = c;
	return 0;
}

int snd_output_buffer_flush(snd_output_t *output ATTRIBUTE_UNUSED)
{
	snd_output_buffer_t *buffer = output->private;
	buffer->size = 0;
	return 0;
}

size_t snd_output_buffer_string(snd_output_t *output, char **buf)
{
	snd_output_buffer_t *buffer = output->private;
	*buf = buffer->buf;
	return buffer->size;
}

snd_output_ops_t snd_output_buffer_ops = {
	close: snd_output_buffer_close,
	printf: snd_output_buffer_printf,
	puts: snd_output_buffer_puts,
	putch: snd_output_buffer_putc,
	flush: snd_output_buffer_flush,
};

int snd_output_buffer_open(snd_output_t **outputp)
{
	snd_output_t *output;
	snd_output_buffer_t *buffer;
	assert(outputp);
	buffer = calloc(1, sizeof(*buffer));
	if (!buffer)
		return -ENOMEM;
	output = calloc(1, sizeof(*output));
	if (!output) {
		free(buffer);
		return -ENOMEM;
	}
	buffer->buf = NULL;
	buffer->alloc = 0;
	buffer->size = 0;
	output->type = SND_OUTPUT_BUFFER;
	output->ops = &snd_output_buffer_ops;
	output->private = buffer;
	*outputp = output;
	return 0;
}
	
