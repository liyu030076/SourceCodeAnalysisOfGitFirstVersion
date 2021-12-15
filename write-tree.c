#include "cache.h"

static int check_valid_sha1(unsigned char *sha1)
{
	char *filename = sha1_file_name(sha1);
	int ret;

	/* If we were anal, we'd check that the sha1 of the contents actually matches */
	ret = access(filename, R_OK);
	if (ret)
		perror(filename);
	return ret;
}

/*
static int prepend_integer(char *buffer, unsigned val, int i)
	buffer 前 40 Bytes 倒着存 tree original object `有效 content` 长度 的 个位、十位、...、最高位
	
input:
	val = tree original object `有效 content` 长度
	i = ORIG_OFFSET = 40
*/
static int prepend_integer(char *buffer, unsigned val, int i) 
{
	
	buffer[--i] = '\0';
	
	do {
		buffer[--i] = '0' + (val % 10);
		val /= 10;
	} while (val);
	
	return i;
}

#define ORIG_OFFSET (40)	/* Enough space to add the header of "tree <size>\0" */


/*
tree size 未知 => header size 未知 -> 但 40 Bytes 足够
	(1) read_cache()
	
	(2) 猜 1 个 size -> malloc
		size = entries * 40 + 400;
		buffer = malloc(size);
	
	(3) 遍历 cache 中 所有 变更文件信息
		1) check_valid_sha1(ce->sha1): filename = sha1_file_name(sha1) -> 判 file 是否存在
	
		2) 若 已写入 buf 的 大小 超过 所猜 size => realloc(buffer, size)
		
		3) 将 变更文件信息 的 mode + name + sha1 写 (sprintf) 到 buf 第 40 Bytes 开始 的 合适 pos
			offset += sprintf(buffer + offset, "%o %s", ce->st_mode, ce->name)
			memcpy(buffer + offset, ce->sha1)
	
	(4) buffer 前 40 Bytes 倒着存 tree original object `有效 content` size 的 个位、十位、...、最高位
		static int prepend_integer(char *buffer, unsigned val, int i)
	
	(5) type "tree" 写入 buffer
		memcpy(buffer+i, "tree ", 5);
	
	(6) tree original object -> zlib 压缩 -> tree object -> write 到 sha1 object 文件
		write_sha1_file(buffer, offset);
*/

int main(int argc, char **argv)
{
	unsigned long size, offset, val;
	
	// (1)
	int i, entries = read_cache();
	char *buffer;

	if (entries <= 0) {
		fprintf(stderr, "No file-cache to create a tree of\n");
		exit(1);
	}

	// (2)
	// 猜1个初值
	/* Guess at an initial size */
	size = entries * 40 + 400;
	buffer = malloc(size);
	offset = ORIG_OFFSET;

	// (3)
	for (i = 0; i < entries; i++) {
		struct cache_entry *ce = active_cache[i];
		
		// 1) 
		if (check_valid_sha1(ce->sha1) < 0)
			exit(1);
		
		// 2)
		if (offset + ce->namelen + 60 > size) {
			size = alloc_nr(offset + ce->namelen + 60);
			buffer = realloc(buffer, size);
		}
		
		// 3)
		offset += sprintf(buffer + offset, "%o %s", ce->st_mode, ce->name);
		
		buffer[offset++] = 0; // tree object 中 item (blob / tree object) 之间 以 '\0' 分隔
		
		// 4)
		memcpy(buffer + offset, ce->sha1, 20);
		
		offset += 20;
	}

	// offset - ORIG_OFFSET == tree original object `有效 content` 长度, ORIG_OFFSET == 预设的 前 40Bytes
	i = prepend_integer(buffer, offset - ORIG_OFFSET, ORIG_OFFSET); 
	i -= 5;
	
	//
	memcpy(buffer+i, "tree ", 5);

	buffer += i;
	offset -= i;

	// 
	write_sha1_file(buffer, offset);
	
	return 0;
}
