/*************************************************
 *
 *	filename: directory.c
 *
 *	CSC 552 LFS Project Phase 1
 *
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 * 
 *	Hoang Van    vnhh@email.arizona.edu
 *
 *  A directory implementation
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
#include "directory.h"
#include "file.h"
#include "fsmeta.h"
#include <unistd.h>

/************************************************************************
 * Helpers
 * function name: substring 
 * what it does? get the substring from the char start at i and stop before
 * end
 * parameters(4): s[], substr[], start, end
 * return nothing - HV
 ***********************************************************************/
void substring(char s[], char substr[], int start, int end) {
   int len_of_substring = end - start; // starting at the start
   for (int i = 0; i < len_of_substring; i++) {
      substr[i] = s[i + start];
   }
}

/*********************************************************************
 * Helpers
 * function name: get_last_dir
 * What it does? return inode_num of the last directory given a path.
 * Parameters(2): *path: a given path, and new_path: new directory
 * Return: last dir in the path as inode_num (uint16_t) - HV
 ********************************************************************/
uint16_t get_last_dir(const char* old_path, char *new_path) {
	int old_path_len = strlen(old_path); 
	int i = old_path_len - 1; 
	while (i >= 0) {
		if(old_path[i] == '/') {
			break; // keep finding the last dir until we find it then break out
		}
		i = i - 1;
	}
	if(i == 0){
		strcpy(new_path, old_path+1); // create new path
		return 1; 
	}
	char* upper = malloc(i); //allocate space for the new path
	substring((char*)old_path, upper, 0, i);
	substring((char*)old_path, new_path, i+1, strlen(old_path));
	return  get_inode_num_lfs(upper);
}

/*******************************************************************************
 * function name: set_inode_num
 * What it does? set a inode_num to a file and then update its parent dir
 * Parameters(2): const char *file (so we won't change the file name),
 * and dir: a directory inum 
 * Return: new inode_num
 *******************************************************************************/
uint16_t set_inode_num(const char *file, uint16_t dir) {
	struct inode *dir_inode = malloc(size_of_inode); // allocate space for the dir
	read_ifile(dir, dir_inode); // read file to directory inode
	void *dir_buffer = malloc(dir_inode->size + size_of_dir_entry);
	read_file(dir, 0, dir_inode->size, dir_buffer);
	// get the parent directory header
	struct dir *parent_dir = dir_buffer; //access parent directory
//	printf("set_inode_num parent_dir->size = %d and dir_inode->size = %d\n",parent_dir->size,dir_inode->size);
	parent_dir->size += 1;
	struct dir_entry *new_dir = dir_buffer + dir_inode->size; // create new directory
	strcpy(new_dir->name, file);
	new_dir->inum = chk_ptr->next_inum++; // set inode_num of new directory to next inode id
	write_to_file(dir, 0, dir_inode->size + size_of_dir_entry, dir_buffer);
	printf("Successfully Update directory\n");
	return new_dir->inum;
}

/*******************************************************************************
 * HELPERS
 * function name: get_inode_num_from_parent
 * What it does? get the inode num of the file from its parent inode
 * Parameters(2): const char *file: the given filename, and parent: the inode
 * of a parent.
 * Return: uint16_t, the inode_inum of the file
 *******************************************************************************/
uint16_t get_inode_num_from_parent(char *file, uint16_t parent){
	struct inode *parent_inode = malloc(size_of_inode);
	read_ifile(parent, parent_inode); // read parent to the parent inode
//	printf("    get_inode_num_from_parent @%s, inum=%d, ifile's seg=%d, ifile's block=%d\n",file,parent,parent_inode->ptrs[0].seg_num,parent_inode->ptrs[0].block_num);
	void *pa_buff = malloc(parent_inode->size); // create buffer for reading parent' data
	read_file(parent, 0, parent_inode->size, pa_buff);
	struct dir *pa_dir = pa_buff;
	struct dir_entry *pa_entry;
	for(int i = 0; i < pa_dir->size; i++){
		pa_entry = pa_buff + size_of_dir + (size_of_dir_entry*i);
		if(strcmp(file, pa_entry->name) == 0){ // check filename with the one we retrieved
			return pa_entry->inum; // correct return it
		}
	}

	return AVAIL_ADDR;
}

/*******************************************************************************
 * HELPERS
 * function name: get_inode_num_lfs
 * What it does? get the inode num.
 * Parameters(1): const char *file: the given filename.
 * Return: uint16_t, the inode_inum of the file
 *******************************************************************************/
uint16_t get_inode_num_lfs(const char *path) {
	if(strcmp(path, "/") == 0){
		return 1;
	}
	uint16_t parent = 1;
    int j = 1, i = 1;
    while(i <= strlen(path)) {
        if(path[i] == '/' || i ==  strlen(path)) {
            char *file = calloc(i - j, char_size );	// shkim. malloc -> calloc. 10.27.2020
            substring((char*)path, file, j, i);
            j = i + 1; // update j to next one
            parent = get_inode_num_from_parent(file, parent); // update the curr parent
        }
        i += 1;
    }
    return parent;

}
