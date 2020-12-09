/*************************************************
 *	filename: file.c
 *	CSC 552 LFS Project Phase 1
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 *	Hoang Van    vnhh@email.arizona.edu
 *  A file layer .C of the LFS
 *************************************************/

#include "file.h"
#include "fsmeta.h"
#include "log.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdbool.h>

//Define the global variable here 
struct superblock *super_blk_lfs = NULL;
struct checkpoint_region *chk_ptr = NULL;
struct segment *tail_seg = NULL;
int s_blk_size;

/***********************************************************************
 * Function name: make_file
 * What it does? make a new file depends on assigned type and inode number.
 * As mentioned in the spec, a file is an inode with metadata with file type
 * Also, information about the size and address at flash will be stored
 * Parameters(2): inode_num: the number of the inode of the file, ftype.
 * Return: 0 if successful and -1 if error occurs
 ***********************************************************************/
int make_file(int inode_num, int ftype) {
	printf("making a new file, inode: %d\n", inode_num); /* debugging tool */
	// According to the specs, the directory will be an array of (directory, inode_num)
	// Since there is no specifics about the format, we picked this kind of format
	// and we  append the ifile
	int ifile_blk = inode_num *  size_of_inode  / s_blk_size;
	// Below is to locate ifile and get the file inode of inode_num
	// extend ifile if inum is in a unassigned block of ifile
	if(chk_ptr->ifile_inode.ptrs[ifile_blk].seg_num == AVAIL_ADDR) {
		if(create_ifile_block(0, ifile_blk) == -1){
			printf("Error: Cannot make file, EXCEEDED Files Limitation!!!\n");
			return -1;
		}
	}
	// Locade the file inode
	struct inode *file_inode = malloc( size_of_inode );
	read_ifile(inode_num, file_inode);
	// Assign inode_num and ftype to inode instance, we will call set_file_inode to set
	// all attributes from file_inode
	set_file_inode(file_inode, inode_num, ftype);

	write_ifile(inode_num, file_inode);	// shkim 1114 (refactoring ifile)
	// successfully crete the file, just return 0
	return 0;
}

/***********************************************************************
 * Function name: write_to_file
 * What it does? write to a file with inode_num. This will take into 
 * account the offset, length and what to write to buffer
 * Parameters(4): inode_num: the number of the inode of the file, offset,
 * size (in byte) of contents, and buffer: data will be written to a file.
 * Return: 0 if successful and -1 if error occurs
 ***********************************************************************/
int write_to_file(int inode_num, int offset, int size, void *buffer) {
	// Locate inode_num in ifile
	struct inode *target_inode = malloc( size_of_inode );
	// the we read the inode from iFile to a target_inode
	read_ifile(inode_num, target_inode);
	int next_blk = offset / s_blk_size; // this is the start of the writting.
	// write blocks of the file we will get the size and then for each small blk we write it
	// to the file and subtract the data from variable size
	int data_written = 0;
	// remaining data originally is the whole size but then will be updated
	int remaining_data = size;
	// keep writing until we have nothing left on the remaining_data 
	int return_value = 0;
	while(remaining_data != 0){
		// increment the data that has been written to a file.
		int offset_to_write = (offset + data_written) % s_blk_size;
		struct addr *target_addr = malloc( size_of_addr ); // allocate the address for the writing block
		get_addr(inode_num, next_blk, target_addr);
		// check if we have to write to a new block or not
		bool new_block_needed = target_addr->seg_num == AVAIL_ADDR || (next_blk == 4 && target_addr->seg_num == AVAIL_ADDR);
		if(new_block_needed) {
			// check if we have ability to write to a file or not.
			if(create_ifile_block(inode_num, next_blk) == -1){
				printf("Error: Cannot make file, EXCEEDED Files Limitation!!!\n");
				return -1*ENFILE;
			}
			read_ifile(inode_num, target_inode);
			// write date to file and update the data has been written counter so far.
			return_value = write_data_to_block(target_inode, next_blk, offset_to_write, remaining_data, buffer + data_written, 1);
		} else {
			// write date to file and update the data has been written counter so far, no need to create a new data
			return_value = write_data_to_block(target_inode, next_blk, offset_to_write, remaining_data, buffer + data_written, 0);
		}
		if(return_value < 0)	{
			return return_value;
		} else {
			data_written += return_value;
		}
		// decrease the data left to write
		remaining_data = size - data_written;
		next_blk = next_blk + 1; //increament the next block to write to
	}
	//set up the timer
	target_inode->lst_mdf = time(NULL); // updated by shkim
	target_inode->lst_acs = time(NULL); // updated by shkim
	read_ifile(inode_num, target_inode);
	return 0;
}

