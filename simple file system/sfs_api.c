#include "sfs_api.h"


//-----------------------------------------------------defining struct---------------------------------------------------------
typedef struct tb_superblock{
    int magic;
    int inode_table_len;
    int file_system_size;
    int inode_root_dir;
    int block_size;
}tb_superblock;

typedef struct tb_inode{
    int is_empty;
    int size_inode;
    int c_link;
    int pt_indir;
    int pts_dir[DIR_PTR_SIZE];
}tb_inode;

typedef struct entry_dir{
    char filename[MAX_FILENAME_LEN];
    int inode_dir;
}entry_dir;

typedef struct entry_fd{
    int inode_fd;
    int pt_RW;
}entry_fd;

//-----------------------------------------------------defining input---------------------------------------------------------
entry_fd fdtb[INODE_TABLE_LEN];
tb_inode itb[INODE_TABLE_LEN];
tb_superblock superblock;
int bm[BITMAP_SIZE] = { [0 ... BITMAP_SIZE-1] = UINT8_MAX };
entry_dir root_dir[DIR_TABLE_LEN];
int pt_dir = -1;

//---------------------------------------------------helper functions------------------------------------------------------------

// free a bit by specifying its index
void free_bit(int idx){
    bm[idx/8] |= 1 << (idx%8);
}


// get the idnode from the directory with its file name, the return should be a inode number 
// return -1 if it is not in the dir
int get_inode_in_dir(char* filename){
    for(int i=0; i<DIR_TABLE_LEN; i++){
        if (!(strcmp(filename, root_dir[i].filename) == 0))continue;
        return root_dir[i].inode_dir;
    }
    return -1;
}


// get a free data block, return -1 if unable to do so
int get_free_data_block(){
    for(int i=INODE_TABLE_LEN+2; i<=NUM_BLOCKS-1; i++){
        int position;
        if (bm[i] == 0)continue;
        position = ffs(bm[i]) - 1;
        bm[i] &=  ~(1 << position);
        return i*8 + position;
    }
    return -1;
}

// get the entry in directory, return the integer representing it, return -1 if fails
int get_entry_from_root_dir(){
    for(int i=0; i<DIR_TABLE_LEN; i++){
        if (root_dir[i].inode_dir != -1)continue;
        return i;
    }
    return -1;
}

// get the entry in fd table, return the integer representing it, return -1 if fails
int get_entry_from_fd_table(){
    for(int i=0; i<INODE_TABLE_LEN; i++){
        if (fdtb[i].inode_fd != -1)continue;
        return i;
    }
    return -1;
}

// get the entry in inode table, return the integer representing it, return -1 if fails
int get_entry_from_inode_table(){
    for(int i=0; i<INODE_TABLE_LEN; i++){
       if (itb[i].c_link != 0)continue;
        return i;
    }
    return -1;
}

//----------------------------------------------------sfs functions-------------------------------------------------------------

// get the next filename, return 1 if succeeds, otherwise return 0
int sfs_getnextfilename(char* filename){
    pt_dir++;
    if (!(pt_dir >= DIR_TABLE_LEN)) {
        while (root_dir[pt_dir].inode_dir == -1) {
            pt_dir++;
            if (!(pt_dir >= DIR_TABLE_LEN)) continue;
            pt_dir = -1;
            return 0;
        }
        memcpy(filename, root_dir[pt_dir].filename, MAX_FILENAME_LEN);
        return 1;
    } else {
        pt_dir = -1;
        return 0;
    }
}

// get the destinated file size, return -1 if unable to do so
int sfs_getfilesize(const char* path){
    for (int i = 0; i < DIR_TABLE_LEN; i++) {
        if (!(strcmp(root_dir[i].filename, path) == 0 && root_dir[i].inode_dir != -1)) continue;
        return itb[root_dir[i].inode_dir].size_inode;
    };
    return -1;
}


