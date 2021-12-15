#ifndef CACHE_H
#define CACHE_H

#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/mman.h>

#include <openssl/sha.h> /* sha1 算法 */
#include <zlib.h>        /* zlib 算法 */

/*
 * Basic data structures for the directory cache
 *
 * NOTE NOTE NOTE! This is all in the native CPU byte format. It's
 * not even trying to be portable. It's trying to be efficient. It's
 * just a cache, after all.
 */

/*
	"DIRC":
		directory cache 的 数据结构:
			由 本地 CPU 字节格式决定 => not portable
			efficient
*/

#define CACHE_SIGNATURE 0x44495243	/* "DIRC" */
struct cache_header {
	unsigned int signature;
	unsigned int version;
	unsigned int entries;
	unsigned char sha1[20];
};

/*
 * The "cache_time" is just the low 32 bits of the
 * time. It doesn't matter if it overflows - we only
 * check it for equality in the 32 bits we save.
 */
/*
	"cache_time": 
		time 的 low 32 bits 
		overflows doesn't matter
*/
struct cache_time {
	unsigned int sec;
	unsigned int nsec;
};

/*
 * dev/ino/uid/gid/size are also just tracked to the low 32 bits
 * Again - this is just a (very strong in practice) heuristic that
 * the inode hasn't changed.
 */
 
/*
	dev/ino/uid/gid/size 也只 tracked to low 32 bits
	(very strong in practice) heuristic(探索式的) that inode hasn't changed.
*/
struct cache_entry {
	struct cache_time ctime;
	struct cache_time mtime;
	unsigned int st_dev;
	unsigned int st_ino;
	unsigned int st_mode;
	unsigned int st_uid;
	unsigned int st_gid;
	unsigned int st_size;
	unsigned char sha1[20];
	unsigned short namelen;
	unsigned char name[0]; /* 0 长数组 */
};

const char *sha1_file_directory;
struct cache_entry **active_cache;
unsigned int active_nr, active_alloc;

#define DB_ENVIRONMENT "SHA1_FILE_DIRECTORY"
#define DEFAULT_DB_ENVIRONMENT ".dircache/objects"

/*
cache_entry_size(len) 
	据 `变更文件 path 的 长度` 求 当前 `变更文件信息` 的 size
	
ce_size(ce)
	据 `变更文件信息 pointer` 求 当前 `变更文件信息` 的 size
	每个 `变更文件 path 的 长度(namelen)` 不同 -> 每个 `变更文件信息 size(长度)` 按 `8 Byte 倍数 上取整`
*/
#define cache_entry_size(len) ((offsetof(struct cache_entry,name) + (len) + 8) & ~7)
#define ce_size(ce) cache_entry_size((ce)->namelen)

/*		x = 			1	2	3	4	5	6 ...	15	16	17
		alloc_nr(x) =   25  27  28  30  31  33 ...  46  48  49
		
没发现规律, 多分配些
*/
#define alloc_nr(x) (((x)+16)*3/2)

/* Initialize the cache information */
extern int read_cache(void);

/* Return a statically allocated filename matching the sha1 signature */
extern char *sha1_file_name(unsigned char *sha1);

/* Write a memory buffer out to the sha file */
extern int write_sha1_buffer(unsigned char *sha1, void *buf, unsigned int size);

/* Read and unpack a sha1 file into memory, write memory to a sha1 file */
extern void * read_sha1_file(unsigned char *sha1, char *type, unsigned long *size);
extern int write_sha1_file(char *buf, unsigned len);

/* Convert to/from hex/sha1 representation */
extern int get_sha1_hex(char *hex, unsigned char *sha1);
extern char *sha1_to_hex(unsigned char *sha1);	/* static buffer! */

/* General helper functions */
extern void usage(const char *err);

#endif /* CACHE_H */
