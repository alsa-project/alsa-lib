/**
 * \file output.c
 * \brief Generic stdio-like output interface
 * \author Abramo Bagnara <abramo@alsa-project.org>
 * \date 2000
 *
 * Generic stdio-like output interface
 */
/*
 *  Output object
 *  Copyright (c) 2000 by Abramo Bagnara <abramo@alsa-project.org>
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

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "local.h"

#ifndef DOC_HIDDEN
typedef struct _snd_output_ops {
	int (*close)(snd_output_t *output);
	int (*print)(snd_output_t *output, const char *format, va_list args);
	int (*puts)(snd_output_t *output, const char *str);
	int (*putch)(snd_output_t *output, int c);
	int (*flush)(snd_output_t *output);
} snd_output_ops_t;

struct _snd_output {
	snd_output_type_t type;
	snd_output_ops_t *ops;
	void *private_data;
};
#endif

/**
 * \brief close output handle
 * \param output Output handle
 * \return 0 on success otherwise a negative error code
 */
int snd_output_close(snd_output_t *output)
{
	int err = output->ops->close(output);
	free(output);
	return err;
}

/**
 * \brief fprintf(3) like on an output handle
 * \param output Output handle
 * \param format fprintf format
 * \param ... other fprintf arguments
 * \return number of characters written otherwise a negative error code
 */
int snd_output_printf(snd_output_t *output, const char *format, ...)
{
	int result;
	va_list args;
	va_start(args, format);
	result = output->ops->print(output, format, args);
	va_end(args);
	return result;
}

/**
 * \brief fputs(3) like on an output handle
 * \param output Output handle
 * \param str Buffer pointer 
 * \return 0 on success otherwise a negative error code
 */
int snd_output_puts(snd_output_t *output, const char *str)
{
	return output->ops->puts(output, str);
}
			
/**
 * \brief fputs(3) like on an output handle
 * \param output Output handle
 * \param str Source buffer pointer
 * \return 0 on success otherwise a negative error code
 */
int snd_output_putc(snd_output_t *output, int c)
{
	return output->ops->putch(output, c);
}

/**
 * \brief fflush(3) like on an output handle
 * \param output Output handle
 * \return 0 on success otherwise a negative error code
 */
int snd_output_flush(snd_output_t *output)
{
	return output->ops->flush(output);
}

#ifndef DOC_HIDDEN
typedef struct _snd_output_stdio {
	int close;
	FILE *fp;
} snd_output_stdio_t;

static int snd_output_stdio_close(snd_output_t *output ATTRIBUTE_UNUSED)
{
	snd_output_stdio_t *stdio = output->private_data;
	if (stdio->close)
		fclose(stdio->fp);
	free(stdio);
	return 0;
}

static int snd_output_stdio_print(snd_output_t *output, const char *format, va_list args)
{
	snd_output_stdio_t *stdio = output->private_data;
	return vfprintf(stdio->fp, format, args);
}

static int snd_output_stdio_puts(snd_output_t *output, const char *str)
{
	snd_output_stdio_t *stdio = output->private_data;
	return fputs(str, stdio->fp);
}
			
static int snd_output_stdio_putc(snd_output_t *output, int c)
{
	snd_output_stdio_t *stdio = output->private_data;
	return putc(c, stdio->fp);
}

static int snd_output_stdio_flush(snd_output_t *output)
{
	snd_output_stdio_t *stdio = output->private_data;
	return fflush(stdio->fp);
}

static snd_output_ops_t snd_output_stdio_ops = {
	close: snd_output_stdio_close,
	print: snd_output_stdio_print,
	puts: snd_output_stdio_puts,
	putch: snd_output_stdio_putc,
	flush: snd_output_stdio_flush,
};

#endif

/**
 * \brief Create a new output using an existing stdio FILE pointer
 * \param outputp Pointer to returned output handle
 * \param fp FILE pointer
 * \param close Close flag (1 if FILE is fclose'd when output handle is closed)
 * \return 0 on success otherwise a negative error code
 */
int snd_output_stdio_attach(snd_output_t **outputp, FILE *fp, int _close)
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
	stdio->close = _close;
	output->type = SND_OUTPUT_STDIO;
	output->ops = &snd_output_stdio_ops;
	output->private_data = stdio;
	*outputp = output;
	return 0;
}
	
/**
 * \brief Open a new output to a file
 * \param outputp Pointer to returned output handle
 * \param file File name
 * \param mode fopen(3) open mode
 * \return 0 on success otherwise a negative error code
 */
int snd_output_stdio_open(snd_output_t **outputp, const char *file, const char *mode)
{
	int err;
	FILE *fp = fopen(file, mode);
	if (!fp) {
		//SYSERR("fopen");
		return -errno;
	}
	err = snd_output_stdio_attach(outputp, fp, 1);
	if (err < 0)
		fclose(fp);
	return err;
}

#ifndef DOC_HIDDEN

typedef struct _snd_output_buffer {
	unsigned char *buf;
	size_t alloc;
	size_t size;
} snd_output_buffer_t;

static int snd_output_buffer_close(snd_output_t *output ATTRIBUTE_UNUSED)
{
	snd_output_buffer_t *buffer = output->private_data;
	free(buffer->buf);
	free(buffer);
	return 0;
}

static int snd_output_buffer_need(snd_output_t *output, size_t size)
{
	snd_output_buffer_t *buffer = output->private_data;
	size_t _free = buffer->alloc - buffer->size;
	size_t alloc;
	if (_free >= size)
		return _free;
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

static int snd_output_buffer_print(snd_output_t *output, const char *format, va_list args)
{
	snd_output_buffer_t *buffer = output->private_data;
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

static int snd_output_buffer_puts(snd_output_t *output, const char *str)
{
	snd_output_buffer_t *buffer = output->private_data;
	size_t size = strlen(str);
	int err;
	err = snd_output_buffer_need(output, size);
	if (err < 0)
		return err;
	memcpy(buffer->buf + buffer->size, str, size);
	buffer->size += size;
	return size;
}
			
static int snd_output_buffer_putc(snd_output_t *output, int c)
{
	snd_output_buffer_t *buffer = output->private_data;
	int err;
	err = snd_output_buffer_need(output, 1);
	if (err < 0)
		return err;
	buffer->buf[buffer->size++] = c;
	return 0;
}

static int snd_output_buffer_flush(snd_output_t *output ATTRIBUTE_UNUSED)
{
	snd_output_buffer_t *buffer = output->private_data;
	buffer->size = 0;
	return 0;
}

static snd_output_ops_t snd_output_buffer_ops = {
	close: snd_output_buffer_close,
	print: snd_output_buffer_print,
	puts: snd_output_buffer_puts,
	putch: snd_output_buffer_putc,
	flush: snd_output_buffer_flush,
};
#endif

/**
 * \brief Return buffer info for a #SND_OUTPUT_TYPE_BUFFER output handle
 * \param output Output handle
 * \param buf Pointer to returned buffer
 * \return size of data in buffer
 */
size_t snd_output_buffer_string(snd_output_t *output, char **buf)
{
	snd_output_buffer_t *buffer = output->private_data;
	*buf = buffer->buf;
	return buffer->size;
}

/**
 * \brief Open a new output to an auto extended  memory buffer
 * \param outputp Pointer to returned output handle
 * \return 0 on success otherwise a negative error code
 */
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
	output->private_data = buffer;
	*outputp = output;
	return 0;
}
	
