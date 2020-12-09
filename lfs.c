/*************************************************
 *
 *	filename: lfs.c
 *
 *	CSC 552 LFS Project Phase 1
 *
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 * 
 *	Hoang Van    vnhh@email.arizona.edu
 *
 *  A LFS layer header definition
 *************************************************/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include "lfs.h"
#include "directory.h"
#include "fsmeta.h"
#include <unistd.h>

int s_blk_size = 0; // set to 0 - HV

char *flash_file = NULL;

int cp_range;

int size_seg; // number of blocks in that segment.
int SUPERBLOCK_SEG_SIZE;

int start;
int stop;

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: opendir_op(
 ***************************************************************************/
static int opendir_op(const char* path, struct fuse_file_info* fi){
	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: symlk_op
 ***************************************************************************/
static int symlk_op(const char* to, const char* from) { 
	printf("===== lfs_symlink(%s, %s)\n", to, from);
	char link_filename[LFS_FILE_NAME_MAX_LEN];
	memset(link_filename, 0, LFS_FILE_NAME_MAX_LEN);
	// "to" is the linked file, when evaluate "from", it lead to "to"
	uint16_t link_parent_inum = get_last_dir(from, link_filename);
	// this will create a directory entry in the function, and a new inum.
	uint16_t link_inum = set_inode_num(link_filename, link_parent_inum);
	// this will create the inode itself with a file type to be symlink
	make_file(link_inum, 3);

	// contents of symlink = the path to actual file.
	write_to_file(link_inum, 0, strlen(to), (void *)to);

	return 0; 
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: access_op
 ***************************************************************************/
static int access_op(const char* path, int mask){	return 0; }

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: getattr_op
 ***************************************************************************/
static int getattr_op(const char* path, struct stat* stbuf){
	int res = 0;
	printf("===getattr_op %s\n",path);
	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0777;
        stbuf->st_nlink = 2;
        stbuf->st_ino = 3;
    } else {
    	uint16_t inum = get_inode_num_lfs(path);
    	printf("GETTING inum in getattr %s %u\n", path, inum);
    	if(inum == AVAIL_ADDR){
    		res = -1*ENOENT;
    		return res;
    	}
    	struct inode *inode = malloc(sizeof(struct inode));
    	read_ifile(inum, inode);
    	// we here check to see if LFS is working with normal file, a directory, or a symlink - HV
    	if(inode->type == 1){
    		printf("****************Working with normal file*********************\n");
    		stbuf->st_size = inode->size;
    	}
    	else if(inode->type == 2){
    		printf("****************Working with directory***********************\n");
    	}
    	else if(inode->type == 3){
    		printf("****************Working with a symbolic link******************\n");
    		stbuf->st_size = inode->size;
    	}
    	else{
    		printf("Type is not valid %u\n", inode->type);
    	}
    	// End checking here - HV
    	stbuf->st_mode = inode->mode;
    	stbuf->st_atime = inode->lst_acs;	// shkim
    	stbuf->st_ctime = inode->lst_cre;	// shkim
    	stbuf->st_mtime = inode->lst_mdf;
    	stbuf->st_nlink = inode->n_links; 
    	stbuf->st_ino = inode->inum;
    	stbuf->st_uid = inode->uid;
    	stbuf->st_gid = inode->gid;
    }
	return res; 
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: open_op
 ***************************************************************************/
static int open_op(const char* path, struct fuse_file_info* fuse_layer) {
	printf("ifile_inum : %d\n\n",get_inode_num_lfs("/.ifile"));
	if(get_inode_num_lfs(path) == AVAIL_ADDR){
		return -1*ENOENT;
	}
	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: read_op
 ***************************************************************************/
static int read_op(const char* path, char *buffer, size_t data_size, off_t offset, struct fuse_file_info* fuse_layer){
	memset(buffer, 0, data_size); //set space in mem for this buffer given the size
    (void) fuse_layer; //change type of fuse_layer
    uint16_t inode_num = get_inode_num_lfs(path);
    struct inode *inode = malloc(size_of_inode);
    read_ifile(inode_num, inode);
    if(inode_num == AVAIL_ADDR){
		return -1*ENOENT;
	}
	size_t len = inode->size;
	if (offset < len) {
        if (offset + data_size > len) {
            data_size = len - offset;
        }
    } else{
        data_size = 0; //set new data_size
    }
	read_file(inode_num, offset, data_size, buffer); // read inode data to buffer
	return data_size;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: write_op
 ***************************************************************************/
static int write_op(const char* path, const char *buffer, size_t data_size, off_t offset, struct fuse_file_info* fuse_layer){
    (void) fuse_layer;
    uint16_t inode_num = get_inode_num_lfs(path);
    if(inode_num == AVAIL_ADDR){
		return -1*ENOENT;
	}
	int error_code = write_to_file (inode_num, offset, data_size, (void *) buffer);
	if(error_code < 0)	{
		return error_code;
	}
	return data_size;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: init_op
 ***************************************************************************/
static void* init_op(struct fuse_conn_info *conn){
	fsmeta_init();	// call fsmeta
	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: mkdir_op
 ***************************************************************************/
static int mkdir_op(const char* path, mode_t mode) {
	printf("===== mkdir_op(%s)\n", path);

	// the given directory name is created under
	char new_dir[LFS_FILE_NAME_MAX_LEN];
	memset(new_dir, 0, LFS_FILE_NAME_MAX_LEN);
	uint16_t parent_inum = get_last_dir(path, new_dir);

	printf("PARENT: %u - NEW DIR: %s\n", parent_inum, new_dir);
	// 1) make a new inum for the new directory.
	uint16_t new_inum = set_inode_num(new_dir, parent_inum);
	// 2) create the new directory file through File layer.
	make_file(new_inum, 2);
	// 3) prepare the initial value of a directory, . and ..
	void *dir_buf = malloc(sizeof(struct dir) + 2 * sizeof(struct dir_entry));
	struct dir *dir_hdr = dir_buf;
	strcpy(dir_hdr->name, new_dir);
	dir_hdr->size = 2;
	struct dir_entry *temp = dir_buf + sizeof(struct dir);
	strcpy(temp->name, ".");
	temp->inum = new_inum;
	temp = dir_buf + sizeof(struct dir) + sizeof(struct dir_entry);
	strcpy(temp->name, "..");
	temp->inum = parent_inum;
	write_to_file(new_inum, 0, sizeof(struct dir) + 2 * sizeof(struct dir_entry), dir_buf);

	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: destroy_op
 ***************************************************************************/
static void destroy_op(void* private_data){
	update_chk_ptr(); // update current checkpoint
	write_to_flash(); // write to segment
}


/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: create_op
 ***************************************************************************/
static int create_op(const char* path, mode_t mode, struct fuse_file_info *fuse_layer){
	char *file = malloc(48);
	uint16_t parent_inode_num = get_last_dir(path, file);
	uint16_t inode_num = set_inode_num(file, parent_inode_num);
	// we create a normal file here - HV, we pass it the option 1 of normal file
	make_file(inode_num, 1);
	struct inode *target_inode = malloc(size_of_inode);
	read_ifile(inode_num, target_inode);
	printf("create_op make_file->read_ifile then %d, %d, %d\n\n",inode_num,target_inode->inum,target_inode->type);

	struct fuse_context *f_context = fuse_get_context();
	target_inode->uid = f_context->uid;
	target_inode->gid = f_context->gid;
	// now write a clear inode to the memory
	write_ifile(inode_num, target_inode);	// shkim 1114 (refactoring ifile)
	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: chmod_op
 ***************************************************************************/
static int chmod_op(const char* path, mode_t mode){	
	printf("===== chmod_op(%s)\n", path);
	uint16_t inum = get_inode_num_lfs(path);
	struct inode *inode = malloc(sizeof(struct inode));
	read_ifile(inum, inode);
	inode->mode = mode;
	write_ifile(inum, inode);
	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: unlk_op
 ***************************************************************************/
static int unlk_op(const char* path) {
	printf("===== unlk_op(%s) \n", path);
	uint16_t inum = get_inode_num_lfs(path);
	struct inode *inode = malloc(sizeof(struct inode));
	read_ifile(inum, inode);

	if(inode->type == 1 && inode->n_links <= 1){
		// need to actually free the file
		free_file(inum);
	}

	// then delete the parent directory entry here.
	char filename[LFS_FILE_NAME_MAX_LEN];
	uint16_t parent_inum = get_last_dir(path, filename);
	struct inode *parent_inode = malloc(sizeof(struct inode));
	read_ifile(parent_inum, parent_inode);
	void *old_parent_dir_buf = malloc(parent_inode->size);
	read_file(parent_inum, 0, parent_inode->size, old_parent_dir_buf);

	void *new_parent_dir_buf = malloc(parent_inode->size - sizeof(struct dir_entry));
	// copy the header
	memcpy(new_parent_dir_buf,old_parent_dir_buf, sizeof(struct dir));
	struct dir *new_ptr = new_parent_dir_buf;
	new_ptr->size --;

	struct dir *old_hdr = old_parent_dir_buf;
	// now iterate through the old parent directory, then copy if needed
	struct dir_entry *old_ptr;
	int j = 0; // counter of new directory file
	for(int i = 0; i < old_hdr->size; i++){
		old_ptr = old_parent_dir_buf + sizeof(struct dir) + (sizeof(struct dir_entry) * i);
		// if it is not the deleted file, then copy into the new directory file
		if(strcmp(filename, old_ptr->name) != 0){
			// find the new destination.
			new_ptr = new_parent_dir_buf + sizeof(struct dir) + (sizeof(struct dir_entry) * j);
			memcpy(new_ptr, old_ptr, sizeof(struct dir_entry));
			j++;
		}
	}

	// after the loop, new_parent_dir_buf has the correct dir file content
	make_file(parent_inum, 2);
	write_to_file(parent_inum, 0, parent_inode->size - sizeof(struct dir_entry), new_parent_dir_buf);

	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: truncate_op
 ***************************************************************************/
static int truncate_op(const char* path, off_t data_size){
	uint16_t inode_num = get_inode_num_lfs(path);
	struct inode *inode = malloc(size_of_inode);
	read_ifile(inode_num, inode);
	void *buffer = malloc(inode->size); //setup the buffer to store data from inode
	read_file(inode_num, 0, inode->size, buffer); // read data to buffer
	for(int i = 3; i >= 0; i--) { // clear 3 blocks from this buffer
		if(inode->ptrs[i].seg_num != AVAIL_ADDR) {
			struct addr *blk_addr = malloc(sizeof(struct addr));
			blk_addr->seg_num = AVAIL_ADDR; //setup seg_num for this block
			blk_addr->block_num = AVAIL_ADDR; //setup block_num for this block
			update_inode(inode_num, i, blk_addr, - inode->size % s_blk_size);
		}
	}
	void *new_buffer = malloc(data_size); //now we create new buffer for this data_size
	memset(new_buffer, 0, data_size); // clean the memory. shkim 10/23/2020
	if (inode->size >= data_size) {
		inode->size = data_size;
	}
	memcpy(new_buffer, buffer, inode->size); //update mem
	write_to_file (inode_num, 0, data_size, new_buffer);
	return 0; // upon success.
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: link_op
 ***************************************************************************/
static int link_op(const char* from, const char* to) { 
	printf("===== link_op(%s, %s)\n", from, to);
	char new_file_name[LFS_FILE_NAME_MAX_LEN];
	memset(new_file_name, 0, LFS_FILE_NAME_MAX_LEN);
	uint16_t file_inum = get_inode_num_lfs(from);
	uint16_t new_parent_inum = get_last_dir(to, new_file_name);
	printf("===== %s %u %u\n", new_file_name, file_inum, new_parent_inum);

	// 1. Add an entry in the desination directory
	struct inode *new_parent_inode = malloc(sizeof(struct inode));
	read_ifile(new_parent_inum, new_parent_inode);
	// buffer to hold dir file
	void *new_parent_dir_buffer = malloc(new_parent_inode->size + sizeof(struct dir_entry));
	read_file(new_parent_inum, 0, new_parent_inode->size, new_parent_dir_buffer);
	// get the parent directory header
	struct dir *dir_hdr = new_parent_dir_buffer;
	dir_hdr->size ++;
	// the new entry
	struct dir_entry *new_entry = new_parent_dir_buffer + new_parent_inode->size;
	strcpy(new_entry->name, new_file_name);
	new_entry->inum = file_inum;
	write_to_file(new_parent_inum, 0, new_parent_inode->size + sizeof(struct dir_entry), new_parent_dir_buffer);

	// 2. update the inode of origin file, increment the number of hard links.
	struct inode *file_inode = malloc(sizeof(struct inode));
	read_ifile(file_inum, file_inode);
	file_inode->n_links ++;
	write_ifile(file_inum, file_inode);
	printf("\t===== link_op() returns\n");

	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: statfs_op
 ***************************************************************************/
static int statfs_op(const char* path, struct statvfs* stbuf){
	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: chown_op
 ***************************************************************************/
static int chown_op(const char* path, uid_t uid, gid_t gid){	
	printf("===== chown_op(%s) \n", path);
	uint16_t inum = get_inode_num_lfs(path);
	struct inode *inode = malloc(sizeof(struct inode));
	read_ifile(inum, inode);
	inode->uid = uid;
	inode->gid = gid;
	write_ifile(inum, inode);
	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: releasedir_op(
 ***************************************************************************/
static int releasedir_op(const char* path, struct fuse_file_info *fi){
	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: rmdir_op
 ***************************************************************************/
static int rmdir_op(const char* path) {
	// ENOTEMPTY
	printf("===== rmdir_op(%s)\n", path);

	// 1) check whether the given directory is empty or not.
	uint16_t inum = get_inode_num_lfs(path);
	struct inode *inode = malloc(sizeof(struct inode));
	read_ifile(inum, inode);
	void *dir_buf = malloc(inode->size);
	read_file(inum, 0, inode->size, dir_buf);
	struct dir *hdr = dir_buf;
	if(hdr->size > 2)
		return -ENOTEMPTY;

	// 2) free the directory file
	free_file(inum);

	// 3) remove the entry from its parent directory
	// 3-1) get the parent directory file
	char dirname[LFS_FILE_NAME_MAX_LEN];
	memset(dirname, 0, LFS_FILE_NAME_MAX_LEN);
	uint16_t parent_inum = get_last_dir(path, dirname);
	struct inode *parent_inode = malloc(sizeof(struct inode));
	read_ifile(parent_inum, parent_inode);
	void *old_parent_dir_buf = malloc(parent_inode->size);
	read_file(parent_inum, 0, parent_inode->size, old_parent_dir_buf);

	// 3-2) update the parent directory file
	void *new_parent_dir_buf = malloc(parent_inode->size - sizeof(struct dir_entry));
	// 3-2-1) copy the header, decrement the size by 1
	memcpy(new_parent_dir_buf, old_parent_dir_buf, sizeof(struct dir));
	struct dir *new_ptr = new_parent_dir_buf;
	new_ptr->size --;
	struct dir *old_hdr = old_parent_dir_buf;
	// 3-2-2) now iterate through the old parent directory, copy the entry if it's not the removed one
	struct dir_entry *old_ptr;
	int j = 0; 	// counter of new directory file
	for(int i = 0; i < old_hdr->size; i++){
		old_ptr = old_parent_dir_buf + sizeof(struct dir) + (sizeof(struct dir_entry) * i);
		// if it's not the deleted file,
		if(strcmp(dirname, old_ptr->name) != 0){
			// find the new destination
			new_ptr = new_parent_dir_buf + sizeof(struct dir) + (sizeof(struct dir_entry) * j);
			memcpy(new_ptr, old_ptr, sizeof(struct dir_entry));
			j++;
		}
	}
	// 3-3) after the loop, need to create and write new directory file with the same inum
	make_file(parent_inum, 2);
	write_to_file(parent_inum, 0, parent_inode->size - sizeof(struct dir_entry), new_parent_dir_buf);

	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: rename_op
 ***************************************************************************/
static int rename_op(const char* from, const char* to){ 
	printf("===== rename_op(%s, %s)\n", from, to);
	char from_filename[LFS_FILE_NAME_MAX_LEN];
	uint16_t parent_inum = get_last_dir(from, from_filename);
	printf("\tget_last_dir(%s) returned %u\n", from, parent_inum);
	char to_filename[LFS_FILE_NAME_MAX_LEN];
	parent_inum = get_last_dir(to, to_filename);
	printf("\tget_last_dir(%s) returned %u\n", to, parent_inum);

	struct inode *parent_inode = malloc(sizeof(struct inode));
	read_ifile(parent_inum, parent_inode);
	void *parent_buffer = malloc(parent_inode->size);
	read_file(parent_inum, 0, parent_inode->size, parent_buffer);
	struct dir *parent_dir = parent_buffer;

	struct dir_entry *entry_ptr;
	for(int i = 0; i < parent_dir->size; i++){
		entry_ptr = parent_buffer + sizeof(struct dir) + (sizeof(struct dir_entry) * i);
		if(strcmp(from_filename, entry_ptr->name) == 0){
			strcpy(entry_ptr->name, to_filename);
			break;
		}
	}

	write_to_file(parent_inum, 0, parent_inode->size, parent_buffer);
	return 0; 
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: release_op
 ***************************************************************************/
static int release_op(const char* path, struct fuse_file_info *fi){
	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: readlk_op
 ***************************************************************************/
static int readlk_op(const char* path, char* buf, size_t size) { 
	printf("===== readlk_op(%s, %lu)\n", path, size);
	uint16_t link_inum = get_inode_num_lfs(path);
	struct inode *link_inode = malloc(sizeof(struct inode));
	read_ifile(link_inum, link_inode);
    if(link_inum == AVAIL_ADDR){
		return -1*ENOENT;
	}

    void *link_buffer = malloc(link_inode->size);
	read_file(link_inum, 0, link_inode->size, link_buffer);
	strcpy(buf, link_buffer);

	return 0;
}

/****************************************************************************
 * Operation function declared to be used with FUSE
 * function name: readdir_op
 ***************************************************************************/
static int readdir_op(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi){
	//
	(void) offset;
	(void) fi;

	printf("===== readdir_op(%s)\n", path);

	uint16_t inum = get_inode_num_lfs(path);

	struct inode *root_inode = malloc(sizeof(struct inode));
	read_ifile(inum, root_inode);
	printf("^^^^^^^^^^^^^^^^^^^^^^^SIZE: %d\n", root_inode->size);
	void *root_buffer = malloc(root_inode->size);
	read_file(inum, 0, root_inode->size, root_buffer);
	struct dir *root = root_buffer;
	printf("DIR+++ %s : %u\n", root->name, root->size);
	int num_entries = root->size;
	struct dir_entry *ptr = root_buffer;
	for (int i = 0; i < num_entries; ++i)
	{
		ptr = root_buffer + sizeof(struct dir) + sizeof(struct dir_entry) * (i);
		printf("FILE %u: %s\n", ptr->inum, ptr->name);
		// each time we read a directory entry from the root buffer.
		filler(buf, ptr->name, NULL, 0);

	}

	return 0;
}

/*************************************************************************
 * This is a struct make it constanst so that fuse can know what operation 
 * to call through the LFS. We will assign each lfs operation to that know 
 * by FUSE and hence Fuse can use it for its operation at FUSE layer
 ************************************************************************/
static struct fuse_operations lfs_ops = {
	.getattr = getattr_op,
	.statfs = statfs_op,
	.access = access_op,
	.chmod = chmod_op,
	.readlink = readlk_op,
	.releasedir = releasedir_op,
	.mkdir = mkdir_op,
	.unlink = unlk_op,
	.rmdir = rmdir_op,
	.open = open_op,
	.write = write_op,
	.truncate = truncate_op,
	.init = init_op,
	.read = read_op,
	.release = release_op,
	.opendir = opendir_op,
	.readdir = readdir_op,
	.destroy = destroy_op,
	.link = link_op,
	.symlink = symlk_op,
	.rename = rename_op,
	.create = create_op,
	.chown = chown_op,
};


// Start the main function here so that we can call lfs within compilation.
int main(int argc, char *argv[]) {
	char **nargv = NULL;
    int updated_argc = argc - 2, nargc;
    cp_range = 4;
    start = 4;
    stop = 8;
	struct option option_array[] = {
		{"start", 	required_argument, 0, 'c'},
		{"interval", 	required_argument, 0, 'i'},
		{"stop", 	required_argument, 0, 'C'}
	};
	int index = 0;
	int opt = getopt_long(argc, argv, "s:i:c:C:", option_array, &index);
	while ((opt) != -1){
		if (opt == 'i') {
			cp_range = (int) strtol(optarg, (char **)NULL, 10);
		} else if (opt == 'c') {
			start = (int) strtol(optarg, (char **)NULL, 10);
		} else if (opt == "C") {
			stop = (int) strtol(optarg, (char **)NULL, 10);
		}
		opt = getopt_long(argc, argv, "s:i:c:C:", option_array, &index); //update option
	}
	flash_file = argv[argc-2];
    nargc = argc + num_args - updated_argc;

    nargv = (char **) malloc(nargc * sizeof(char*));
    nargv[0] = argv[0];
    nargv[1] = "-f";
    nargv[2] = "-s";
    nargv[3] = "-d";
    for (int j = 1; j < argc - updated_argc; j++) {
		nargv[j+num_args] = argv[j+updated_argc];
    }
    return fuse_main(nargc, nargv, &lfs_ops, NULL);
}
