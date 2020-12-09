/*************************************************
 *
 *	filename: directory.h
 *
 *	CSC 552 LFS Project Phase 1
 *
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 * 
 *	Hoang Van    vnhh@email.arizona.edu
 *
 *  A directory layer header definition
 *************************************************/

#include <stdio.h>
#include <stdint.h>
#include "fsmeta.h"
#include <stdlib.h>
#include <string.h>
#include "file.h"
#include <fuse.h>
#include <string.h>
#include <fcntl.h>

#define FUSE_USE_VERSION 26
#ifndef DIRECTORY_H
#define DIRECTORY_H
#define char_size sizeof(char)
#define LFS_FILE_NAME_MAX_LEN 48

struct dir {
	char name[LFS_FILE_NAME_MAX_LEN];
	int size;								
};

struct dir_entry{
	char name[LFS_FILE_NAME_MAX_LEN];
	uint16_t inum;
};

uint16_t get_inode_num_from_parent(char *filename, uint16_t parent_inum);
uint16_t get_inode_num_lfs(const char *path);
uint16_t get_last_dir(const char* path, char *new_dir);
void substring(char s[], char sub[], int p, int l);

#endif