/***********************************************************************
 * Function name: read_file
 * What it does? read from a file identified by inode_num. First, find the
 * ifile, then using inode_num to locate the inode. For phase 1 we only
 * worry about the direct block. Then use read_log to get the data in blocks.
 * Parameters(4): inode_num: the number of the inode of the file, offset,
 * size (in byte) of contents, and buffer.
 * Return: return nothing
 ***********************************************************************/
void read_file(int inode_num, int offset, int size, void *buffer) {
	struct inode *target_inode = malloc( size_of_inode ); // create the variable to hold target inode
	// then read the ifile at this target inode
	read_ifile(inode_num, target_inode);

	if(target_inode->size != 0 || inode_num == chk_ptr->ifile_inode.inum) {	// shkim 1112. add "or condition" for read_ifile (ifile's inode_num = 0)
		// read block of the file
		int current_read = 0;
		int remaining_data = size - current_read; // begining, remaining_data is the size.
		// start reading data one block at a time. This is the starting block for this inode
		int next_blk = offset / s_blk_size;
		while(remaining_data != 0){
			struct addr *blk_addr = malloc( size_of_addr ); //allocate the block address so that we can 
			// get hold of it. This needs to be the same size of the size_of_addr
			get_addr(inode_num, next_blk, blk_addr);
			int num_block = (offset + current_read) % s_blk_size;
			// then read the block
			int read_size = s_blk_size - num_block; // size of the attempt read
			read_size = read_size > remaining_data? remaining_data : s_blk_size - num_block; // make sure that we do not exceed the remaining

//			if(inode_num == chk_ptr->ifile_inode.inum)
//				printf("read_file about ifile blk_addr=%d,%d, read_size=%d, current_read=%d\n"
//						,blk_addr->seg_num,blk_addr->block_num,read_size,current_read);
//
			Log_Read(blk_addr, read_size, buffer+current_read); // read from block
//			if(inode_num == chk_ptr->ifile_inode.inum)	{
//				struct inode *test_inode = malloc( size_of_inode );
//				memcpy(test_inode,buffer+current_read,size_of_inode);
//				printf("read_file about ifile result test_inode=%d,%d,%d, type=%d, size=%d\n"
//						,test_inode->inum,test_inode->ptrs[0].seg_num,test_inode->ptrs[0].block_num,test_inode->type,test_inode->size);
//			}
			current_read += read_size; // add what we have read to current_read
			remaining_data = size - current_read; // substract what we have read.
			next_blk = next_blk + 1; // go to next block
			free(blk_addr); //free that block so we don't have junk space built up
		}

	target_inode->lst_acs = time(NULL);		// shkim		
	}
	// there is nothing here
}

/***********************************************************************
 * Function name: free_file
 * What it does? free a file identified by inode_num. 
 * Parameters(1): inode_num: the number of the inode of the file, offset
 * Return: nothing
 ***********************************************************************/
void free_file(int inode_num) {
	struct inode *target_inode = malloc( size_of_inode ); // allocate the space for target inode
	memset(target_inode, 0,  size_of_inode ); //now clear out everything there

	// now write a clear inode to the memory
	struct inode *blk = malloc(s_blk_size);
	read_ifile(inode_num, blk);

	memcpy(blk + (inode_num *  size_of_inode ) % s_blk_size, target_inode, size_of_inode); // write to mem

	write_ifile(inode_num, blk);	// shkim 1114 (refactoring ifile)
}

