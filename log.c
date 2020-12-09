/*************************************************
 *
 *	filename: log.c
 *
 *	CSC 552 LFS Project Phase 1
 *
 *	Sunghoon Kim poizzonkorea@email.arizona.edu
 * 
 *	Hoang Van    vnhh@email.arizona.edu
 *
 *  A LFS layer header definition
 *************************************************/

#include "log.h"
#include "fsmeta.h"
#include "cleaner.h"		// shkim 1120. cleaner

// set these variable here to reduce code duplication and increase readability
Flash flash = NULL;
int nxt_tail_blk = 0;
int write_ctr = 0;
void *seg_hash_tab;
int avail_seg_ctr;
int size_seg_super_blk;
int size_seg;
int s_blk_size;

/***********************************************************************
 * function Log_Read
 * What it does? read and write data to a buffer given a address.
 * Parameters(3) log_addr, data_size, buffer.
 * Return: 0 if succeeded and others if failed
 ***********************************************************************/
int Log_Read(struct addr *log_addr, int data_size, void* buffer) {
	if (flash == NULL) {
		return -1; // return -1 if flash does not initialize
	}
	struct addr *addr = malloc(sizeof(struct addr));
	addr->seg_num = log_addr->seg_num;
	addr->block_num = 0;
	int isFlashReadSucceeded = 0;
	int num_sectors = addr->seg_num * super_blk_lfs->seg_size * super_blk_lfs->b_size + addr->block_num * super_blk_lfs->b_size;
	u_int sector_offset = num_sectors;
	u_int sector_n = super_blk_lfs->seg_size * super_blk_lfs->b_size;
	void *temp = malloc(FLASH_SECTOR_SIZE * sector_n);
	// shkim 1126. using segment buffer
//	printf("in Log_Read :: %d vs %d\n",log_addr->seg_num,tail_seg->seg_num);
	if(log_addr->seg_num == tail_seg->seg_num)	{
		memcpy(temp, tail_seg->blocks, FLASH_SECTOR_SIZE * sector_n);
	} else {
		isFlashReadSucceeded = Flash_Read(flash, sector_offset, sector_n, temp);
	}
	if (isFlashReadSucceeded == 0) {
		void *t = temp + log_addr->block_num * super_blk_lfs->b_size * FLASH_SECTOR_SIZE;
		memcpy(buffer, t, data_size); //write to mem
		free(temp); // free temp so we will not have overhead
		return isFlashReadSucceeded;
	}
	return isFlashReadSucceeded; // it failed report it
}

/******************************************************************************************
 * function name:Log_Write
 * What it does? write data to a block associate with log_addr
 * Parameters(5): inodde_num, blk_id, data_size, buffer, log_addr
 * Return: if fails return -1
 ****************************************************************************************/
int Log_Write(int inode_num, int blk_id, int data_size, void* buffer, struct addr *log_addr) {
	if (flash == NULL) {
		return -1*ENXIO; // no flash is initialized just failed the operation
	} else if (data_size > super_blk_lfs->b_size * FLASH_SECTOR_SIZE) {
		return -1*EDOM; //check if we have enough space to write data or not
	}

//	if(inode_num==chk_ptr->ifile_inode.inum)	{
//		struct inode *tmp_inode = malloc( size_of_inode );
//		memcpy(tmp_inode,buffer+data_size-size_of_inode,size_of_inode);
//		printf("Log_Write blk_id=%d, data_size=%d, size_of_inode=%ld, log_addr->seg_num=%d,block_num=%d,tmp_inode->inum=%d,type=%d\n"
//				,blk_id,data_size,size_of_inode,log_addr->seg_num,log_addr->block_num,tmp_inode->inum,tmp_inode->type);
//	}

	void *nxt_blk = tail_seg->blocks + s_blk_size * nxt_tail_blk;
	memcpy(nxt_blk, buffer,  s_blk_size ); //update mem
	(log_addr)->seg_num = tail_seg->seg_num; //set log_addr o what the tail segment is
	(log_addr)->block_num = nxt_tail_blk++;
	update_summary_table(tail_seg->blocks, nxt_tail_blk - 1, inode_num); //update the segment table

	// Clean Mechanism. shkim 1121
	int cleaned = start_cleaner();
	if( cleaned == -1 ) {
		printf("No space left on device\n\n");
		return -1*ENOSPC;	// flash full
	}

	// write to flash @consumed all blocks
	// shkim 1126. using segment buffer
	int i = 0;
	if(nxt_tail_blk+1 == super_blk_lfs->seg_size)	{
		i = write_to_flash();
	}


	if (inode_num != chk_ptr->ifile_inode.inum && inode_num != -1 && blk_id >= 0) {
		update_inode(inode_num, blk_id, (log_addr), data_size);
	} else if(inode_num == chk_ptr->ifile_inode.inum && cleaned<=0) {	// shkim 1114 (refactoring ifile)
		chk_ptr->ifile_inode.ptrs[blk_id].seg_num = (log_addr)->seg_num;	// shkim 1114 (refactoring ifile)
		chk_ptr->ifile_inode.ptrs[blk_id].block_num = (log_addr)->block_num;	// shkim 1114 (refactoring ifile)
	}

	return i; // if failed return -1
}

