#include "cache.h"

#define MTIME_CHANGED	0x0001
#define CTIME_CHANGED	0x0002
#define OWNER_CHANGED	0x0004
#define MODE_CHANGED    0x0008
#define INODE_CHANGED   0x0010
#define DATA_CHANGED    0x0020

static int match_stat(struct cache_entry *ce, struct stat *st)
{
	unsigned int changed = 0;

	if (ce->mtime.sec  != (unsigned int)st->st_mtim.tv_sec ||
	    ce->mtime.nsec != (unsigned int)st->st_mtim.tv_nsec)
		changed |= MTIME_CHANGED;
	if (ce->ctime.sec  != (unsigned int)st->st_ctim.tv_sec ||
	    ce->ctime.nsec != (unsigned int)st->st_ctim.tv_nsec)
		changed |= CTIME_CHANGED;
	if (ce->st_uid != (unsigned int)st->st_uid ||
	    ce->st_gid != (unsigned int)st->st_gid)
		changed |= OWNER_CHANGED;
	if (ce->st_mode != (unsigned int)st->st_mode)
		changed |= MODE_CHANGED;
	if (ce->st_dev != (unsigned int)st->st_dev ||
	    ce->st_ino != (unsigned int)st->st_ino)
		changed |= INODE_CHANGED;
	if (ce->st_size != (unsigned int)st->st_size)
		changed |= DATA_CHANGED;
	return changed;
}

/*
	popen("diff -u -<ce->name>", w)
	fwrite(old_contents, old_size, 1, f);
*/
static void show_differences(struct cache_entry *ce, struct stat *cur,
							 void *old_contents, unsigned long long old_size)
{
	static char cmd[1000];
	FILE *f;

	snprintf(cmd, sizeof(cmd), "diff -u - %s", ce->name);
	
	f = popen(cmd, "w");
	fwrite(old_contents, old_size, 1, f);
	pclose(f);
}

/*
show-diff.c

(1) read_cache()
									
									1) stat(ce->name, &st): 将 `current 变更文件 (可能已变) state` copy 到 &st 所指内存
								 /	
     							|	2) match_stat(ce, &st): 比较  `current 变更文件 state` 与 `cache 中 相应的 文件 state`, 匹配 查找 是哪种 change

								|	3) unchanged: print ok -> continue
									
(2) 遍历 cache 中各 变更文件信息|									
												   输出 <变更文件名>: <sha1>
								|			     /  
											    /  	  
								\			   /	
									4) changed —   new = read_sha1_file(ce->sha1, type, &size)
											   \										
											    \										  f = popen("diff -u -<ce->name>", w)
											     \								        /
											       show_differences(ce, &st, new, size)
																				        \
																					      fwrite(old_contents == new, old_size, 1, f);
*/


int main(int argc, char **argv)
{
	// (1) 
	int entries = read_cache();
	int i;

	if (entries < 0) {
		perror("read_cache");
		exit(1);
	}
	
	// (2) 遍历 cache 中各 变更文件信息
	for (i = 0; i < entries; i++) {
		struct stat st;
		struct cache_entry *ce = active_cache[i];
		int n, changed;
		unsigned int mode;
		unsigned long size;
		char type[20];
		void *new;
		
		// (2-1) 
		if (stat(ce->name, &st) < 0) {
			printf("%s: %s\n", ce->name, strerror(errno));
			continue;
		}
		
		// (2-2) 
		changed = match_stat(ce, &st);
		
		// (2-2-1) unchanged: print ok -> continue
		if (!changed) {
			printf("%s: ok\n", ce->name);
			continue;
		}
		
		// (2-2-2) change 
		printf("%.*s:  ", ce->namelen, ce->name);
		for (n = 0; n < 20; n++)
			printf("%02x", ce->sha1[n]);
		printf("\n");
		
		// 2) 
		new = read_sha1_file(ce->sha1, type, &size);
		
		// 3)
		show_differences(ce, &st, new, size);
		
		free(new);
	}
	return 0;
}
