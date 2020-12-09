/*************************************************
 *
 *	filename: cleaner.c
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
#include "cleaner.h"

int avail_seg_ctr;
int is_cleaning = 0;
struct addr *curr_merged_addr;
//void *curr_tail_seg;
void *segment;
void *merged_segment;
void *ifile;
struct indirect_block {
	struct indirect_block* next;
	uint16_t inum;
	void* ind_blk;
};
struct indirect_block* iblk;
int iblk_cnt = 0;
/*
 *----------------------------------------------------------------------
 *
 * Segment Cleaner
 * return 0 : no need to cleaner or clean successfully
 *       -1 : num of free segments < threshold cleaning stop
 *----------------------------------------------------------------------
 */
int start_cleaner() {
	// block recursive call
	if (is_cleaning==1)
		return 0;

	// num of free segments < threshold cleaning stop
	if (avail_seg_ctr < stop)
		return -1*ENOSPC;

	// no need to cleaner : num of used segments < threshold cleaning start
	if ((super_blk_lfs->seg_num - avail_seg_ctr) < start)
		return 0;

//	return 0;

	printf("START_CLEANER...%d, %d, %d, %d\n", avail_seg_ctr, start, super_blk_lfs->seg_num, super_blk_lfs->sb_seg_num);
	printf("START_CLEANER ifile address, %d seg, %d block\n",chk_ptr->ifile_inode.ptrs[0].seg_num,chk_ptr->ifile_inode.ptrs[0].block_num);

	merged_segment = malloc(super_blk_lfs->seg_size*s_blk_size);
	memset(merged_segment, 0, super_blk_lfs->seg_size * s_blk_size); //update mem
	curr_merged_addr = malloc(sizeof(struct addr));
	curr_merged_addr->seg_num = 1;
	curr_merged_addr->block_num = 0;

	// 1) backup tail segment
	void *backup_tail_segment = malloc(super_blk_lfs->seg_size*s_blk_size);
	int backup_tail_seg_num=tail_seg->seg_num;
	int backup_tail_block_num=nxt_tail_blk;
	int ifile_seg_num = 0; int ifile_block_num = 0;
	memcpy(backup_tail_segment, tail_seg->blocks, super_blk_lfs->seg_size*s_blk_size);
	int tail_seg_is_ifile = 0;
	if(get_block_inum(backup_tail_segment, backup_tail_block_num-1) == chk_ptr->ifile_inode.inum)	{
		printf("Cleaner before writing ifile\n");
		tail_seg_is_ifile = 1;
	} else {
		printf("Cleaner before writing normal file : %d\n",get_block_inum(backup_tail_segment, backup_tail_block_num-1));
	}

	// 2-1) read ifile from ifile or tail_seg
	ifile = malloc(s_blk_size);
	if(tail_seg_is_ifile == 1)	{
		memcpy(ifile, tail_seg->blocks + s_blk_size * (backup_tail_block_num-1), s_blk_size);
	} else {
		read_file(chk_ptr->ifile_inode.inum, 0, s_blk_size, ifile);
		ifile_seg_num = chk_ptr->ifile_inode.ptrs[0].seg_num;
		ifile_block_num = chk_ptr->ifile_inode.ptrs[0].block_num;
	}
	// 2-2) read indirect_block
	iblk = malloc(sizeof(struct indirect_block));
	struct indirect_block* curr_iblk;
	iblk->inum = 0;
	iblk->next = NULL;
	for(int kk=1;kk<chk_ptr->next_inum;kk++)	{
		struct inode *tmp_inode = malloc(size_of_inode);
		memcpy(tmp_inode, ifile+kk*size_of_inode, size_of_inode);
		if(tmp_inode->indirect.seg_num != AVAIL_ADDR && tmp_inode->inum > 0)	{
			printf("in start_cleaner Load indBlk. inum=%d\n",tmp_inode->inum);
			struct addr *tmp_addr = malloc(size_of_addr);
			tmp_addr->seg_num   = tmp_inode->indirect.seg_num;
			tmp_addr->block_num = tmp_inode->indirect.block_num;
			void* indirect_blk = malloc( s_blk_size );
			Log_Read(tmp_addr,  s_blk_size , indirect_blk);
			// first linked_list
			if(iblk->inum==0)	{
				iblk->inum = tmp_inode->inum;
				iblk->ind_blk = indirect_blk;
				curr_iblk = iblk;
			} else {
				// or not
				struct indirect_block* tmp = malloc(sizeof(struct indirect_block));
				tmp->inum = tmp_inode->inum;
				tmp->ind_blk = indirect_blk;
				tmp->next = NULL;
				curr_iblk->next = tmp;
				curr_iblk = tmp;
			}
			iblk_cnt++;
		}
	}

	// 3) loop "num of segment"-1 times to clean fragmented blocks ( except for first segment = superblock )
//	for (int ii=1;ii<35;ii++) {
	for (int ii=1;ii<super_blk_lfs->seg_num;ii++) {
		uint8_t *temp = seg_hash_tab + ii;

		// 3-1) unused segment identified by sut,
		if (*temp == SEG_USED) {
			// 3-2) read current segment to check need_to_clean blocks
			int* ntc = malloc(super_blk_lfs->seg_size*sizeof(int));
//			int blank_count = 0;
			need_to_clean(ii, ntc);

			for(int jj=1;jj<super_blk_lfs->seg_size;jj++)	{
				if (ntc[jj] < 0) {
					// 3-3-1) if unused_block, skip and tagging;
					is_cleaning = 1;
//					blank_count++;
				} else {
					// 3-3-2) if used_block, append_block_to_clean_segment
					append_block_to_clean_segment(segment+jj*s_blk_size, ntc[jj],ii,jj);
				}
			}
		}
	}

	if(is_cleaning>0)	{
		// 4) loop merged_segments ~ backup_tail_seg to log_free
		for (int ii=curr_merged_addr->seg_num;ii<backup_tail_seg_num;ii++) {
			struct addr *old_addr = malloc(sizeof(struct addr));
			old_addr->seg_num = ii;
			old_addr->block_num = 0;
			printf("clear segment no.%d\n",ii);
			Log_Free(old_addr, super_blk_lfs->seg_size*s_blk_size);
			avail_seg_ctr++;
		}

		// 5) revoke backup tail segment
		for(int kk=0;kk<5;kk++)	{
			struct inode *tmp_inode = malloc(size_of_inode);
			memcpy(tmp_inode, ifile+kk*size_of_inode, size_of_inode);
			printf("debug ifile %d, %d, %d\n",tmp_inode->inum,tmp_inode->ptrs[0].seg_num,tmp_inode->ptrs[0].block_num);
		}
		// 5-1) backup_tail_seg + indirect_block + ifile -> merged_segment
		for (int ii=1;ii<backup_tail_block_num;ii++) {
			append_block_to_clean_segment(backup_tail_segment+ii*s_blk_size, get_block_inum(backup_tail_segment, ii),backup_tail_seg_num,ii);
		}
		if(iblk_cnt > 0)	{
			int ii=0;
			struct indirect_block *curr = iblk;
			append_block_to_clean_segment(curr->ind_blk, curr->inum,0,0);
			printf("%d'th indirect block has inserted\n",ii);
			while(curr->next != NULL) {
				ii++;
				curr = curr->next;
				append_block_to_clean_segment(curr->ind_blk, curr->inum,0,0);
				printf("%d'th indirect block has inserted\n",ii);
			}
		}
		append_block_to_clean_segment(ifile, chk_ptr->ifile_inode.inum,ifile_seg_num,ifile_block_num);

		// 5-2) merged_segment -> tail_seg
		tail_seg->seg_num = curr_merged_addr->seg_num;
		nxt_tail_blk = curr_merged_addr->block_num;
		printf("revoke new tail_seg. seg_num=%d, blk_num=%d\n",tail_seg->seg_num,nxt_tail_blk);
		memcpy(tail_seg->blocks, merged_segment, super_blk_lfs->seg_size*s_blk_size);
		// 5-3) write to log
		write_to_flash();
	}

	is_cleaning = 0;

	return 1;
}

