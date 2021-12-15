#include "cache.h"

/*
int cache_name_compare(const char *name1, int len1, 
					   const char *name2, int len2)

													(1) 取 公共长度(即 短者长度 )
												 /	
												|	(2) memcmp 公共长度 的 bytes
	按 字典顺序 比较 2个 字符串(filename) 大小 
												|	(3) 不等, 则 比较结果由 memcmp 返回值 (< >) 得到 (< >) -> 提前 返回 memcmp 返回值
												 \
													(4) 相等, 则 谁长谁大 -> 返回 -1 / 1 ( str1 < / > str2)

*/
static int cache_name_compare(const char *name1, int len1, const char *name2, int len2)
{
	// (1) 
	int len = len1 < len2 ? len1 : len2;
	int cmp;

	// (2)
	cmp = memcmp(name1, name2, len);
	
	// (3)
	if (cmp) // cmp != 0
		return cmp;
		
	// (4) 
	// below: cmp == 0
	if (len1 < len2)
		return -1;
	if (len1 > len2)
		return 1;
	return 0;
}

/*
									
int cache_name_pos(const char *name,   
				   int namelen)		    						  
											   
	功能
		对 变更文件信息, 按 filename/filepath 在 `变更文件信息 pointer array 所管理的 进程虚拟内存 和 堆内存` 中 查询 是否 existing match?
			二分法, 每次对 目标变更文件信息 与 `pointer array 中 mid_pos 所管理的 变更文件信息`, 
				基于 `变更文件 path( + path长度)` 按 `字典顺序 比较`, 查找 要 replace(existing match) 或 insert(don't existing match) 的 position

	(1) 取 mid 		
			
	(2) cache_name_compare(name, namelen, ce->name, ce->namelen)
	
				      == 0 => match -> return (-posToBeUpdate - 1 == -mid - 1 )
				   /	
	(3) 比较 结果 |
				  |	  < 0 => (not match) => last  指针前移 ( last = mid )     \
				   \											 			    until 找到要插入的位置 / last == first (<=> loop 条件: first < last ) -> return (posToBeInsert == first)
					  > 0 => (not match) => first 指针后移 ( first = mid + 1) /
	
	
			 - posToBeUpdate ( == mid) - 1, existing match
		   /
	return  
		   \
			 posToBeInsert == first       , don't existing match
			 
	note: 二分区间 [first, last) 初始为 [0, active_nr): 左闭(可取到) 右开(取不到, 可能因为 上次迭代 已取过)
*/
static int cache_name_pos(const char *name, int namelen)
{
	int first, last;

	first = 0;
	last = active_nr;
	
	// 假定 二分区间 为 [first, last) = 左闭(可取到) 右开(取不到, 可能因为上次迭代已取过)
	// first + last == 偶 / 奇 => mid = (first + last) >> 1 余 0 / 1
	// => if value[cur] < (> / ==) value[mid] -> target_pos 在 mid 左 (右 / 本身) -> last = mid = 开 ( first = mid + 1 = 闭 / exit)
	// -> [first, last) = [0, 1) -> mid = 0 
	
	// last loop: 
	// 		last == first + 1 => mid == first => < / > / == 时, last == mid == first / first == mid + 1 == last => return first(开) 
	//   或 cmp == 0 => 找到 => 返回负数 -mid - 1 => 要更新的 位置 为 -( -mid - 1) - 1 = mid
	while (last > first) {
		// (1)
		int next = (last + first) >> 1; // mid
		struct cache_entry *ce = active_cache[next];
		
		// (2)
		int cmp = cache_name_compare(name, namelen, ce->name, ce->namelen);
		
		// (3-1)
		if (!cmp) // == 0
			return -next-1;
			
		// (3-2)
		if (cmp < 0) { // < 0
			last = next;
			continue;
		}
		
		// (3-3) > 0
		first = next+1;
	}
	return first;
}

