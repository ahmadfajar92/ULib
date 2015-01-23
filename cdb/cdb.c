/* Public domain. */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include "readwrite.h"
#include "error.h"
#include "seek.h"
#include "byte.h"
#include "cdb.h"

void cdb_free(struct cdb *c)
{
  if (c->map) {
	 munmap(c->map,c->size);
	 c->map = 0;
  }
}

void cdb_findstart(struct cdb *c)
{
  c->loop = 0;
}

void cdb_init(struct cdb *c,int fd)
{
  struct stat st;
  char *x;

  cdb_free(c);
  cdb_findstart(c);
  c->fd = fd;

  if (fstat(fd,&st) == 0)
	 if (st.st_size <= 0xffffffff) {
		x = mmap(0,st.st_size,PROT_READ,MAP_SHARED,fd,0);
		if (x + 1) {
	c->size = st.st_size;
	c->map = x;
		}
	 }
}

int cdb_read(struct cdb *c,char *buf,unsigned int len,uint32 pos)
{
  if (c->map) {
	 if ((pos > c->size) || (c->size - pos < len)) goto FORMAT;
	 byte_copy(buf,len,c->map + pos);
  }
  else {
	 if (seek_set(c->fd,pos) == -1) return -1;
	 while (len > 0) {
		int r;
		do
		  r = read(c->fd,buf,len);
		while ((r == -1) && (errno == error_intr));
		if (r == -1) return -1;
		if (r == 0) goto FORMAT;
		buf += r;
		len -= r;
	 }
  }
  return 0;

  FORMAT:
  errno = error_proto;
  return -1;
}

static int match(struct cdb *c,char *key,unsigned int len,uint32 pos)
{
  char buf[32];
  int n;

  while (len > 0) {
	 n = sizeof buf;
	 if (n > len) n = len;
	 if (cdb_read(c,buf,n,pos) == -1) return -1;
	 if (byte_diff(buf,n,key)) return 0;
	 pos += n;
	 key += n;
	 len -= n;
  }
  return 1;
}

#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define U_INTERNAL_TRACE(format,args...) \
      { char u_internal_buf[8192]; (void) sprintf(u_internal_buf, format"\n", args); \
        (void) write(2, u_internal_buf, strlen(u_internal_buf)); }
#else
#define U_INTERNAL_TRACE(format,args...)
#endif

int cdb_findnext(struct cdb *c,char *key,unsigned int len)
{
  char buf[8];
  uint32 pos;
  uint32 u;

	U_INTERNAL_TRACE("cdb_findnext(%p,%.*s,%u)",c,len,key,len)

  if (!c->loop) {
	 u = cdb_hash(key,len);
	 if (cdb_read(c,buf,8,(u % CDB_NUM_HASH_TABLE_POINTER) * 8) == -1) return -1;
	 uint32_unpack(buf + 4,&c->hslots);
	 if (!c->hslots) return 0;
	 uint32_unpack(buf,&c->hpos);
	 c->khash = u;
	 u /= CDB_NUM_HASH_TABLE_POINTER;
	 u %= c->hslots;
	 u <<= 3;
	 c->kpos = c->hpos + u;
  }

  while (c->loop < c->hslots) {
	 if (cdb_read(c,buf,8,c->kpos) == -1) return -1;
	 uint32_unpack(buf + 4,&pos);
	 if (!pos) return 0;
	 c->loop += 1;
	 c->kpos += 8;
	 if (c->kpos == c->hpos + (c->hslots << 3)) c->kpos = c->hpos;
	 uint32_unpack(buf,&u);
	 if (u == c->khash) {
		if (cdb_read(c,buf,8,pos) == -1) return -1;
		uint32_unpack(buf,&u);
		if (u == len)
	switch(match(c,key,len,pos + 8)) {
	  case -1:
		 return -1;
	  case 1:
		 uint32_unpack(buf + 4,&c->dlen);
		 c->dpos = pos + 8 + len;
		 return 1;
	}
	 }
  }

  U_INTERNAL_TRACE("not found",0)
  return 0;
}

int cdb_find(struct cdb *c,char *key,unsigned int len)
{
  cdb_findstart(c);
  return cdb_findnext(c,key,len);
}