// open file operation: if the file DNE we should create a new file with the filename otherwise we should open it
// return the entry index from the fd table
int sfs_fopen(char* filename){
    if (strlen(filename) > 20)return -1;
    // we need to find an available fd table
    int fdidx = get_entry_from_fd_table();
    // returns -1 if we cannot find one
    if (fdidx != -1) {

        // try to find the file in dir to check if we can open it directly
        int iidx = get_inode_in_dir(filename);
        // we should create one if there is no such file with the filename
        if (iidx != -1) {
            // make sure the file is not already opened
            for (int i = 0; i < INODE_TABLE_LEN; i++) {
                if (fdtb[i].inode_fd == iidx) {
                    return i;
                }
            }
        } else {
            // get entry from inode table to create inode for the new file
            int idx = get_entry_from_inode_table();
            printf("inode_index: %d\n", idx);
            if (idx != -1) {
                iidx = idx;
                itb[iidx].c_link = 1;
                itb[idx].size_inode = 0;

                // put this newly created file in dir
                int didx = get_entry_from_root_dir();
                if (didx != -1) {
                    strncpy(root_dir[didx].filename, filename, MAX_FILENAME_LEN);
                    root_dir[didx].inode_dir = iidx;
                }

                // write to disk
                if (!(write_blocks(INODE_BLOCKS_LEN + 1, DIR_BLOCKS_LEN, &root_dir) <= 0 ||
                      write_blocks(1, INODE_BLOCKS_LEN, &itb) <= 0)) {
                } else {
                    printf("cannot write to disk");
                    return -1;
                }
            } else {
                // return -1 if cannot find a free inode
                printf("no availble inode found");
                return -1;
            }
        }

        fdtb[fdidx].inode_fd = iidx;
        fdtb[fdidx].pt_RW = itb[iidx].size_inode;
        printf("****************file opened with file discriptor index: %d******************\n", fdidx);
        return fdidx;
    } else {
        printf("no available fd entry found\n");
        return -1;
    }
}

// close file operation: close a file by passing its file id, return -1 if not successful, return 0 if is successful
int sfs_fclose(int file_id){
    // check the file id
    if (file_id >= 0 && file_id < INODE_TABLE_LEN) {
        // check repetitive close
        if (!(fdtb[file_id].inode_fd == -1)) {
            fdtb[file_id].pt_RW = -1;
            fdtb[file_id].inode_fd = -1;
            return 0;
        } else {
            printf("this file is already closed\n");
            return -1;
        }
    } else {
        printf("given file id is invalid\n");
        return -1;
    }
}


// read file operation: read certain number of bytes to a buffer giving the file id
// return the reading bytes if successful, otherwise return -1
int sfs_fread(int fild_id, char* buf, int length){
    // check if the file has been opened
    if(fdtb[fild_id].inode_fd == -1){
        printf("this file has not been opened\n");
        return -1;
    }
    int temp_len = length;

    tb_inode *inode = &itb[fdtb[fild_id].inode_fd];
    int pt_rw = fdtb[fild_id].pt_RW;
    if (pt_rw + length > inode->size_inode)temp_len = inode->size_inode;
    int return_read = 0;
    void* bf = (void*) malloc(BLOCK_SIZE);
    //defining the start and end block index
    int start_block = pt_rw / BLOCK_SIZE;
    int end_block = (pt_rw+temp_len) / BLOCK_SIZE;

    for(int i=start_block; i<=end_block; i++){
        int start_block_offset = 0;
        int end_block_offset = BLOCK_SIZE;

        if (i == start_block)start_block_offset = pt_rw % BLOCK_SIZE;
        if (i == end_block)end_block_offset = (pt_rw + temp_len) % BLOCK_SIZE;
        int offset_length = end_block_offset - start_block_offset;

        // use direct pointer if within the UNIX limit 12, otherwise use indirect pointer
        if(i < 12){
            read_blocks(inode->pts_dir[i], 1, bf);
            memcpy(buf+return_read, bf+start_block_offset, offset_length);
        }else{
            // init indirect pointer
            int pts_indir[INDIR_PTR_SIZE];
            read_blocks(inode->pt_indir, 1, bf);
            memcpy(pts_indir, bf, BLOCK_SIZE);

            int indir_block = i - 12;
            read_blocks(pts_indir[indir_block], 1, bf);
            memcpy(buf+return_read, bf+start_block_offset, offset_length);
        }
        return_read += offset_length;
    }
    entry_fd* fdEntry = &fdtb[fild_id];
    fdEntry->pt_RW += temp_len;
    free(bf);
    printf("====================Read finished=====================\n");
    return return_read;

}