// append input using block into merged segment
// if merged segment become full, then write_to_flush() and reinit
void append_block_to_clean_segment(void* block, int inum, int old_seg_num, int old_block_num)	{
	printf("start to append_block_to_clean_segment. curr_merged_addr seg_num=%d, block_num=%d, (old)inode_num=%d, seg_num=%d, block_num=%d\n"
			,curr_merged_addr->seg_num, curr_merged_addr->block_num, inum, old_seg_num, old_block_num);

	// 1) if first block, then init_seg_summary_table
	if(curr_merged_addr->block_num == 0)	{
		// init segment_summary_table
		init_seg_summary(merged_segment);
		curr_merged_addr->block_num++;
	}

	// 2) copy @current_merged_next block and update ifile_in_memory
	memcpy(merged_segment + (curr_merged_addr->block_num*s_blk_size), block, s_blk_size);
	update_summary_table(merged_segment, curr_merged_addr->block_num, inum);

	struct inode *target_inode = malloc(size_of_inode);
	memcpy(target_inode, ifile+inum*size_of_inode, size_of_inode);
	printf("append_block_to_clean %d, %d, %d\n",target_inode->inum,target_inode->ptrs[0].seg_num,target_inode->ptrs[0].block_num);
	// 2-1) replace normal file's new address in ifile
	if(old_seg_num != 0 && old_block_num != 0)	{
		int old_ptrs = 0;
		// 2-1-1) Check direct pointers
		for (int i = 0; i < 4; i++) {
			if(target_inode->ptrs[i].seg_num == old_seg_num && target_inode->ptrs[i].block_num == old_block_num)
				old_ptrs = i;
		}

		// 2-1-2) check indirect pointers
		if (target_inode->indirect.seg_num != AVAIL_ADDR) {
			void* indirect_blk = malloc( s_blk_size );
			Log_Read(&(target_inode->indirect),  s_blk_size , indirect_blk);
			for (int i = 0; i < s_blk_size/sizeof(struct addr); i++) {
				struct addr *inode_addr = indirect_blk + i * sizeof(struct addr);
				if(inode_addr->seg_num == old_seg_num && inode_addr->block_num == old_block_num)
					old_ptrs = i+4;
			}
		}

		// 2-1-3) replace new address in inode
		if(old_ptrs>4)	{
			// ind block
			if(iblk_cnt > 0)	{
				struct indirect_block *curr = iblk;
				printf("append_block_to_clean indirect file address inum(%d)==curr->inum(%d)\n",inum,curr->inum);
				if(inum==curr->inum)	{
					struct addr *inode_addr = curr->ind_blk + ((old_ptrs-4) * sizeof(struct addr));
					inode_addr->seg_num   = curr_merged_addr->seg_num;
					inode_addr->block_num = curr_merged_addr->block_num;
					printf("append_block_to_clean update indirect file address 1, %d inum, %d seg, %d block\n",curr->inum,inode_addr->seg_num,inode_addr->block_num);
				}
				while(curr->next != NULL) {
					curr = curr->next;
					if(inum==curr->inum)	{
						struct addr *inode_addr = curr->ind_blk + ((old_ptrs-4) * sizeof(struct addr));
						inode_addr->seg_num   = curr_merged_addr->seg_num;
						inode_addr->block_num = curr_merged_addr->block_num;
						printf("append_block_to_clean update indirect file address 2, %d inum, %d seg, %d block\n",curr->inum,inode_addr->seg_num,inode_addr->block_num);
					}
				}
			}
		} else {
			target_inode->ptrs[old_ptrs].seg_num  =curr_merged_addr->seg_num;
			target_inode->ptrs[old_ptrs].block_num=curr_merged_addr->block_num;
			// update ifile info @chk_ptr
			if(inum == chk_ptr->ifile_inode.inum)	{
				chk_ptr->ifile_inode.ptrs[old_ptrs].seg_num  =curr_merged_addr->seg_num;
				chk_ptr->ifile_inode.ptrs[old_ptrs].block_num=curr_merged_addr->block_num;
				printf("append_block_to_clean update ifile address, %d seg, %d block\n",chk_ptr->ifile_inode.ptrs[old_ptrs].seg_num,chk_ptr->ifile_inode.ptrs[old_ptrs].block_num);
			}
		}
	} else {
	// 2-2) replace indirect block's new address in ifile
		target_inode->indirect.seg_num   = curr_merged_addr->seg_num;
		target_inode->indirect.block_num = curr_merged_addr->block_num;
	}
	memcpy(ifile+inum*size_of_inode, target_inode, size_of_inode);

	// 3) if merged_segment meets the end of block, then rewrite merged_segments
	if(curr_merged_addr->block_num+1 == super_blk_lfs->seg_size)	{
		struct addr *old_addr = malloc(sizeof(struct addr));
		old_addr->seg_num = curr_merged_addr->seg_num;
		old_addr->block_num = 0;
		Log_Free(old_addr, super_blk_lfs->seg_size*s_blk_size);

		tail_seg->seg_num = curr_merged_addr->seg_num;
		memcpy(tail_seg->blocks,segment,super_blk_lfs->seg_size*s_blk_size);
		write_to_flash();

		merged_segment = malloc(super_blk_lfs->seg_size*s_blk_size);
		memset(merged_segment, 0, super_blk_lfs->seg_size * s_blk_size); //update mem
		curr_merged_addr->seg_num++;
		curr_merged_addr->block_num = 0;
	} else {
		curr_merged_addr->block_num++;
	}
}