// 从 pointer array (active_cache) 中 删除 对 (相应)变更文件信息 的 管理
static int remove_file_from_cache(char *path) // path: 变更文件 path
{
	int pos = cache_name_pos(path, strlen(path));
	if (pos < 0) {
		pos = -pos-1;
		active_nr--;
		if (pos < active_nr)
			memmove(active_cache + pos, active_cache + pos + 1, (active_nr - pos - 1) * sizeof(struct cache_entry *));
	}
}

/*
																					while: cache_name_compare(name, namelen,
																				 /							  ce->name, ce->namelen)
																			    /
											pos = cache_name_pos(ce->name,     /  二分法  
										 /                       ce->namelen)								 
										/										 
add_cache_entry(struct cache_entry *ce)     							   prt replace 到 变更文件信息 pointer array 中 合适 position
				ce: malloc 堆内存 ptr	\							     /
										 \							    / 是
											由 pos 判 existing mathch ? 
																	    \ 否
																	     \
																		   ptr insert
																		   
	功能
		把 给 `变更文件信息` 新申请的 `堆内存 pointer` replace 或 insert 到 `变更文件信息 pointer array` 中 合适位置 (基于 `变更文件 path` 按 `字典顺序` 比较)
	
	(1) cache_name_pos(ce->name, ce->namelen) 
			对 变更文件信息, 按 filename/filepath 在 `变更文件信息 pointer array 所管理的 进程虚拟内存 和 堆内存` 中 查询 是否 existing match?
			二分法, 每次对 目标变更文件信息 与 `pointer array 中 mid_pos 所管理的 变更文件信息`, 
				基于 `变更文件 path( + path长度)` 按 `字典顺序 比较`, 查找 要 replace(existing match) 或 insert(don't existing match) 的 position
			
	(2) yes
		-> replace it -> 提前 return
	
	(3) otherwise
	   (3-1) 若 `变更文件信息 pointer array` 已满载, 则 realloc 更大的 空间 
	   
	   (3-2) 将 `变更文件信息 ptr` insert 到 `变更文件信息 pointer array` 中 相应 position:
			memmove (内存移动 以 腾出 1 个位置 ) 
			active_cache[pos] = ce
	    
		-> return
*/
static int add_cache_entry(struct cache_entry *ce)
{
	int pos;

	//(1)  
	pos = cache_name_pos(ce->name, ce->namelen);
	
	// (2) 存在 match -> replace
	/* existing match? Just replace it */
	if (pos < 0) {
		active_cache[-pos-1] = ce;
		return 0;
	}

	// (3)  otherwise
	// (3-1)
	/* Make sure the array is big enough .. */
	if (active_nr == active_alloc) {
		active_alloc = alloc_nr(active_alloc);
		active_cache = realloc(active_cache, active_alloc * sizeof(struct cache_entry *));
	}

	// (3-2) 
	/* Add it in.. */
	active_nr++;
	if (active_nr > pos)
		memmove(active_cache + pos + 1, active_cache + pos, (active_nr - pos - 1) * sizeof(ce));
	active_cache[pos] = ce;
	return 0;
}

