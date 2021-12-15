#include "cache.h"

/*
init-db.c
	(1) 取 sha1_dir ( 即 `objects 对象 / sha1 对象` 目录 )
		getenv(DB_ENVIRONMENT) 
			取 `环境变量` 中 字段 "SHA1_FILE_DIRECTORY" 的 值 sha1_dir ( 类型 为 char * )	
		-> 若没该字段 -> sha1_dir 设为 默认值 ".dircache/objects"
		
	(2) 在 sha1_dir 下 新建 00~ff 共 256 个 目录
		char* path = malloc(len + 40) -> memcpy(path, sha1_dir, len)
		-> for:
			sprintf(path+len, ...) -> mkdir(path,...)
*/
int main(int argc, char **argv)
{
	char *sha1_dir = getenv(DB_ENVIRONMENT), *path;
	int len, i, fd;

	if (mkdir(".dircache", 0700) < 0) {
		perror("unable to create .dircache");
		exit(1);
	}

	/*
	 * If you want to, you can share the DB area with any number of branches.
	 * That has advantages: you can save space by sharing all the SHA1 objects.
	 * On the other hand, it might just make lookup slower and messier. You
	 * be the judge.
	 */
	//(1)
	sha1_dir = getenv(DB_ENVIRONMENT);
	if (sha1_dir) {
		struct stat st;
		if (!stat(sha1_dir, &st) < 0 && S_ISDIR(st.st_mode))
			return;
		fprintf(stderr, "DB_ENVIRONMENT set to bad directory %s: ", sha1_dir);
	}

	/*
	 * The default case is to have a DB per managed directory. 
	 */
	sha1_dir = DEFAULT_DB_ENVIRONMENT;
	fprintf(stderr, "defaulting to private storage area\n");
	len = strlen(sha1_dir);
	if (mkdir(sha1_dir, 0700) < 0) {
		if (errno != EEXIST) {
			perror(sha1_dir);
			exit(1);
		}
	}
	
	//(2)
	path = malloc(len + 40);
	memcpy(path, sha1_dir, len);
	for (i = 0; i < 256; i++) {
		sprintf(path+len, "/%02x", i);
		if (mkdir(path, 0700) < 0) {
			if (errno != EEXIST) {
				perror(path);
				exit(1);
			}
		}
	}
	return 0;
}