// search segment summary table and return inum
uint16_t get_block_inum(void* segment, int block_num) {
	uint16_t *inum = segment + block_num * 2;
	return *inum;
}

// check segment summary table and every_blocks are fragmented
// return(every blocks) : -1 - need clean; inum - no need clean
void need_to_clean(int seg_num, int* ntc) {
	// 1) Read the segment
	segment = malloc(super_blk_lfs->seg_size*s_blk_size);
	struct addr *seg_addr = malloc(sizeof(struct addr));
	seg_addr->seg_num = seg_num;
	seg_addr->block_num = 0;
	Log_Read(seg_addr, super_blk_lfs->seg_size*s_blk_size, segment);
	char* cur_block_usage = calloc(sizeof(char), super_blk_lfs->seg_size-1);
	ntc[0]=-1;

	// 2) check every blocks ( except for first block = segment_summary )
	for (int i = 1; i < super_blk_lfs->seg_size; i++) {
		ntc[i]=-1;

		int inum = get_block_inum(segment, i);
		if(inum < 0 || inum == AVAIL_ADDR)	{
			cur_block_usage[i-1]='0';
		} else {
//			if(seg_num==10) printf("%d,%d\n",i,inum);
			cur_block_usage[i-1]='1';
			ntc[i] = inum;
		}
		struct addr *blk_addr = malloc(sizeof(struct addr));
		blk_addr->seg_num = seg_num;
		blk_addr->block_num = i;
		// valid inum && is_fragmented then need to clean!
		if (inum >= 0 && inum < 0xffff && is_fragmented(inum, blk_addr) < 0) {
			ntc[i] = -1;
		}
		free(blk_addr);
	}

	// debug for need_to_clean
	printf("%02d'th current seg_num's usage = %s\n",seg_num, cur_block_usage);
}