/*
static int index_fd(const char 		    *path, 
				    int 				 namelen, 
					struct cache_entry 	*ce, 
					int 				 fd, 
					struct stat 		*st)

	功能	
								   blob original object 的 header 填值 + 压缩 									求 sha1 值 -> 填到 `变更文件信息 (新)堆内存` 的 sha1 字段					
								 /												 \							  /
		变更文件 content -> mmap 												    压缩输出 buf / blob object 
								 \												 /                            \ 
								   mmap 出的 虚拟内存 中 `变更文件 content 压缩` 								write `压缩输出 buf (即 blob object )` 到 `sha1 object 文件`
	
	(1) mmap: 变更文件内容 mmap 映射到 进程虚拟内存
	
	(2) 元数据 (metadata, 即 `blob original object` 的 header = type + size) 字段 填值 + zlib 压缩
	(3) 进程虚拟内存 中 变更文件 -> zlib 压缩
	
	(4) 求 压缩输出 的 sha1 值 -> 填到 `变更文件信息 (新)堆内存` 的 sha1 字段
	
	(5) write_sha1_buffer(ce->sha1, out, stream.total_out);
		write 压缩输出 buf ( content == sha1 object == blob object)` 到 `sha1 object 文件`				  
*/
static int index_fd(const char *path, int namelen, struct cache_entry *ce, int fd, struct stat *st)
{
	z_stream stream;
	int max_out_bytes = namelen + st->st_size + 200;
	void *out = malloc(max_out_bytes);
	void *metadata = malloc(namelen + 200);
	
	// (1)
	void *in = mmap(NULL, st->st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	SHA_CTX c;

	close(fd);
	if (!out || (int)(long)in == -1)
		return -1;

	memset(&stream, 0, sizeof(stream));
	deflateInit(&stream, Z_BEST_COMPRESSION);

	/*
	 * ASCII size + nul byte
	 */	
	// (2) 
	stream.next_in = metadata;
	stream.avail_in = 1+sprintf(metadata, "blob %lu", (unsigned long) st->st_size);
	stream.next_out = out;
	stream.avail_out = max_out_bytes;
	while (deflate(&stream, 0) == Z_OK)
		/* nothing */;

	/*
	 * File content
	 */
	// (3) 
	stream.next_in = in;
	stream.avail_in = st->st_size;
	while (deflate(&stream, Z_FINISH) == Z_OK)
		/*nothing */;

	deflateEnd(&stream);
	
	// (4)
	SHA1_Init(&c);
	SHA1_Update(&c, out, stream.total_out);
	SHA1_Final(ce->sha1, &c);

	// (5)
	return write_sha1_buffer(ce->sha1, out, stream.total_out);
}

/* 

												(1) remove_file_from_cache(path)
											 /
											| 若 同时存在 另一进程 正在 read_cache()		
										   		
											|	(2) cache_entry_size(namelen)
static int add_file_to_cache(char *path)  
	update-cache.o 入参中各 变更文件 path 	|									   
											
											|	(3) malloc `变更文件信息` 的 `堆内存` (ce) 
											
											|																	    blob original object 的 header 填值 + 压缩 									求 sha1 值 -> 填到 `变更文件信息 (新)堆内存` 的 sha1 字段					
																												  /												 \							  /
											|	(4) index_fd(path, namelen, ce, fd, &st): 变更文件 content -> mmap 												    压缩输出 buf / blob object 
																												  \												 /                            \ 
											|																	    mmap 出的 虚拟内存 中 `变更文件 content 压缩` 								write_sha1_buffer(ce->sha1, out, stream.total_out)
											 \																																						: write `压缩输出 buf (即 blob object )` 到 `sha1 object 文件`																									
												(5) add_cache_entry(ce)
														|
														|																			while: cache_name_compare(name, namelen,
														|																		 /							  ce->name, ce->namelen)
														|																		/
														|									pos = cache_name_pos(ce->name,     /  二分法  
														|								 /                       ce->namelen)								 
														|								/										 
												add_cache_entry(struct cache_entry *ce)     							   prt replace 到 变更文件信息 pointer array 中 合适 position
																ce: malloc 堆内存 ptr	\							     /
																						 \							    / 是
																							由 pos 判 existing mathch ? 
																														\ 否
																														 \
																														   ptr insert												
												
												
												
												
												
	功能
										 生成 blob object
								       /
		据 变更文件 path ( + content ) 
									   \
									     生成 1 条 `变更文件信息` -> 堆内存 ptr replace/insert 到 pointer array(active_cache) 


	(1) 打开 变更文件 + 检查文件状态
		若 同时存在 另一进程 正在 read_cache(), 则
		remove_file_from_cache(path)
			从 pointer array (active_cache) 中 删除 对 (相应)变更文件信息 的 管理
		
		note: 若同时存在2个进程, 一个 read_cache(), 而 另一个 add_file_to_cache(char *path), 则 该 变更文件信息 败坏 -> 不被 pointer 管理 -> 不会写到 新 index 文件
		
	(2) cache_entry_size(namelen) 
		据 `变更文件 path 的 长度` 求 当前 `变更文件信息` 的 size

	(3) malloc `申请` 该 `变更文件信息` 的 `堆内存` (ce) 
	    + 据 `变更文件 path` 及 `变更文件 状态` 填 变更文件信息 中 `0 长数组` 和 `除 sha1 之外的 字段` 	

	(4) index_fd(path, namelen, ce, fd, &st)
								   blob original object 的 header 填值 + 压缩 									求 sha1 值 -> 填到 `变更文件信息 (新)堆内存` 的 sha1 字段					
								 /												 \							  /
		变更文件 content -> mmap 												    压缩输出 buf / blob object 
								 \												 /                            \ 
								   mmap 出的 虚拟内存 中 `变更文件 content 压缩` 								write `压缩输出 buf (即 blob object )` 到 `sha1 object 文件`
	

	(5) add_cache_entry(ce)	
		把 给 `变更文件信息` 新申请的 `堆内存 pointer` replace 或 insert 到 `变更文件信息 pointer array` 中 合适位置 (基于 `变更文件 path` 按 `字典顺序` 比较)
	
		
 */
static int add_file_to_cache(char *path)
{
	int size, namelen;
	struct cache_entry *ce;
	struct stat st;
	int fd;
	
	// (1)
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT)
			return remove_file_from_cache(path);
		return -1;
	}
	if (fstat(fd, &st) < 0) {
		close(fd);
		return -1;
	}
	namelen = strlen(path);
	
	// (2)
	size = cache_entry_size(namelen);
	
	// (3)
	ce = malloc(size);
	memset(ce, 0, size);
	memcpy(ce->name, path, namelen);
	ce->ctime.sec = st.st_ctime;
	ce->ctime.nsec = st.st_ctim.tv_nsec;
	ce->mtime.sec = st.st_mtime;
	ce->mtime.nsec = st.st_mtim.tv_nsec;
	ce->st_dev = st.st_dev;
	ce->st_ino = st.st_ino;
	ce->st_mode = st.st_mode;
	ce->st_uid = st.st_uid;
	ce->st_gid = st.st_gid;
	ce->st_size = st.st_size;
	ce->namelen = namelen;

	// (4)
	if (index_fd(path, namelen, ce, fd, &st) < 0)
		return -1;

	// (5)
	return add_cache_entry(ce);
}