/*
 *----------------------------------------------------------------------
 * function name: read_ifile
 * What it does? return blocks of data as ifile after reading from ifiles
 * Parameters (2): file_inum: the inum of the file we want to read, 
 * target_inode, the inode that we want to read from     
 * Return: nothing
 *----------------------------------------------------------------------
 */
void read_ifile(int file_inum, struct inode *target_inode){

	void  *ifile = malloc(s_blk_size);
	int blk_num = file_inum*size_of_inode/s_blk_size; // calculate the blk num to read to

	// ifile : read from log to avoid recursive call between read_ifile and read_file (shkim 1112)
	if(file_inum==chk_ptr->ifile_inode.inum) {
		struct addr *ifile_addr = malloc( size_of_addr ); // allocate space for address for ifile
		// assigin segment num to the ifile addr
		// assigin block num to ifile
		get_addr(file_inum, blk_num, ifile_addr);
//		printf("reading ifile ::: file_inum:%d, blk_num:%d, ifile_addr:%d,%d\n\n",file_inum,blk_num, ifile_addr->block_num,ifile_addr->seg_num);
		Log_Read(ifile_addr, s_blk_size, ifile);

	// general file : read from file
	} else {
		// read_ifile from not Log but File
//		printf("reading general file ::: file_inum:%d, ifile_inum:%d\n\n",file_inum,chk_ptr->ifile_inode.inum);
//		read_file(chk_ptr->ifile_inode.inum, file_inum*size_of_inode, s_blk_size, ifile);
		read_file(chk_ptr->ifile_inode.inum, 0, s_blk_size, ifile);
	}

	int space_allocation = (file_inum *  size_of_inode ) % s_blk_size;
	memcpy(target_inode, ifile + space_allocation,  size_of_inode );
}

/*
 *----------------------------------------------------------------------
 * function name: write_ifile
 * What it does? writing into ifile
 * Parameters (2): file_inum: the inum of the file we want to write,
 * target_inode, the inode that we want to write into
 * Return: nothing
 *----------------------------------------------------------------------
 */
void write_ifile(int file_inum, struct inode *target_inode){
	// shkim 1114 (refactoring ifile)
	int ifile_blk = (file_inum *  size_of_inode ) / s_blk_size;
	struct inode *ifile_inode = malloc( size_of_inode ); // create the variable to hold target inode
	// then read the ifile at this ifile inode
	read_ifile(chk_ptr->ifile_inode.inum, ifile_inode);
//	read_file(int inode_num, int offset, int size, void *buffer)
	void* buffer_whole_ifile = malloc(s_blk_size);
	read_file(chk_ptr->ifile_inode.inum, 0, s_blk_size, buffer_whole_ifile);
	memcpy(buffer_whole_ifile+((file_inum *  size_of_inode ) % s_blk_size), target_inode, size_of_inode);

//	write_data_to_block(ifile_inode, ifile_blk, (file_inum *  size_of_inode ) % s_blk_size, size_of_inode, target_inode, 0);
	write_data_to_block(ifile_inode, ifile_blk, 0, s_blk_size, buffer_whole_ifile, 0);
//	write_data_to_block(struct inode *target_inode, int block, int offset, int remaining_data, void *buffer, int new_block){

}


/******************************************HELPERS***********************************************
Below functions are HELPERS that make the function with main ideas more readable.
These are not main function but for us to reuse repeated code whenever possible.
*************************************************************************************************/

/***********************************************************************
 * Function name: set_file_inode
 * What it does? Assign inode_num and ftype to inode instance, we will call set_file_inode 
 * to set all attributes from file_inode
 * Parameters(3): inode, inode_num, ftype.
 * Return: void (just set up the file_node)
 ***********************************************************************/