// return : -1 = fragmented; 0 = in use
int is_fragmented(int inum, struct addr *seg_addr) {
	struct inode *live_inode = malloc(size_of_inode);
//	read_ifile(inum, live_inode);
	memcpy(live_inode, ifile + (inum *  size_of_inode ) % s_blk_size,  size_of_inode );

	// 1) for data blocks
	// Check direct pointers
	for (int i = 0; i < 4; i++) {
		if (is_same_addresses(seg_addr, &(live_inode->ptrs[i])) == 0) {
			return 0;
		}
	}

	// check indirect pointers
	if (live_inode->indirect.seg_num != AVAIL_ADDR) {
		void* indirect_blk = malloc( s_blk_size );
		Log_Read(&(live_inode->indirect),  s_blk_size , indirect_blk);
		for (int i = 0; i < s_blk_size/sizeof(struct addr); i++) {
			struct addr *inode_addr = indirect_blk + i * sizeof(struct addr);
			if (is_same_addresses(seg_addr, inode_addr) == 0) {
				return 0;
			}
		}
	}

	// 2) for indirect blocks
	if (live_inode->indirect.seg_num != AVAIL_ADDR) {
		struct addr *inode_addr = malloc(sizeof(struct addr));
		inode_addr->seg_num  =live_inode->indirect.seg_num;
		inode_addr->block_num=live_inode->indirect.block_num;
		if (is_same_addresses(seg_addr, inode_addr) == 0) {
			return 0;
		}
	}

	return -1;
}

// return : 0 = same address, -1 = different address
int is_same_addresses(struct addr *addr1, struct addr *addr2) {
	if (addr1->seg_num == addr2->seg_num && addr1->block_num == addr2->block_num)
		return 0;
	return -1;
}