// write file operation: read certain number of bytes to a buffer giving the file id
// return the writing bytes if successful, otherwise return -1
int sfs_fwrite(int file_id, const char* buf, int length){

    int pt_rw = fdtb[file_id].pt_RW;

    void* bf = (void*) malloc(BLOCK_SIZE);
    int pts_indir[INDIR_PTR_SIZE];
    int start_block = pt_rw / BLOCK_SIZE;
    int end_block = (pt_rw+length) / BLOCK_SIZE;
    int return_write = 0;
    bool bool_pts_indir = false;

    // check if the file has been opened
    if(fdtb[file_id].inode_fd == -1){
        printf("this file has not been opened\n");
        return -1;
    }
    tb_inode *inode = &itb[fdtb[file_id].inode_fd];
    // check if the max file size is exceeded, in that can we cannot continue writing
    if (length+pt_rw > MAX_FILE_SIZE){
        printf("writing process has been stopped\n");
        length = MAX_FILE_SIZE - pt_rw;
    }


    for(int i=start_block; i<=end_block; i++){
        int start_block_offset = 0;
        int end_block_offset = BLOCK_SIZE;

        if (i == start_block)start_block_offset = pt_rw % BLOCK_SIZE;
        if (i == end_block)end_block_offset = (pt_rw + length) % BLOCK_SIZE;
        int offset_length = end_block_offset - start_block_offset;
        // use direct pointer if within the UNIX limit 12, otherwise use indirect pointer
        if(i < 12){
            if (inode->pts_dir[i] == -1)inode->pts_dir[i] = get_free_data_block();
            //read data with the buffer
            read_blocks(inode->pts_dir[i], 1, bf);
            memcpy(bf+start_block_offset, buf+return_write, offset_length);
            write_blocks(inode->pts_dir[i], 1, bf);
        }else{
            bool_pts_indir = true;
            if(inode->pt_indir == -1){
                //get new data block for new indirect pointer
                inode->pt_indir = get_free_data_block();
                int indir_idx;
                for (indir_idx = 0; indir_idx < INDIR_PTR_SIZE; indir_idx++)pts_indir[indir_idx] = -1;
                memcpy(bf, pts_indir, BLOCK_SIZE);
                write_blocks(inode->pt_indir, 1, bf);
            }
            read_blocks(inode->pt_indir, 1, bf);
            memcpy(pts_indir, bf, BLOCK_SIZE);
            int block_indir = i - 12;
            if (pts_indir[block_indir] == -1)pts_indir[block_indir] = get_free_data_block();
            read_blocks(pts_indir[block_indir], 1, bf);
            memcpy(bf+start_block_offset, buf+return_write, offset_length);
            write_blocks(pts_indir[block_indir], 1, bf);
        }
        return_write += offset_length;
    }
    entry_fd* entry_fd = &fdtb[file_id];
    entry_fd->pt_RW += length;
    if(bool_pts_indir){
        memcpy(bf, pts_indir, BLOCK_SIZE);
        write_blocks(inode->pt_indir, 1, bf);
    }
    if (entry_fd->pt_RW > inode->size_inode)inode->size_inode = entry_fd->pt_RW;
    // check is we can write to blcok
    if (!(write_blocks(NUM_BLOCKS - 1, 1, &bm) > 0 && write_blocks(1, INODE_BLOCKS_LEN, &itb) > 0))
        printf("Error: cannot write block\n");
    free(bf);
    printf("++++++++++++++++++++++++++Writing finished++++++++++++++++++++++++++\n");
    return return_write;
}