void set_file_inode(struct inode *file_inode, int inode_num, int ftype) {
	file_inode->inum = inode_num;
	file_inode->type = ftype;
	// We have to check the file type: 0 is ifile, 1 is file, 2 is directory, 3 symlink maybe
	// This will help us create the correct inode_inum mode and hence be able to call the correct command - HV
	switch(ftype) {
		case 1: /*file*/
      		file_inode->mode = S_IFREG | 0777;
      		break; 
   		case 2: /* directory */
   			file_inode->mode = S_IFDIR | 0777;
      		break; /* optional */
   		/* you can have any number of case statements */
   		case 3: /* syslink */
   			file_inode->mode = S_IFLNK | 0777;
   			break;
	}
	file_inode->lst_cre = time(NULL);		// shkim
	file_inode->lst_acs = time(NULL);		// shkim
	file_inode->lst_mdf = time(NULL);
	file_inode->n_links = 1;
	file_inode->size = 0;
	for (int pointer_id = 0; pointer_id <= 3; pointer_id++) {
		file_inode->ptrs[pointer_id].seg_num = AVAIL_ADDR;//set file_inode segment to available address on lfs
	}
	file_inode->indirect.seg_num = AVAIL_ADDR; // this might not be for this one yet for Phase 2 but leave it here for now
	for (int pointer_id = 0; pointer_id <= 3; pointer_id++) {
		file_inode->ptrs[pointer_id].block_num = AVAIL_ADDR; //set file_inode block to available address on lfs
	}
	file_inode->indirect.block_num = AVAIL_ADDR; // this might not be for this one yet for Phase 2 but leave it here for now
}
/***********************************************************************
 * Function name: write_data_to_block
 * What it does? given a target_inode and the block id, this function will write data  to the buffer. 
 * Depends on if we need to create a new block or not.
 * Parameters(6): target_inode, int block: block id to write data to, int offset, int remaining_length: size of data to write
 * void *buffer, and int new_block: to determine if we need a new block or not (1 means new block)
 * Return: int the size of data we have written to the block
 ***********************************************************************/
int write_data_to_block(struct inode *target_inode, int block, int offset, int remaining_data, void *buffer, int new_block){
	// allocate the block to write data
	void *data_blk = malloc(s_blk_size);
	memset(data_blk, 0, s_blk_size); // clean the memory. shkim 10/23/2020
	if(target_inode->indirect.seg_num != AVAIL_ADDR) {
		int block_alloc = block * s_blk_size;
		read_file(target_inode->inum, block_alloc, s_blk_size, data_blk);
	}
	// change the size of the content needed to write to
	int blk_available_size = s_blk_size - offset;
	// now if the block available size is not right, we decrease the block size.
	if(blk_available_size > remaining_data) {
		blk_available_size = remaining_data;
	}
	//set up the space for content in memory
	void* content = data_blk + offset;
	memcpy(content, buffer, blk_available_size);
	// Change of size
	int change_of_size = 0;
	if((remaining_data < s_blk_size) || ((target_inode->size / s_blk_size) == block)){
		int boundary = target_inode->size % s_blk_size;
		// if the old_boundary is less than what we need, we have to increase the size.
		// change_of_size = old_boundary > (offset + blk_available_size) ? 0 : (offset + blk_available_size) - old_boundary;
		if (boundary <= (offset + blk_available_size)){
			change_of_size = offset + (blk_available_size - boundary); 	
		}
		
	} else{
		// if we need a new block, we have to set the change neededd to be increased to blk_available_size (where the content is)
		if (new_block != 0) {
			change_of_size = blk_available_size;
		}
	}

	// Write new data of block
	struct addr * target_addr = malloc( size_of_addr );
	int return_value = Log_Write(target_inode->inum, block, change_of_size, data_blk, target_addr);
	if(return_value < 0)	{
		return return_value;
	}
	// free the space allocated for the block so that we won't have garbage.
	free(target_addr);
	// return this size so that we can add it to data written.
	return blk_available_size;
}

/*******************************************************************************
 * function name: create_ifile_block
 * what it does? create the block for ifile
 * parameters(2): int inode_num and int i_blk
 * return 0 if successfully create a block and -1 otherwise
 ******************************************************************************/ 
int create_ifile_block(int inode_num, int i_blk){
	void *new_blk = malloc(s_blk_size); // allocate space for a new block
	struct addr *new_blk_addr = malloc( size_of_addr ); //allocate spacce for the address
	if (Log_Write(inode_num, i_blk, 0, new_blk, new_blk_addr) == -1) {
		return -1;
	}  // update the log layer
	return 0;
}
