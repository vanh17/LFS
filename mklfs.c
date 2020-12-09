/*************************************************
 *
 *	filename: mklfs.c
 *
 *	CSC 552 LFS Project Phase 1
 *
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 * 
 *	Hoang Van    vnhh@email.arizona.edu
 *
 *  A LFS layer header definition
 *************************************************/



#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include "fsmeta.h"
#include "flash.h"
#include <errno.h>
#include "directory.h"
#include "log.h"
#include "file.h"

#include <time.h>

// declare constants here for reduce code duplication
int sizeof_block = 2; 		// size of a block, in sectors
int sizeof_segment = 32;  	// segment size, in blocks, must be multiple of flash erase block size
int sizeof_flash = 100; 	// size of flash, in segments.
int wear_limit = 1000;		// wear limit for erase blocks
int segment_size = 0;
int super_blk_seg_size = 0; // size of segment (in terms of number) used by super block.
int num_of_block_superblock; //blocks being reserved for super block


/************************************************************************
 * Helpers for readability of the code
 * parameter(1): buffer to set root directory
 * return: nothing
 ***********************************************************************/
void set_buffer_to_root(void *ifile_buffer) {
	struct inode *ifile_inode = malloc( size_of_inode );
	ifile_inode->inum = 0;
	// set type to file
	ifile_inode->type = 1;
	ifile_inode->size = 3;
	// set first block to superblock and following blocks to be AVAIL_ADDR
	for (int i = 0; i <=3; i++) {
		if (i == 0) {
			ifile_inode->ptrs[i].seg_num = super_blk_seg_size;
			ifile_inode->ptrs[i].block_num = segment_size;
		} else {
			ifile_inode->ptrs[i].seg_num = AVAIL_ADDR;
			ifile_inode->ptrs[i].block_num = AVAIL_ADDR;
		}
	}
	// leave this simple indirect here for now, will work more on it on Phase 2
	ifile_inode->indirect.seg_num = AVAIL_ADDR;
	ifile_inode->indirect.block_num = AVAIL_ADDR;
	ifile_inode->mode = S_IFREG | 0400;
	ifile_inode->lst_cre = time(NULL);
	ifile_inode->lst_acs = time(NULL);
	ifile_inode->lst_mdf = time(NULL);
	ifile_inode->n_links = 1;
	memcpy(ifile_buffer, ifile_inode,  size_of_inode  );

	struct inode *root_inode = malloc( size_of_inode );
	root_inode->inum = 1;
	// set type to directory
	root_inode->type = 2;
	root_inode->size = size_of_dir + 3 * size_of_dir_entry;
	// set first block to superblock and following blocks to be AVAIL_ADDR
	for (int i = 0; i <=3; i++) {
		if (i == 0) {
			root_inode->ptrs[i].seg_num = super_blk_seg_size;
			root_inode->ptrs[i].block_num = segment_size+1;
		} else {
			root_inode->ptrs[i].seg_num = AVAIL_ADDR;
			root_inode->ptrs[i].block_num = AVAIL_ADDR;
		}
	}
	// leave this simple indirect here for now, will work more on it on Phase 2
	root_inode->indirect.seg_num = AVAIL_ADDR;
	root_inode->indirect.block_num = AVAIL_ADDR;
	memcpy(ifile_buffer +  size_of_inode  , root_inode,  size_of_inode  );
}

/*************************************************************************
 * Helpers
 * function name: save_chk_ptr
 * parameter(1): chk_ptr: the chk_ptr we wanted to save
 * return none
 ************************************************************************/
void save_chk_ptr(struct checkpoint_region *chk_ptr) {
	chk_ptr->timestamp = time(NULL); //start a timer
	chk_ptr->segment_usage_table = 0;
	chk_ptr->last_seg_addr.seg_num = super_blk_seg_size; // get the size of super block
	chk_ptr->next_inum = 3;
	chk_ptr->last_seg_addr.block_num = 0; //starting block is zero
	chk_ptr->ifile_inode.inum = 0; //set this to 0 for now
	// set type to ifile
	chk_ptr->ifile_inode.type = 0;
	// multiple by 2 for now will need to update for phase 2
	chk_ptr->ifile_inode.size = size_of_inode * 2;
	for (int i = 0; i <= 3; i++) {
		if (i == 0) {
			chk_ptr->ifile_inode.ptrs[i].seg_num = super_blk_seg_size;
			chk_ptr->ifile_inode.ptrs[i].block_num = segment_size;
		} else {
			chk_ptr->ifile_inode.ptrs[1].seg_num = AVAIL_ADDR; // next ptr go to available address
			chk_ptr->ifile_inode.ptrs[1].block_num = AVAIL_ADDR;
		}
	}
	// simply put this for phase 1. 
	chk_ptr->ifile_inode.indirect.seg_num = AVAIL_ADDR;
	chk_ptr->ifile_inode.indirect.block_num = AVAIL_ADDR;
}