/*
static int 
write_cache(int 				 newfd, 
			struct cache_entry **cache, 
			int 			     entries)	
	功能
		将 actice_cache 所管理的 `进程虚拟内存 和 堆内存(新申请的)` 中的 `变更文件信息` 写到 index.lock	

	(1) update cache header: 
		(1-1) 非 sha1 字段: 直接填
		(1-2)    sha1 字段: 遍历 变更文件信息 + SHA1_Update(&c, ce, size) -> SHA1_Final(hdr.sha1, &c)
			     => 该 sha1 值 是 各 变更文件信息 累加计算出的
		
	(2) write cache header 到 index.lock
	
	(3) 遍历 write 各 变更文件信息 到 index.lock
	
	return 
		0 , success
		-1, fail
*/
static int write_cache(int newfd, struct cache_entry **cache, int entries)
{
	SHA_CTX c;
	struct cache_header hdr;
	int i;

	// (1-1) 
	hdr.signature = CACHE_SIGNATURE;
	hdr.version = 1;
	hdr.entries = entries;

	// (1-2) 
	SHA1_Init(&c);
	SHA1_Update(&c, &hdr, offsetof(struct cache_header, sha1));
	for (i = 0; i < entries; i++) {
		struct cache_entry *ce = cache[i];
		int size = ce_size(ce);
		SHA1_Update(&c, ce, size);
	}
	SHA1_Final(hdr.sha1, &c);

	// (2) 
	if (write(newfd, &hdr, sizeof(hdr)) != sizeof(hdr))
		return -1;

	// (3)
	for (i = 0; i < entries; i++) {
		struct cache_entry *ce = cache[i];
		int size = ce_size(ce);
		if (write(newfd, ce, size) != size)
			return -1;
	}
	return 0;
}		

