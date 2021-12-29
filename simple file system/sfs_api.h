#ifndef SFS_API_H
#define SFS_API_H

#include "stdint.h"
#include "string.h"
#include "stdlib.h"
#include <stdbool.h>
#include "disk_emu.h"
#include "stdio.h"


#define MAGIC 0xACBD0005
#define INODE_BLOCKS_LEN (INODE_TABLE_LEN*sizeof(tb_inode)/BLOCK_SIZE + 1)
#define MAX_FILE_SIZE BLOCK_SIZE*(DIR_PTR_SIZE+INDIR_PTR_SIZE-1)
#define BITMAP_SIZE (NUM_BLOCKS / 8)
#define DIR_PTR_SIZE 12
#define INDIR_PTR_SIZE BLOCK_SIZE/sizeof(int)
#define DISK_FILENAME "sfs.disk"
#define INODE_TABLE_LEN 128
#define DIR_TABLE_LEN 128
#define DIR_BLOCKS_LEN (DIR_TABLE_LEN*sizeof(entry_dir)/BLOCK_SIZE + 1)
#define NUM_BLOCKS 4096
#define DATA_BLOCKS_LEN NUM_BLOCKS-2-INODE_BLOCKS_LEN
#define BLOCK_SIZE 1024
#define MAX_FILENAME_LEN 22



// You can add more into this file.

void mksfs(int);

int sfs_getnextfilename(char*);

int sfs_getfilesize(const char*);

int sfs_fopen(char*);

int sfs_fclose(int);

int sfs_fwrite(int, const char*, int);

int sfs_fread(int, char*, int);

int sfs_fseek(int, int);

int sfs_remove(char*);

#endif
