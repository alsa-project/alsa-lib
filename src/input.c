/*
 *  Input object
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "local.h"

typedef struct _snd_input_ops {
	int (*close)(snd_input_t *input);
	int (*scanf)(snd_input_t *input, const char *format, va_list args);
	char *(*gets)(snd_input_t *input, char *str, size_t size);
	int (*getch)(snd_input_t *input);
	int (*ungetch)(snd_input_t *input, int c);
} snd_input_ops_t;

struct _snd_input {
	snd_input_type_t type;
	snd_input_ops_t *ops;
	void *private_data;
};

int snd_input_close(snd_input_t *input)
{
	int err = input->ops->close(input);
	free(input);
	return err;
}

int snd_input_scanf(snd_input_t *input, const char *format, ...)
{
	int result;
	va_list args;
	va_start(args, format);
	result = input->ops->scanf(input, format, args);
	va_end(args);
	return result;
}

char *snd_input_gets(snd_input_t *input, char *str, size_t size)
{
	return input->ops->gets(input, str, size);
}
			
int snd_input_getc(snd_input_t *input)
{
	return input->ops->getch(input);
}

int snd_input_ungetc(snd_input_t *input, int c)
{
	return input->ops->ungetch(input, c);
}

typedef struct _snd_input_stdio {
	int close;
	FILE *fp;
} snd_input_stdio_t;

int snd_input_stdio_close(snd_input_t *input ATTRIBUTE_UNUSED)
{
	snd_input_stdio_t *stdio = input->private_data;
	if (close)
		fclose(stdio->fp);
	free(stdio);
	return 0;
}

int snd_input_stdio_scanf(snd_input_t *input, const char *format, va_list args)
{
	snd_input_stdio_t *stdio = input->private_data;
	extern int vfscanf(FILE *fp, const char *format, va_list args);
	return vfscanf(stdio->fp, format, args);
}

char *snd_input_stdio_gets(snd_input_t *input, char *str, size_t size)
{
	snd_input_stdio_t *stdio = input->private_data;
	return fgets(str, size, stdio->fp);
}
			
int snd_input_stdio_getc(snd_input_t *input)
{
	snd_input_stdio_t *stdio = input->private_data;
	return getc(stdio->fp);
}

int snd_input_stdio_ungetc(snd_input_t *input, int c)
{
	snd_input_stdio_t *stdio = input->private_data;
	return ungetc(c, stdio->fp);
}

snd_input_ops_t snd_input_stdio_ops = {
	close: snd_input_stdio_close,
	scanf: snd_input_stdio_scanf,
	gets: snd_input_stdio_gets,
	getch: snd_input_stdio_getc,
	ungetch: snd_input_stdio_ungetc,
};

int snd_input_stdio_attach(snd_input_t **inputp, FILE *fp, int close)
{
	snd_input_t *input;
	snd_input_stdio_t *stdio;
	assert(inputp && fp);
	stdio = calloc(1, sizeof(*stdio));
	if (!stdio)
		return -ENOMEM;
	input = calloc(1, sizeof(*input));
	if (!input) {
		free(stdio);
		return -ENOMEM;
	}
	stdio->fp = fp;
	stdio->close = close;
	input->type = SND_INPUT_STDIO;
	input->ops = &snd_input_stdio_ops;
	input->private_data = stdio;
	*inputp = input;
	return 0;
}
	
int snd_input_stdio_open(snd_input_t **inputp, const char *file)
{
	int err;
	FILE *fp = fopen(file, "r");
	if (!fp) {
		//SYSERR("fopen");
		return -errno;
	}
	err = snd_input_stdio_attach(inputp, fp, 1);
	if (err < 0)
		fclose(fp);
	return err;
}

typedef struct _snd_input_buffer {
	unsigned char *buf;
	unsigned char *ptr;
	size_t size;
} snd_input_buffer_t;

int snd_input_buffer_close(snd_input_t *input)
{
	snd_input_buffer_t *buffer = input->private_data;
	free(buffer->buf);
	free(buffer);
	return 0;
}

int snd_input_buffer_scanf(snd_input_t *input, const char *format, va_list args)
{
	snd_input_buffer_t *buffer = input->private_data;
	extern int vsscanf(const char *buf, const char *format, va_list args);
	/* FIXME: how can I obtain consumed chars count? */
	assert(0);
	return vsscanf(buffer->ptr, format, args);
}

char *snd_input_buffer_gets(snd_input_t *input, char *str, size_t size)
{
	snd_input_buffer_t *buffer = input->private_data;
	size_t bsize = buffer->size;
	while (--size > 0 && bsize > 0) {
		unsigned char c = *buffer->ptr++;
		bsize--;
		*str++ = c;
		if (c == '\n')
			break;
	}
	if (bsize == buffer->size)
		return NULL;
	buffer->size = bsize;
	*str = '\0';
	return str;
}
			
int snd_input_buffer_getc(snd_input_t *input)
{
	snd_input_buffer_t *buffer = input->private_data;
	if (buffer->size == 0)
		return EOF;
	buffer->size--;
	return *buffer->ptr++;
}

int snd_input_buffer_ungetc(snd_input_t *input, int c)
{
	snd_input_buffer_t *buffer = input->private_data;
	if (buffer->ptr == buffer->buf)
		return EOF;
	buffer->ptr--;
	assert(*buffer->ptr == (unsigned char) c);
	buffer->size++;
	return c;
}

snd_input_ops_t snd_input_buffer_ops = {
	close: snd_input_buffer_close,
	scanf: snd_input_buffer_scanf,
	gets: snd_input_buffer_gets,
	getch: snd_input_buffer_getc,
	ungetch: snd_input_buffer_ungetc,
};

int snd_input_buffer_open(snd_input_t **inputp, const char *buf, int size)
{
	snd_input_t *input;
	snd_input_buffer_t *buffer;
	assert(inputp);
	buffer = calloc(1, sizeof(*buffer));
	if (!buffer)
		return -ENOMEM;
	input = calloc(1, sizeof(*input));
	if (!input) {
		free(buffer);
		return -ENOMEM;
	}
	if (size < 0)
		size = strlen(buf);
	buffer->buf = malloc(size+1);
	if (!buffer->buf) {
		free(input);
		free(buffer);
		return -ENOMEM;
	}
	memcpy(buffer->buf, buf, size);
	buffer->buf[size] = 0;
	buffer->ptr = buffer->buf;
	buffer->size = size;
	input->type = SND_INPUT_BUFFER;
	input->ops = &snd_input_buffer_ops;
	input->private_data = buffer;
	*inputp = input;
	return 0;
}
	