/*
 * We fundamentally don't like some paths: we don't want
 * dot or dot-dot anywhere, and in fact, we don't even want
 * any other dot-files (.dircache or anything else). They
 * are hidden, for chist sake.
 *
 * Also, we don't want double slashes or slashes at the
 * end that can make pathnames ambiguous. 
 */
/*					
					 含 . 或 ..
				   /
	不允许的路径   - dot-files (如 .dircache) -> 它们是 隐藏文件
				   \
					 双斜杠 / 在末尾的单斜杠 -> 使 pathname 模糊不清
*/
static int verify_path(char *path)
{
	char c;

	goto inside;
	for (;;) {
		if (!c) // c == '\0'
			return 1;
		if (c == '/') {
inside:
			c = *path++;
			if (c != '/' && c != '.' && c != '\0')
				continue;
			return 0;
		}
		c = *path++;
	}
}

/*
1. 各 `变更文件信息` 如何 `replace/insert` 到 `index 文件` ?
	(1) index 文件 -> content mmap 到 进程 虚拟内存空间 
		-> 各 变更文件信息 相应的 `虚拟 memory block` 用 (变更文件信息 的) pointer array 指向/管理
		
	(2) 对 新增的 各 变更文件信息 -> malloc `堆 memory block` 
		-> ptr 也 replace/insert 到 pointer array 中 合适 position (按 所指 变更文件信息 的 变更文件名 的 字典顺序) 以 管理之

	(3) pointer array 管理/所指 的 各 memory block 内容 write 到 index.lock 文件
		-> index.lock 文件 `更名` 为 index 文件
	
2. update.c
	(1) read_cache()
 /			index 文件 mmap + 变更文件信息 pointer array 各 pointer 指向/管理 `进程虚拟内存中 各 变更文件信息 的 memory block`. 失败, 则 报错并退出
|		
|	(2) 打开 index.lock 文件, 以 O_CREAT | O_EXCL 方式, 
|		以 保证 `同时只允许 1 个 进程` 将 `变更文件信息` 写入 index.lock; 否则, 报错并退出
 \	
	(3) `遍历` update-cache.o 入参中 各 `变更文件 path`
		(3-1) verify_path(path)
	 /		      检查 变更文件 path, 不允许 点(隐藏文件), 双斜杠(使路径模棱两可) 等 
	|
	|	(3-2) add_file_to_cache(path) 
				  若失败, 则 报错 -> unlink(".dircache/index.lock"): 删除 index.lock 文件 (=> 而不影响 index 文件) -> 退出
	|			
		(3-3) write_cache(newfd, active_cache, active_nr)
	| 		      将 actice_cache 所管理的 `进程虚拟内存 和 堆内存(新申请的)` 中的 `变更文件信息` 写到 index.lock	
	 \
 		(3-4) rename(".dircache/index.lock", ".dircache/index")	
				  index.lock 文件 重命名 为 index	
	
*/
int main(int argc, char **argv)
{
	int i, newfd, entries;

	// (1)
	entries = read_cache();
	if (entries < 0) {
		perror("cache corrupted");
		return -1;
	}

	// (2)
	newfd = open(".dircache/index.lock", O_RDWR | O_CREAT | O_EXCL, 0600);
	if (newfd < 0) {
		perror("unable to create new cachefile");
		return -1;
	}
	
	// (3)
	for (i = 1 ; i < argc; i++) {
		char *path = argv[i]; // path: update-cache.o 入参中各 变更文件 path
		
		if (!verify_path(path) ) {
			fprintf(stderr, "Ignoring path %s\n", argv[i]);
			continue;
		}
		
		if (add_file_to_cache(path)) { // 总 return 0
			fprintf(stderr, "Unable to add %s to database\n", path);
			goto out;
		}
	}
	if (!write_cache(newfd, active_cache, active_nr) && !rename(".dircache/index.lock", ".dircache/index"))
		return 0;
out:
	unlink(".dircache/index.lock");
}