/*************************************************************************
 * Helpers
 * function name: setup_super_blk
 * what it does? set up the newly created struct superblock so that we can
 * initialize all the value for it.
 * parameter(1): super_blk
 * return none
 ************************************************************************/
void setup_super_blk(struct superblock *super_blk) {
	super_blk->seg_size = sizeof_segment;
	super_blk->b_size = sizeof_block;
	super_blk->seg_num = sizeof_flash;
	super_blk->sb_seg_num = super_blk_seg_size;
	super_blk->checkpoint_addr.seg_num = super_blk_seg_size;
	super_blk->checkpoint_addr.block_num = 2 + + segment_size;		// checkpoint region is in the third block
	super_blk->seg_usage_table_addr.block_num = num_of_block_superblock % sizeof_segment;
	super_blk->seg_usage_table_addr.seg_num = num_of_block_superblock / sizeof_segment;
	if (super_blk->seg_usage_table_addr.block_num == 0) { //added one if it is even.
		super_blk->seg_usage_table_addr.seg_num += 1; //increase to larger segment.
	}
}


/*************************************************************************
 * Helpers
 * function name: setup_seg_summary_table
 * what it does? set up the newly created seg summary table so that we can
 * initialize all the value for it.
 * parameter(1): summary_buff
 * return none
 ************************************************************************/
void setup_seg_summary_table(void *summary_buff) {
	memset(summary_buff, AVAIL_ADDR, segment_size * sizeof_block * FLASH_SECTOR_SIZE);
	memset(summary_buff, 0, uint16_t_size*3);
	memset(summary_buff+uint16_t_size, 1, sizeof(uint8_t));
	memset(summary_buff+uint16_t_size*2, 2, sizeof(uint8_t));
}

