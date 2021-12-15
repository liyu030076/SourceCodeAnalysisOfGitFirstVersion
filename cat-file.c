#include "cache.h"

/*
cat-file.c
	
	(1) get_sha1_hex(argv[1], sha1)			
 /		
|	(2) read_sha1_file(sha1, type, &size)
					
|	(3) mkstemp(template)
 \			
	(4) write(fd, buf, size)
			
	功能
		sha1 -> sha1 object -> zlib 解压 -> sha1 original object -> type 输出 + write 有效 content 到 (唯一)临时文件
			
	(1) get_sha1_hex(argv[1], sha1)
		将 `sha1 值` 从 输入形式 (40 个 hex 的 字符串 -> 以 char* 指向 ) 转化为 存储形式 (20 个 Bytes 的 unsigned char array -> 以 unsigned char * 指向)
		
	(2) read_sha1_file(sha1, type, &size)	

												header 中 "type/size"  读 (sscanf) 到 type/size 所指 内存
											/
		sha1 -> ... -> sha1 original object
											\
												有效 content 放到 malloc 的 堆内存 -> ptr 返回
									
	(3) mkstemp(template)
		在 系统中 以唯一 文件名 创建并打开 临时文件

	(4) write(fd, buf, size)
			write `sha1 original object` 有效 content (从 堆内存) 到 临时文件 

				
	input: sha1 值 的 输入形式: 40 个 hex 的 字符串
*/
int main(int argc, char **argv)
{
	unsigned char sha1[20];
	char type[20];
	void *buf;
	unsigned long size;
	char template[] = "temp_git_file_XXXXXX"; // note: template 的一种实现: char [] -? 
	int fd;

	// (1)
	if (argc != 2 || get_sha1_hex(argv[1], sha1) )
		usage("cat-file: cat-file <sha1>");
	
	// (2)
	buf = read_sha1_file(sha1, type, &size);
	if (!buf)
		exit(1);
	
	// (3)
	fd = mkstemp(template);
	if (fd < 0)
		usage("unable to create tempfile");
	
	// (4)
	if (write(fd, buf, size) != size)
		strcpy(type, "bad");
	
	printf("%s: %s\n", template, type);
}