/******************************************************************************************
 * function name:write_to_flash
 * What it does? write data to a block associate with log_addr
 * Parameters(0)
 * Return: if fails return -1
 ****************************************************************************************/
int write_to_flash() {
	printf("write_to_flash @seg_num=%d\n",tail_seg->seg_num);
//	sleep(1);
	update_segment_usage_table(tail_seg->seg_num, SEG_USED); //update the table segment
	write_ctr += 1; //update the counter
	int isSucceeded = Flash_Write(flash, super_blk_lfs->seg_size * super_blk_lfs->b_size * tail_seg->seg_num,
			super_blk_lfs->seg_size * super_blk_lfs->b_size, tail_seg->blocks);
	if (isSucceeded != 0) {
		printf("write_to_flash failure in flash_write\n");
		return isSucceeded;
	}
	// reduce available segment - HV
	avail_seg_ctr = avail_seg_ctr-1;
	//find next available segment 
	int pointer = tail_seg->seg_num + 1;
	uint8_t *ptr = seg_hash_tab;
	while (pointer != tail_seg->seg_num) {
		ptr = ptr + pointer;
//		printf("write_to_flash find_next_available_segment %d,%d\n",*ptr,pointer);
		if (*ptr == 0) {
			tail_seg->seg_num = pointer;
			break;
		}
		pointer++;
		pointer %= super_blk_lfs->seg_num;
	}
	// found it
	memset(tail_seg->blocks, 0, super_blk_lfs->seg_size * super_blk_lfs->b_size * FLASH_SECTOR_SIZE); //update mem
	// Initialize segment summary table
	init_seg_summary(tail_seg->blocks);

	// shkim 1120. ckpt interval
	if(write_ctr % cp_range == 0)	{
		printf("Reach checkpoint interval. cp_range=%d, write_ctr=%d\n\n", cp_range, write_ctr);
		update_chk_ptr();
		write_ctr = 0;
	}

	return 0;
}

/***********************************************************************************
 * function name: Log_Free
 * What it does? free the log of a log_addr
 * Parameters(2): log_addr, free_size
 * Return: if succeed, 0 else, 1
 ***********************************************************************************/
int Log_Free(struct addr *log_addr, int free_size) {
	if (flash == NULL) {
		return -1; //check if flash is not initialized
	}
	int num_sectors = free_size / FLASH_SECTOR_SIZE; //how many sectors needed to be free
	if (num_sectors % FLASH_SECTOR_SIZE != 0) {
		num_sectors = num_sectors + 1;
	}
	int start_FlashBlock = log_addr->seg_num * super_blk_lfs->seg_size * super_blk_lfs->b_size / 16; 
	int rc = Flash_Erase(flash, start_FlashBlock, num_sectors); // erase whatever on the flash
	return rc; //return if succeeded or not
}


/*************************************************************************************
 * HELPERS
 * function name: update_inode
 * what it does? update information/data changes about the inode
 * parameters(5): inode_num, blk_id, blk_addr, and data_size
 * return: none
 ************************************************************************************/
