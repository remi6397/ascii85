/*
 * ascii85 - Ascii85 encode/decode data and print to standard output
 *
 * Copyright (C) 2017 Jeremiasz Nelz <remi6397[at]gmail[dot]com>.
 * Copyright (C) 2012-2016 Remy Oukaour <http://www.remyoukaour.com>.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "ascii85.h"

unsigned int AllocMax;
char* Data;
unsigned int Index;
unsigned int IIndex;

void append_char(int c)
{
	if (Index > 0 && (Index % AllocMax) == 0)
	{
		realloc(Data, (AllocMax *= 2));
	}
	Data[Index++] = c;
}

int get_char(char* ptr)
{
	return ptr[IIndex++];
}

int unget_char(char c, char* ptr)
{
	return (Data[--IIndex] = c);
}

#define putchar(c) append_char(c)
#define getc(ptr) get_char(ptr)
#define ungetc(c, ptr) unget_char(c, ptr)

int powers[5] = {85*85*85*85, 85*85*85, 85*85, 85, 1};

int getc_nospace(char* f) {
	int c;
	while (isspace(c = getc(f)));
	return c;
}

void putc_wrap(char c, int wrap, int *len) {
	if (wrap && *len >= wrap) {
		putchar('\n');
		*len = 0;
	}
	putchar(c);
	(*len)++;
}

void encode_tuple(uint32_t tuple, int count, int wrap, int *plen, int y_abbr) {
	int i, lim;
	char out[5];
	if (tuple == 0 && count == 4) {
		putc_wrap('z', wrap, plen);
	}
	else if (tuple == 0x20202020 && count == 4 && y_abbr) {
		putc_wrap('y', wrap, plen);
	}
	else {
		for (i = 0; i < 5; i++) {
			out[i] = tuple % 85 + '!';
			tuple /= 85;
		}
		lim = 4 - count;
		for (i = 4; i >= lim; i--) {
			putc_wrap(out[i], wrap, plen);
		}
	}
}

void decode_tuple(uint32_t tuple, int count) {
	int i;
	for (i = 1; i < count; i++) {
		putchar(tuple >> ((4 - i) * 8));
	}
}

int ascii85_encode(char** input_p, char** output_p, int delims, int wrap, int y_abbr) {
	char* input = *input_p;
	int c, count = 0, len = 0;
	uint32_t tuple = 0;

	AllocMax = 16;
	Data = (char*)malloc(AllocMax);
	Index = 0;
	IIndex = 0;
	
	if (delims) {
		putc_wrap('<', wrap, &len);
		putc_wrap('~', wrap, &len);
	}
	for (;;) {
		c = getc(input);
		if (c != '\0') {
			tuple |= ((unsigned int)c) << ((3 - count++) * 8);
			if (count < 4) continue;
		}
		else if (count == 0) break;
		encode_tuple(tuple, count, wrap, &len, y_abbr);
		if (c == '\0') break;
		tuple = 0;
		count = 0;
	}
	if (delims) {
		putc_wrap('~', wrap, &len);
		putc_wrap('>', wrap, &len);
	}
	
	*output_p = (char*)malloc(Index+1);
	memcpy(*output_p, Data, Index);
	(*output_p)[Index] = '\0';

	return 0;
}

int ascii85_decode(char** input_p, char** output_p, int delims, int ignore_garbage) {
	char* input = *input_p;
	int c, count = 0, end = 0;
	uint32_t tuple = 0;

	AllocMax = 16;
	Data = (char*)malloc(AllocMax);
	Index = 0;
	IIndex = 0;
	
	while (delims) {
		c = getc_nospace(input);
		if (c == '<') {
			c = getc_nospace(input);
			if (c == '~') break;
			ungetc(c, input);
		}
		else if (c == '\0') {
			return 1;
		}
	}
	for (;;) {
		c = getc_nospace(input);
		if (c == 'z' && count == 0) {
			decode_tuple(0, 5);
			continue;
		}
		if (c == 'y' && count == 0) {
			decode_tuple(0x20202020, 5);
			continue;
		}
		if (c == '~' && delims) {
			c = getc_nospace(input);
			if (c != '>') {
				return 1;
			}
			c = '\0';
			end = 1;
		}
		if (c == '\0') {
			if (delims && !end) {
				return 1;
			}
			if (count > 0) {
				tuple += powers[count-1];
				decode_tuple(tuple, count);
			}
			break;
		}
		if (c < '!' || c > 'u') {
			if (ignore_garbage) continue;
			return 1;
		}
		tuple += (c - '!') * powers[count++];
		if (count == 5) {
			decode_tuple(tuple, count);
			tuple = 0;
			count = 0;
		}
	}
	
	*output_p = (char*)malloc(Index+1);
	memcpy(*output_p, Data, Index);
	(*output_p)[Index] = '\0';

	return 0;
}