// remove file operation: remove a file with the given filename, return 1 if the removal is successful
int sfs_remove(char* filename){
    int inumber = get_inode_in_dir(filename);
    // check if the file exist
    if (inumber != -1) {

        // erase the information of the file in the directory inode table
        for (int i = 0; i < INODE_TABLE_LEN; i++) {
            if (!!strcmp(root_dir[i].filename, filename)) continue;
            root_dir[i].inode_dir = -1;
            strcpy(root_dir[i].filename, "");
            break;
        }

        // erase the information of the file in the fd table
        for (int i = 0; i < INODE_TABLE_LEN; i++) {
            if (fdtb[i].inode_fd == inumber) fdtb[i] = (entry_fd) {-1, -1};
        }

        void *bf = (void *) malloc(BLOCK_SIZE);
        tb_inode *inode = &itb[inumber];
        int isize = inode->size_inode;
        int end_block = isize / BLOCK_SIZE;
        for (int i = 0; i <= end_block; i++) {
            memset(bf, 0, BLOCK_SIZE);
            // use direct pointer if within the UNIX limit 12, otherwise use indirect pointer
            if (i < 12) {
                write_blocks(inode->pts_dir[i], 1, bf);
                free_bit(inode->pts_dir[i]);
                inode->pts_dir[i] = -1;
            } else {
                int pts_indir[INDIR_PTR_SIZE];
                read_blocks(inode->pt_indir, 1, bf);
                memcpy(pts_indir, bf, BLOCK_SIZE);
                int block_indir = i - 12;
                memset(bf, 0, BLOCK_SIZE);
                write_blocks(pts_indir[block_indir], 1, bf);
                free_bit(pts_indir[block_indir]);
                pts_indir[block_indir] = -1;
            }
        }
        free_bit(inode->pt_indir);
        inode->pt_indir = -1;

        if (!!(write_blocks(INODE_BLOCKS_LEN + 1, DIR_BLOCKS_LEN, &root_dir) > 0 &&
               write_blocks(NUM_BLOCKS - 1, 1, &bm) > 0 &&  
               write_blocks(1, INODE_BLOCKS_LEN, &itb) > 0)) {
            free(bf);
            printf("--------Remove finished--------\n");
            return 1;
        } else {
            printf("Error: cannot write block\n");
            return 0;
        }
    } else {
        printf("cannot find such a file\n");
        return -1;
    }
    
}

// seek file operation: find a file giving the file id and location
// seek to the location from the beginning
int sfs_fseek(int fild_id, int location){
    // check that this file is not currently opened
    if (fdtb[fild_id].inode_fd == -1) {
        printf("file is not open.");
        return -1;
    } else {
        if (!(location < 0)) {
            if (location > itb[fild_id].size_inode) {
                fdtb[fild_id].pt_RW = itb[fild_id].size_inode;
            } else {
                fdtb[fild_id].pt_RW = location;
            }
        } else {
            fdtb[fild_id].pt_RW = 0;
        }
        return 0;
    }
}


// create file system: use fresh to indicate whether to open from disk or build a new file system
void mksfs(int fresh){
    if(fresh){
        // create new file system
        init_fresh_disk(DISK_FILENAME, BLOCK_SIZE, NUM_BLOCKS);
        // create the superblock
        superblock.block_size = BLOCK_SIZE;
        superblock.inode_table_len = INODE_TABLE_LEN;
        superblock.inode_root_dir = 0;
        superblock.magic = MAGIC;
        superblock.file_system_size = NUM_BLOCKS;
        bm[0/8] &= ~(1 << (0%8));
        // create file descriptor table
        for (int i = 0; i < INODE_TABLE_LEN; i++){
            fdtb[i].pt_RW = -1;
            fdtb[i].inode_fd = -1;
        }
        // create root directory
        for (int i = 0; i < INODE_TABLE_LEN; i++){
            root_dir[i].inode_dir = -1;
            strcpy(root_dir[i].filename, "");
        }	
        // create the inode table
        for(int i=0; i<INODE_TABLE_LEN; i++){
            itb[i].is_empty = 1;
            itb[i].size_inode = -1;
            itb[i].pt_indir = -1;
            for(int j=0; j<DIR_PTR_SIZE; j++)
                itb[i].pts_dir[j] = -1;
        }
        // save these newly created structure and write to disk
        if (write_blocks(NUM_BLOCKS - 1, 1, &bm) > 0 && 
            write_blocks(0, 1, &superblock) > 0 && 
            write_blocks(INODE_BLOCKS_LEN + 1, DIR_BLOCKS_LEN, &root_dir) > 0 &&
            write_blocks(1, INODE_BLOCKS_LEN, &itb) > 0)
            printf("write block finished\n");
    }else{
        // load from disk
        init_disk(DISK_FILENAME, BLOCK_SIZE, NUM_BLOCKS);
        read_blocks(0, 1, &superblock);
        read_blocks(NUM_BLOCKS - 1, 1, &bm);
        read_blocks(INODE_BLOCKS_LEN+1, DIR_BLOCKS_LEN, &root_dir);
        read_blocks(1, INODE_BLOCKS_LEN, &itb);
    }
}