void update_inode(int inode_num, int blk_id, struct addr *blk_addr, int data_size) {
	// Locate the block of inode_num.
	struct inode *inode = malloc( size_of_inode ); // create the variable to hold target inode
	read_ifile(inode_num, inode);	// shkim 1114 (refactoring ifile)
	printf("***** update_inode : inode->inum=%d, inode->size=%d, data_size=%d, blk_id=%d\n\n",inode->inum, inode->size, data_size,blk_id);

	// Finaly, we need to update the block address
	if (blk_id <= 3) {
		inode->ptrs[blk_id].seg_num = blk_addr->seg_num;
		inode->ptrs[blk_id].block_num = blk_addr->block_num;
	} else {
		void* indir_blk = malloc( s_blk_size );
		if(inode->indirect.seg_num == AVAIL_ADDR){
			struct addr *addr = malloc(size_of_addr);
			//create indirect block here as requested for Phase 1
			// will update it for better version
			memset(indir_blk, AVAIL_ADDR,  s_blk_size );
			Log_Write(inode_num, -1,  s_blk_size , indir_blk, addr);
			inode->indirect.seg_num = addr->seg_num; //update inode information
			inode->indirect.block_num = addr->block_num;
			free(addr); //free the space allocated for address
		}
		Log_Read(&(inode->indirect),  s_blk_size , indir_blk); //read data to indirect block
		struct addr *temp_indir_blk = indir_blk + (blk_id * size_of_addr);
		temp_indir_blk->seg_num = blk_addr->seg_num; //update info of temporary indirect block
		temp_indir_blk->block_num = blk_addr->block_num;
		struct addr *idr_addr = malloc(size_of_addr); //start wrting back to Flash
		Log_Write(inode_num, -1*blk_id,  s_blk_size , indir_blk, idr_addr);
		inode->indirect.seg_num = idr_addr->seg_num;
		inode->indirect.block_num = idr_addr->block_num;
	}
	inode->size += data_size; //update size of this inode
	inode->lst_mdf = time(NULL); //update timer

	write_ifile(inode_num, inode);	// shkim 1114 (refactoring ifile)
	//update chk_ptr at the end to complte the inode update
	if (chk_ptr->ifile_inode.size < (inode_num + 1) * size_of_inode) {
		chk_ptr->ifile_inode.size =	(inode_num + 1) * size_of_inode;
	}
}

/*****************************************************************************
 * HELPERS
 * function name: get_addr
 * This return the address
 ****************************************************************************/
void get_addr(int inode_num, int i, struct addr *addr) {
	struct inode *node = malloc(sizeof(struct inode));
	// 1) read ifile
	if(inode_num == chk_ptr->ifile_inode.inum) {
		addr->seg_num = chk_ptr->ifile_inode.ptrs[i].seg_num;
		addr->block_num = chk_ptr->ifile_inode.ptrs[i].block_num;
	// 2) read general file
	} else {
		read_ifile(inode_num, node); //read to node
		if (i < 4) {
			addr->seg_num = node->ptrs[i].seg_num;
			addr->block_num = node->ptrs[i].block_num;
		} else if (node->indirect.seg_num == AVAIL_ADDR) {
			addr->seg_num = AVAIL_ADDR;
			addr->block_num = AVAIL_ADDR;
		} else if (node->indirect.seg_num != AVAIL_ADDR) {
			void* indirect_blk = malloc( s_blk_size );
			Log_Read(&(node->indirect),  s_blk_size , indirect_blk);
			struct addr *inode_addr = indirect_blk + i * sizeof(struct addr);
			addr->seg_num = inode_addr->seg_num;
			addr->block_num = inode_addr->block_num;
		}
	}
	free(node); // free that node
}

/*
 *----------------------------------------------------------------------
 * Segment Summary Table :: contains information about the blocks in the segment
 *----------------------------------------------------------------------
 */

void init_seg_summary(void* new_block) {
	int total = 1 * super_blk_lfs->b_size * FLASH_SECTOR_SIZE / 16;
	uint16_t t = AVAIL_ADDR;
	int i = 0;
	while (i < total) {
//		memcpy(tail_seg->blocks + i * 2, &t, uint16_t_size);
		memcpy(new_block + i * 2, &t, uint16_t_size);
		i++;
	}
	nxt_tail_blk = size_seg; //finally update nxt_tail_blk
}

void update_summary_table(void* block, int block_num, uint16_t inum) {
//	memcpy(tail_seg->blocks + block_num * 2, &inum, sizeof(inum));
	memcpy(block + block_num * 2, &inum, sizeof(inum));
}