int main(int argc, char *argv[]) {
	int opt = 0;
	struct option long_options[] = {
		{"block", 		required_argument, 0, 'b'}, 
		{"segment", 	required_argument, 0, 'l'},
		{"segments", 	required_argument, 0, 's'},
		{"wear_limit", 	required_argument, 0, 'w'}
	};
	char *file_name = argv[argc-1];
	int long_index = 0;
	opt = getopt_long(argc, argv, "b:l:s:w:", long_options, &long_index);
	while ((opt) != -1){
		if (opt == 'b') {
			sizeof_block = (int) strtol(optarg, (char **)NULL, 10);
		} else if(opt == 'l') {
			sizeof_segment = (int) strtol(optarg, (char **)NULL, 10);
		} else if(opt == 's') {
			sizeof_flash = (int) strtol(optarg, (char **)NULL, 10);
		} else if(opt == 'w') {
			wear_limit = (int) strtol(optarg, (char **)NULL, 10);
		}
		opt = getopt_long(argc, argv, "b:l:s:w:", long_options, &long_index);
	}
	int sizeof_flash_in_blocks = sizeof_flash * sizeof_segment;
	Flash_Create(file_name, wear_limit, sizeof_flash_in_blocks); // creat flash layer
	// initializing LFS
	num_of_block_superblock = super_blk_size / (sizeof_block * FLASH_SECTOR_SIZE);
	if(super_blk_size % (sizeof_block * FLASH_SECTOR_SIZE) != 0) num_of_block_superblock ++;
	int num_of_block_seg_usage_table = sizeof_flash * sizeof(uint8_t) / (sizeof_block * FLASH_SECTOR_SIZE);
	if(sizeof_flash * sizeof(uint8_t) % (sizeof_block * FLASH_SECTOR_SIZE) != 0) num_of_block_seg_usage_table ++;
	super_blk_seg_size = (num_of_block_superblock + num_of_block_seg_usage_table) / (sizeof_segment);
	if((num_of_block_superblock + num_of_block_seg_usage_table) % (sizeof_segment) != 0) super_blk_seg_size ++;
	segment_size = sizeof_segment / (sizeof_block * FLASH_SECTOR_SIZE /  uint16_t_size  );
	segment_size += (sizeof_segment % (sizeof_block * FLASH_SECTOR_SIZE /  uint16_t_size  ) == 0) ? 0 : 1;

	void *sb_buffer = malloc(sizeof_block * sizeof_segment * super_blk_seg_size * FLASH_SECTOR_SIZE);
	struct superblock *super_blk = sb_buffer;
	setup_super_blk(super_blk);
	

	// write the superblock in flash.
	u_int *n_blocks = malloc(sizeof(u_int));
	Flash flash = Flash_Open(file_name, FLASH_ASYNC, n_blocks);
	printf("%u\n", *n_blocks);

	// write segment usage table
	void *sut = sb_buffer + num_of_block_superblock * sizeof_block * FLASH_SECTOR_SIZE;
	memset(sut, 0, sizeof(sut));
	memset(sut, 1, sizeof(uint8_t) * (super_blk_seg_size + 1));

	// Write superblock to flash
	Flash_Write(flash, 0, sizeof_block * sizeof_segment * super_blk_seg_size, sb_buffer);


	void *ifile_buffer = malloc(sizeof_block * FLASH_SECTOR_SIZE); // root directory has one inode
	set_buffer_to_root(ifile_buffer);

	void *root = malloc( size_of_dir_entry  * 3 +  size_of_dir );
	// now creating root dir file
	struct dir *root_dir = malloc( size_of_dir );
	strncpy(root_dir->name, "/", sizeof(root_dir->name));
	root_dir->size = 3; 						// ".", ".." and ".ifile", initially 3 "files" in "/"
	void *temp = root;
	memcpy(temp, root_dir,  size_of_dir );
	temp = root +  size_of_dir ;

	struct dir_entry *entry_temp = malloc( size_of_dir_entry );
	strncpy(entry_temp->name, ".", sizeof(entry_temp->name));
	entry_temp->inum = 1;
	memcpy(temp, entry_temp,  size_of_dir_entry );

	temp = root +  size_of_dir  +  size_of_dir_entry ;
	strncpy(entry_temp->name, ".ifile", sizeof(entry_temp->name));
	entry_temp->inum = 0;
	memcpy(temp, entry_temp,  size_of_dir_entry );

	temp = root +  size_of_dir  +  size_of_dir_entry  * 2;
	strncpy(entry_temp->name, "..", sizeof(entry_temp->name));
	entry_temp->inum = 1;
	memcpy(temp, entry_temp,  size_of_dir_entry );


	// allocate the new check point here and save it.
	struct checkpoint_region *new_chk_ptr = malloc( size_of_chk_ptr );
	save_chk_ptr(new_chk_ptr);

	// Create and setup Summary Table
	void *summary_buff = malloc(segment_size * sizeof_block * FLASH_SECTOR_SIZE);
	setup_seg_summary_table(summary_buff);

	// Write ifile, root directory, initial checkpoint region to LFS
	void *total_buff = malloc(sizeof_block*FLASH_SECTOR_SIZE*(3 + segment_size));
	void *ptr_buff = total_buff;
	memcpy(ptr_buff, summary_buff, segment_size * sizeof_block * FLASH_SECTOR_SIZE);
	ptr_buff = total_buff + sizeof_block * FLASH_SECTOR_SIZE * segment_size;
	memcpy(ptr_buff, ifile_buffer, sizeof_block * FLASH_SECTOR_SIZE);
	ptr_buff = total_buff + sizeof_block * FLASH_SECTOR_SIZE * (segment_size + 1);
	memcpy(ptr_buff, root,  size_of_dir_entry  * 3 +  size_of_dir );
	ptr_buff = total_buff + sizeof_block * FLASH_SECTOR_SIZE * (segment_size + 2);
	memcpy(ptr_buff, new_chk_ptr,  size_of_chk_ptr );
	// write to FLASH layer
	Flash_Write(flash, super_blk_seg_size * sizeof_segment * sizeof_block, (3 + segment_size) * sizeof_block, total_buff);
	Flash_Close(flash); // close Flash layer
	return 0;
}
