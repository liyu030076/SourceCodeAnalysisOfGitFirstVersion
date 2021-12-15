#include "cache.h"
/*

1. 4 大 全局变量 初始化

每次调 read-cache.o -> 都 相应 初始化 为 NULL 或 0
*/
const char *sha1_file_directory = NULL;
struct cache_entry **active_cache = NULL;
unsigned int active_nr = 0, active_alloc = 0;

void usage(const char *err)
{
	fprintf(stderr, "read-tree: %s\n", err);
	exit(1);
}

// hex -> 10 进制 数 (decimal number)
static unsigned hexval(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return ~0;
}

/*
int get_sha1_hex(char *hex, unsigned char *sha1)
	功能
		将 `sha1 值` 从 输入形式 (40 个 hex 的 字符串 -> 以 char* 指向 ) 转化为 存储形式 (20 个 Bytes 的 unsigned char array -> 以 unsigned char * 指向)
*/
int get_sha1_hex(char *hex, unsigned char *sha1)
{
	int i;
	for (i = 0; i < 20; i++) {
		unsigned int val = ( hexval(hex[0]) << 4) | hexval(hex[1] );
		
		if (val & ~0xff) // & 0x..00 ！= 0 => 超过 最低 Byte 的部分不为 0
			return -1;
		
		*sha1++ = val;
		
		hex += 2;
	}
	return 0;
}

// sha1 从 存储形式 ( 20 Bytes 的 char []: unsigned char * ) -> 字符串形式 ( 40 个 hex 字符: char * )
char * sha1_to_hex(unsigned char *sha1)
{
	static char buffer[50];
	static const char hex[] = "0123456789abcdef";
	char *buf = buffer;
	int i;

	for (i = 0; i < 20; i++) {
		unsigned int val = *sha1++;
		*buf++ = hex[val >> 4];
		*buf++ = hex[val & 0xf];
	}
	return buffer;
}

/*
 * NOTE! This returns a statically allocated buffer, so you have to be
 * careful about using it. Do a "strdup()" if you need to save the
 * filename.
 */
/* 
char * sha1_file_name(unsigned char *sha1)
	功能
		由 `sha1 值 (20 Bytes 的 char [] -> 用 unsigned char * 指向)` 解析 `sha1 object` 的 filename(这里指 filepath == ".dircache/objects/../...")
		
	return 
		filename 所占 (malloc) 内存 的 指针 (static char*)
 
*/ 
char *sha1_file_name(unsigned char *sha1)
{
	int i;
	static char *name, *base; // static 变量 初始化为 0

	if (!base) {
		char *sha1_file_directory = getenv(DB_ENVIRONMENT) ? : DEFAULT_DB_ENVIRONMENT;
		int len = strlen(sha1_file_directory);
		base = malloc(len + 60);
		memcpy(base, sha1_file_directory, len);
		memset(base+len, 0, 60);
		base[len] = '/';
		base[len+3] = '/';
		name = base + len + 1;
	}
	
	// name = "01/23456789..." 40 位 sha1/hash/hex 值
	for (i = 0; i < 20; i++) {
		static char hex[] = "0123456789abcdef"; // => SIZE = 16
		unsigned int val = sha1[i]; // unsigned int = 16 + 8 + 4 ( *pos++ ) + 4 ( *pos ) 位
		char *pos = name + i*2 + (i > 0);
		*pos++ = hex[val >> 4]; // == hex[(val >> 4) & 0xf] 取出 第 4-8 位 相应的 char 值
		*pos = hex[val & 0xf];  // 取出 第 0-3 位 相应的 char 值
	}
	return base;
}

