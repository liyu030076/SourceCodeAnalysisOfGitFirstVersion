#include "cache.h"

static int unpack(unsigned char *sha1)
{
	void *buffer;
	unsigned long size;
	char type[20];
	
	// 
	buffer = read_sha1_file(sha1, type, &size);
	
	if (!buffer)
		usage("unable to read sha1 file");
	
	// 
	if (strcmp(type, "tree") )
		usage("expected a 'tree' node");
	
	//
	while (size) {
		int len = strlen(buffer)+1;
		unsigned char *sha1 = buffer + len;
		char *path = strchr(buffer, ' ')+1;
		unsigned int mode;
		
		if (size < len + 20 || sscanf(buffer, "%o", &mode) != 1)
			usage("corrupt 'tree' file");
		
		buffer = sha1 + 20;
		size -= len + 20;
		
		printf("%o %s (%s)\n", mode, path, sha1_to_hex(sha1) );
	}
	return 0;
}

/*
read-tree.c
(1) get_sha1_hex(argv[1], sha1)
	sha1 的 字符串形式 -> 存储形式 ( 20 Bytes char [] )

(2) unpack(unsigned char *sha1)
	1) sha1 值 -> sha1 original object 有效 content 放到 malloc 堆内存 -> ptr 返回
	
	2) 判 sha1 original object 的 header 中 type 是 "tree" 才可 继续
	
	3) 遍历: 输出 tree original object 中 各 item 的 mode / path / sha1 的 hex 值
		sscanf(buffer, "%o", &mode)        // %o 格式匹配 mode
		char *path = strchr(buffer, ' ')+1 // path 前 有 空格 => strchr
		printf("%o %s (%s)\n", mode, path, sha1_to_hex(sha1) );
	
*/
int main(int argc, char **argv)
{
	int fd;
	unsigned char sha1[20];

	if (argc != 2)
		usage("read-tree <key>");
	
	// (1)
	if (get_sha1_hex(argv[1], sha1) < 0)
		usage("read-tree <key>");
	
	sha1_file_directory = getenv(DB_ENVIRONMENT);
	if (!sha1_file_directory)
		sha1_file_directory = DEFAULT_DB_ENVIRONMENT;
	
	// (2)
	if (unpack(sha1) < 0)
		usage("unpack failed");
	
	return 0;
}
