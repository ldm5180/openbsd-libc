/*	$OpenBSD: fread.c,v 1.12 2014/05/01 16:40:36 deraadt Exp $ */
/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include "local.h"

#define MUL_NO_OVERFLOW	(1UL << (sizeof(size_t) * 4))

size_t
fread(void *buf, size_t size, size_t count, FILE *fp)
{
	size_t resid;
	char *p;
	int r;
	size_t total;

	/*
	 * Extension:  Catch integer overflow
	 */
	if ((size >= MUL_NO_OVERFLOW || count >= MUL_NO_OVERFLOW) &&
	    size > 0 && SIZE_MAX / size < count) {
		errno = EOVERFLOW;
		fp->_flags |= __SERR;
		return (0);
	}

	/*
	 * ANSI and SUSv2 require a return value of 0 if size or count are 0.
	 */
	if ((resid = count * size) == 0)
		return (0);
	FLOCKFILE(fp);
	_SET_ORIENTATION(fp, -1);
	if (fp->_r < 0)
		fp->_r = 0;
	total = resid;
	p = buf;

	if ((fp->_flags & __SNBF) != 0) {
		/*
		 * We know if we're unbuffered that our buffer is empty, so
		 * we can just read directly. This is much faster than the
		 * loop below which will perform a series of one byte reads.
		 */
		while (resid > 0 && (r = (*fp->_read)(fp->_cookie, p, resid)) > 0) {
			p += r;
			resid -= r;
		}
		FUNLOCKFILE(fp);
		return ((total - resid) / size);
	}

	while (resid > (r = fp->_r)) {
		(void)memcpy((void *)p, (void *)fp->_p, (size_t)r);
		fp->_p += r;
		/* fp->_r = 0 ... done in __srefill */
		p += r;
		resid -= r;
		if (__srefill(fp)) {
			/* no more input: return partial result */
			FUNLOCKFILE(fp);
			return ((total - resid) / size);
		}
	}
	(void)memcpy((void *)p, (void *)fp->_p, resid);
	fp->_r -= resid;
	fp->_p += resid;
	FUNLOCKFILE(fp);
	return (count);
}