/*
											    sha1_file_name(sha1)
											 /
											/
void * read_sha1_file(unsigned char *sha1, 	——  mmap ( <- fstat(fd, &st) <- open )
					  char 			*type,  \										header 中 "type/size"  读 (sscanf) 到 type/size 所指 内存
					  unsigned long *size)   \									  /
												zlib 解压 出 sha1 original object
																				  \
																					有效 content 放到 malloc 堆内存 -> ptr 返回  
																					
																					
	功能									
																								header 中 "type/size"  读 (sscanf) 到 type/size 所指 内存
																							/
		sha1 -> sha1 object path -> open + fstat + mmap -> zlib 解压 -> sha1 original object
																							\
																								有效 content 放到 malloc 堆内存 -> ptr 返回
	
	(1) sha1_file_name(sha1)
			sha1 -> sha1 object path
			
	(2) open + fstat(fd, &st) 
	    + mmap sha1 object file 到 进程虚拟内存
	
	(3) zlib 解压 出 sha1 original object
		(3-1) 从 zlib 解压出的 header 读 (sscanf) type 和 size 所指内存
		(3-2) malloc 申请 sha1 original object 有效 content 大小 的 内存
		(3-3) 将 zlib 解压出的 有效 conetnt memcpy 到 malloc 的 堆内存
*/
void * read_sha1_file(unsigned char *sha1, char *type, unsigned long *size)
{
	z_stream stream;
	char buffer[8192];
	struct stat st;
	int i, fd, ret, bytes;
	void *map, *buf;
	
	// (1) 
	char *filename = sha1_file_name(sha1);

	// (2) 
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		perror(filename);
		return NULL;
	}
	if (fstat(fd, &st) < 0) {
		close(fd);
		return NULL;
	}
	
	map = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (-1 == (int)(long)map)
		return NULL;

	// (3) 
	/* Get the data stream */
	memset(&stream, 0, sizeof(stream));
	stream.next_in = map;
	stream.avail_in = st.st_size;
	stream.next_out = buffer;
	stream.avail_out = sizeof(buffer);

	inflateInit(&stream);
	
	ret = inflate(&stream, 0);
	
	// (3-1) 
	if (sscanf(buffer, "%10s %lu", type, size) != 2)
		return NULL;
	bytes = strlen(buffer) + 1; // blob original object header 长度
	
	
	// (3-2)
	buf = malloc(*size);
	if (!buf)
		return NULL;

	// (3-3) 
	memcpy(buf, buffer + bytes, stream.total_out - bytes);
	bytes = stream.total_out - bytes; // blob original object 有效 content 长度
	if (bytes < *size && ret == Z_OK) {
		stream.next_out = buf + bytes;
		stream.avail_out = *size - bytes;
		while (inflate(&stream, Z_FINISH) == Z_OK)
			/* nothing */;
	}
	inflateEnd(&stream);
	
	return buf;
}

/*
										buf: sha1 original object 的 malloc 内存													
									 /	
									/														
int write_sha1_file(char    *buf, 	——	zlib 压缩
				    unsigned len)	\
									 \									 filename = sha1_file_name(sha1)
									  \								   /    
										write_sha1_buffer(sha1,       /
														  compressed, —— fd = open
														  size)  	  \
																	   \ 
													buf == compressed	\
																		  write(fd, buf, size) //  write 到 sha1 object 文件 
																				
																					  
		
	功能
		sha1 original object (输入 buf / malloc 内存) -> zlib 压缩 > sha1 object ( 输出 buf / malloc 内存 ) -> 写到 `sha1 object 文件`  
	
	(1) malloc 内存 for zlib 压缩输出的 sha1 object
	(2) zlib 压缩
	(3) write_sha1_buffer(sha1, compressed, size) 将 `sha1 object` 从 `压缩输出 buf` 写到 `sha1 object 文件`  
*/
int write_sha1_file(char *buf, unsigned len)
{
	int size;
	char *compressed;
	z_stream stream;
	unsigned char sha1[20];
	SHA_CTX c;

	/* Set it up */
	memset(&stream, 0, sizeof(stream));
	deflateInit(&stream, Z_BEST_COMPRESSION);
	size = deflateBound(&stream, len);
	
	// (1) 
	compressed = malloc(size);

	// (2) 
	/* Compress it */
	stream.next_in = buf;
	stream.avail_in = len;
	stream.next_out = compressed;
	stream.avail_out = size;
	while (deflate(&stream, Z_FINISH) == Z_OK)
		/* nothing */;
	deflateEnd(&stream);
	size = stream.total_out;

	/* Sha1.. */
	SHA1_Init(&c);
	SHA1_Update(&c, compressed, size);
	SHA1_Final(sha1, &c);

	// (3)
	if (write_sha1_buffer(sha1, compressed, size) < 0)
		return -1;
	printf("%s\n", sha1_to_hex(sha1));
	return 0;
}

/* 										      
write_sha1_buffer(unsigned char *sha1,     
				  void          *buf, 	 
				  unsigned int   size) 		 

	功能
		将 `sha1 object` 从 `压缩输出 buf` 写到 `sha1 object 文件`  

	(1) sha1_file_name(sha1): 由 `sha1 值` 解析 `sha1 object` 的 filename(这里指 filepath == ".dircache/objects/../...")
	(2) fd = open(filename, O_WRONLY | O_CREAT | O_EXCL): `写 方式 打开` sha1 object 文件, O_CREAT | O_EXCL 方式 保证 同时只有1个进程 打开文件
	(3) write(fd, buf, size): `sha1 original object` 的 压缩 输出 (buf) 写到 `sha1 object 文件`					
						
*/
int write_sha1_buffer(unsigned char *sha1, void *buf, unsigned int size)
{
	// (1) 
	char *filename = sha1_file_name(sha1);
	int i, fd;

	// (2) 
	fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, 0666);
	if (fd < 0)
		return (errno == EEXIST) ? 0 : -1;
	
	// (3) 
	write(fd, buf, size);
	close(fd);
	return 0;
}

static int error(const char * string)
{
	fprintf(stderr, "error: %s\n", string);
	return -1;
}

/*
verify_hdr(struct cache_header *hdr, 
		   unsigned long        size)

	功能: 检查 index 文件 的 mmap 是否正确: 
	(1) header 中 除 sha1 之外的 字段: 因为 填的是 固定值, 可直接判
	
	(2) header 中 sha1 字段: 
		index 文件 size - header_size = effective_content_size 
		-> + effective_content (虚拟内存 content)
		-> sha1 压缩 
		-> sha1 值 
		-> 与 header 中 sha1 值 比较
*/

/*
	input:
		hdr : mmap 出的 进程虚拟内存 中 header 指针
		size: index 文件 size
		
	return:
		0 , mmap 正确
		-1, 出错
*/
static int verify_hdr(struct cache_header *hdr, unsigned long size)
{
	SHA_CTX c;
	unsigned char sha1[20];

	if (hdr->signature != CACHE_SIGNATURE)
		return error("bad signature");
	if (hdr->version != 1)
		return error("bad version");
	SHA1_Init(&c);
	SHA1_Update(&c, hdr, offsetof(struct cache_header, sha1));
	SHA1_Update(&c, hdr+1, size - sizeof(*hdr));
	SHA1_Final(sha1, &c);
	if (memcmp(sha1, hdr->sha1, 20))
		return error("bad header sha1");
	return 0;
}

/*
int read_cache(void)
	功能:  将 index 文件 mmap 映射到 进程虚拟内存, 
		   用 `变更文件信息 pointer array` 各 `pointer` 指向/管理 `虚拟内存中 各 变更文件信息 的 memory block` 

	(1) 保证 同时只有1个进程 read_cache(), 即 打开 index 文件:
		方法: 利用 每次 调 read_cache() 时, 必先 初始化 (其所在 .c 文件 最前面的) 全局变量 active_cache ( 变更文件信息 pointer array 的 指针 ) 为 NULL
			  => 若 active_cache != NULL => 已有另一进程 调 read_cache() => 报错 + return 
		
	(2) index 文件 <-> 进程 虚拟内存 的 mmap

	(3) 取 `index 文件` 所映射的 `进程 虚拟内存地址`
		
	(4) verify_hdr(struct cache_header *hdr, unsigned long size)
		检查 index 文件 的 mmap 是否正确: 
		
	(5)	
									 内存申请: 更大一些, 以容纳 更多的 管理 pointer( 比 `当前 index 文件` 中 变更文件信息 数 `更多`)
								   /	active_alloc = alloc_nr(active_nr);
								  /		active_cache = calloc(active_alloc, sizeof(struct cache_entry *) );
		变更文件信息 pointer array
								  \
								   \
									 各 ptr 赋值: 指向 `虚拟内存` 中 `各 变更文件信息`
										ce = map + offset
		`								active_cache[i] = ce; 
										offset = offset + ce_size(ce);		
*/

/*
			 `index 文件` 中 `变更文件数`
		   /	
	return 
		   \
			 -1, index 文件 header 检查错误
					
	cache, 即 index 文件: .dircache/index 

	sha1_file_directory, 即 object(SHA-1) 文件目录: 默认 .dircache/objects
*/
int read_cache(void)
{
	int fd, i;
	struct stat st;
	unsigned long size, offset;
	void *map;
	struct cache_header *hdr;

	errno = EBUSY;
	//(1) 
	if (active_cache)
		return error("more than one cachefile");
	errno = ENOENT;
	// 取 object 目录
	sha1_file_directory = getenv(DB_ENVIRONMENT);
	if (!sha1_file_directory)
		sha1_file_directory = DEFAULT_DB_ENVIRONMENT;
	if (access(sha1_file_directory, X_OK) < 0)
		return error("no access to SHA1 file directory");
	
	// (2) 
	fd = open(".dircache/index", O_RDONLY);
	if (fd < 0)
		return (errno == ENOENT) ? 0 : error("open failed");

	map = (void *)-1; // 初值, 为了 后续判是否 被改, 从而 判 mmap 是否 success
	if (!fstat(fd, &st)) { // fd -> file -> file state -> copy 到 st -> success
		map = NULL;
		size = st.st_size;
		errno = EINVAL;
		if (size > sizeof(struct cache_header)) // index 文件 以含 > 0 个 变更文件信息
			map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	}
	close(fd);
	if (-1 == (int)(long)map)
		return error("mmap failed");

	// (3)
	hdr = map;
	
	// (4)
	if (verify_hdr(hdr, size) < 0)
		goto unmap;

	// (5)
	active_nr = hdr->entries;
	
	// (5-1) 内存申请 更大一些(规律不细究)
	active_alloc = alloc_nr(active_nr);
	active_cache = calloc(active_alloc, sizeof(struct cache_entry *));

	offset = sizeof(*hdr); // 偏移掉 cache header
	// (5-2)
	for (i = 0; i < hdr->entries; i++) {
		struct cache_entry *ce = map + offset;
		offset = offset + ce_size(ce);
		active_cache[i] = ce;
	}
	return active_nr;

unmap:
	munmap(map, size);
	errno = EINVAL;
	return error("verify header failed");
}

