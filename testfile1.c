#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

 /*
 * des takes two arguments on the command line:
 *    des -enc -ecb      -- encrypt in ECB mode
 *    des -enc -ctr      -- encrypt in CTR mode
 *    des -dec -ecb      -- decrypt in ECB mode
 *    des -dec -ctr      -- decrypt in CTR mode
 * des also reads some hardcoded files:
 *    message.txt            -- the ASCII text message to be encrypted,
 *                              read by "des -enc"
 *    encrypted_msg.bin      -- the encrypted message, a binary file,
 *                              written by "des -enc"
 *    decrypted_message.txt  -- the decrypted ASCII text message
 *    key.txt                -- just contains the key, on a line by itself, as an ASCII 
 *                              hex number, such as: 0x34FA879B
*/


uint64_t
u8tou64(uint8_t const u8[static 8]){
  uint64_t u64;
  memcpy(&u64, u8, sizeof u64);
  return u64;
}

unsigned createMask(unsigned a, unsigned b)
{
   unsigned r = 0;
   for (unsigned i=a; i<=b; i++)
       r |= 1 << i;

   return r;
}

/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////
typedef uint64_t KEYTYPE;
typedef uint32_t SUBKEYTYPE;
typedef uint64_t BLOCKTYPE;
typedef uint32_t HALFBLOCKTYPE;

struct BLOCK {
    BLOCKTYPE block;        // the block read
    int size;               // number of "real" bytes in the block, should be 8, unless it's the last block
    struct BLOCK *next;     // pointer to the next block
};

struct BLOCKHEAD {
    struct BLOCK *first;   // first block
    struct BLOCK *last;    // last block
};

typedef struct BLOCK* BLOCKNODE;
typedef struct BLOCKHEAD* BLOCKLIST;


BLOCKLIST newBlockList() {
   BLOCKLIST blocks = (BLOCKLIST)malloc(sizeof(struct BLOCKHEAD));
   blocks->first = NULL;
   blocks->last = NULL;
   return blocks;
}

BLOCKNODE newBlock(uint64_t block, int size){
  BLOCKNODE new_node = (BLOCKNODE) malloc(sizeof(struct BLOCK));
  new_node->block = block;
  new_node->size = size;
  new_node->next = NULL;
  return new_node;
}

void addBlockLast(BLOCKLIST list, BLOCKTYPE block, int size) {
   BLOCKNODE currBlock = newBlock(block, size);
   if (list->first == NULL) {
      list->first = currBlock;
      list->last = currBlock;
    } else {
      list->last->next = currBlock;
      list->last = currBlock;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Debugging
/////////////////////////////////////////////////////////////////////////////
int debug = 1;

void print_blocklist(BLOCKLIST head) {
   BLOCKNODE curr = head->first;
   int x = 0;
   assert(curr != NULL);

   while (curr != NULL) {
           printf("\tBLOCK [%d]: 0x%lx, size=%d\n", x, curr->block,curr->size);
      x++;
      curr = curr->next;
   }
}

void print_block(struct BLOCK* head) {
   
   printf("\tBLOCK : 0x%lx, size=%d\n", head->block,head->size);
      
}

/////////////////////////////////////////////////////////////////////////////
// Initial and final permutation
/////////////////////////////////////////////////////////////////////////////
uint64_t init_perm[] = {
   58,50,42,34,26,18,10,2,
   60,52,44,36,28,20,12,4,
   62,54,46,38,30,22,14,6,
   64,56,48,40,32,24,16,8,
   57,49,41,33,25,17,9,1,
   59,51,43,35,27,19,11,3,
   61,53,45,37,29,21,13,5,
   63,55,47,39,31,23,15,7
};

int final_perm[] = {
   40,8,48,16,56,24,64,32,
   39,7,47,15,55,23,63,31,
   38,6,46,14,54,22,62,30,
   37,5,45,13,53,21,61,29,
   36,4,44,12,52,20,60,28,
   35,3,43,11,51,19,59,27,
   34,2,42,10,50,18,58,26,
   33,1,41,9, 49,17,57,25
};

/////////////////////////////////////////////////////////////////////////////
// Subkey generation
/////////////////////////////////////////////////////////////////////////////

//SUBKEYTYPE subkeys[16];
KEYTYPE key = 0;    // Global variable key

// This function returns the i:th subkey, 48 bits long. To simplify the assignment 
// you can use a trivial implementation: just take the input key and xor it with i.
uint64_t getSubKey(int i) {
   // TODO: return the first 48 bits of the 56 bit DES key, xor:ed with i.
   return ((key ^ i) & 0xffffffffffff);
   //return subkeys[i];
}

// For extra credit, write the real DES key expansion routine!
//SUBKEYTYPE* generateSubKeys(KEYTYPE key) {
//   // TODO for extra credit
//  for( int i = 0; i<16; i++){
//    unsigned r = createMask(0,47);
//    unsigned result = r & (key^i);
//    subkeys[i] = result;
//  }
//  return subkeys;
//}

/////////////////////////////////////////////////////////////////////////////
// P-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t expand_box[] = {
   32,1,2,3,4,5,4,5,6,7,8,9,
   8,9,10,11,12,13,12,13,14,15,16,17,
   16,17,18,19,20,21,20,21,22,23,24,25,
   24,25,26,27,28,29,28,29,30,31,32,1
};

uint32_t Pbox[] = 
{
   16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,
   2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25,
};      

/////////////////////////////////////////////////////////////////////////////
// S-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t sbox_1[4][16] = {
   {14,  4, 13,  1,  2, 15, 11,  8,  3, 10 , 6, 12,  5,  9,  0,  7},
   { 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8},
   { 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0},
   {15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13}};

uint64_t sbox_2[4][16] = {
   {15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5 ,10},
   { 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5},
   { 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15},
   {13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9}};

uint64_t sbox_3[4][16] = {
   {10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8},
   {13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1},
   {13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7},
   { 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12}};


uint64_t sbox_4[4][16] = {
   { 7, 13, 14,  3,  0 , 6,  9, 10,  1 , 2 , 8,  5, 11, 12,  4 ,15},
   {13,  8, 11,  5,  6, 15,  0,  3,  4 , 7 , 2, 12,  1, 10, 14,  9},
   {10,  6,  9 , 0, 12, 11,  7, 13 ,15 , 1 , 3, 14 , 5 , 2,  8,  4},
   { 3, 15,  0,  6, 10,  1, 13,  8,  9 , 4 , 5, 11 ,12 , 7,  2, 14}};
 
 
uint64_t sbox_5[4][16] = {
   { 2, 12,  4,  1 , 7 ,10, 11,  6 , 8 , 5 , 3, 15, 13,  0, 14,  9},
   {14, 11 , 2 ,12 , 4,  7, 13 , 1 , 5 , 0, 15, 10,  3,  9,  8,  6},
   { 4,  2 , 1, 11, 10, 13,  7 , 8 ,15 , 9, 12,  5,  6 , 3,  0, 14},
   {11,  8 ,12 , 7 , 1, 14 , 2 ,13 , 6 ,15,  0,  9, 10 , 4,  5,  3}};


uint64_t sbox_6[4][16] = {
   {12,  1, 10, 15 , 9 , 2 , 6 , 8 , 0, 13 , 3 , 4 ,14 , 7  ,5 ,11},
   {10, 15,  4,  2,  7, 12 , 9 , 5 , 6,  1 ,13 ,14 , 0 ,11 , 3 , 8},
   { 9, 14 ,15,  5,  2,  8 ,12 , 3 , 7 , 0,  4 ,10  ,1 ,13 ,11 , 6},
   { 4,  3,  2, 12 , 9,  5 ,15 ,10, 11 ,14,  1 , 7  ,6 , 0 , 8 ,13}};
 

uint64_t sbox_7[4][16] = {
   { 4, 11,  2, 14, 15,  0 , 8, 13, 3,  12 , 9 , 7,  6 ,10 , 6 , 1},
   {13,  0, 11,  7,  4 , 9,  1, 10, 14 , 3 , 5, 12,  2, 15 , 8 , 6},
   { 1 , 4, 11, 13, 12,  3,  7, 14, 10, 15 , 6,  8,  0,  5 , 9 , 2},
   { 6, 11, 13 , 8,  1 , 4, 10,  7,  9 , 5 , 0, 15, 14,  2 , 3 ,12}};
 
uint64_t sbox_8[4][16] = {
   {13,  2,  8,  4,  6 ,15 ,11,  1, 10,  9 , 3, 14,  5,  0, 12,  7},
   { 1, 15, 13,  8 ,10 , 3  ,7 , 4, 12 , 5,  6 ,11,  0 ,14 , 9 , 2},
   { 7, 11,  4,  1,  9, 12, 14 , 2,  0  ,6, 10 ,13 ,15 , 3  ,5  ,8},
   { 2,  1, 14 , 7 , 4, 10,  8, 13, 15, 12,  9,  0 , 3,  5 , 6 ,11}};

/////////////////////////////////////////////////////////////////////////////
// I/O
/////////////////////////////////////////////////////////////////////////////

// Pad the list of blocks, so that every block is 64 bits, even if the
// file isn't a perfect multiple of 8 bytes long. In the input list of blocks,
// the last block may have "size" < 8. In this case, it needs to be padded. See 
// the slides for how to do this (the last byte of the last block 
// should contain the number if real bytes in the block, add an extra block if
// the file is an exact multiple of 8 bytes long.) The returned
// list of blocks will always have the "size"-field=8.
// Example:
//    1) The last block is 5 bytes long: [10,20,30,40,50]. We pad it with 2 bytes,
//       and set the length to 5: [10,20,30,40,50,0,0,5]. This means that the 
//       first 5 bytes of the block are "real", the last 3 should be discarded.
//    2) The last block is 8 bytes long: [10,20,30,40,50,60,70,80]. We keep this 
//       block as is, and add a new final block: [0,0,0,0,0,0,0,0]. When we decrypt,
//       the entire last block will be discarded since the last byte is 0
BLOCKLIST pad_last_block(BLOCKLIST blocks) {
    // TODO
  struct BLOCK* lastblock = blocks->last;
  if(debug)
      printf("Printing blocklist before padding\n");
      print_blocklist(blocks);

  uint8_t *p = (uint8_t *)&lastblock->block;
  //if you need a copy
  uint8_t result[8];
  for(int i = 0; i < 8; i++) {
      result[i] = p[i];
      if(debug)
      {
        printf("%x ", result[i]);
        printf("\n");
      }
  }
  int blocksize = lastblock->size;
  if( blocksize == 8)
  {
    // create a new empty block
    BLOCKTYPE emptyBlock = 0;
    BLOCKNODE paddedblock = newBlock(emptyBlock, 8);
    lastblock->next = paddedblock;
    lastblock = paddedblock;
    
  }
  else {
    result[7] = blocksize;
  
    uint64_t paddedblock = u8tou64(result);
    if(debug){
      printf("%lx\n", paddedblock);
    }
    lastblock->block = paddedblock;
    if(debug) {
      p = (uint8_t *)&paddedblock;
      //if you need a copy
       result[8];
      for(int i = 0; i < 8; i++) {
          result[i] = p[i];
          if (debug) {
            printf("%x ", result[i]);
            printf("\n");
          }
      }
    }

  }

  if(debug)
      printf("Printing blocklist after padding\n");
      print_blocklist(blocks);

  return blocks;
}

// Reads the message to be encrypted, an ASCII text file, and returns a linked list 
// of BLOCKs, each representing a 64 bit block. In other words, read the first 8 characters
// from the input file, and convert them (just a C cast) to 64 bits; this is your first block.
// Continue to the end of the file.

BLOCKLIST read_cleartext_message(FILE *msg_fp) {
   BLOCKLIST blocks = newBlockList();
   unsigned char byte = 0;
   int retVal = fscanf(msg_fp, "%c", &byte);
   BLOCKTYPE block = 0;
   int counter = 0;
   while (retVal > 0) {
      if (counter == 8) {
         addBlockLast(blocks, block, counter);
         counter = 0;
         block = 0;
      }
      block = (block << 8) | byte;
      counter++;
      retVal = fscanf(msg_fp, "%c", &byte);
   }
   if (counter > 0) {
      addBlockLast(blocks, block, counter);
   }
   blocks = pad_last_block(blocks);
   if (debug) {
      printf("Cleartext Blocks Read\n");
      print_blocklist(blocks);
   }
   return blocks;
}


// Reads the encrypted message, and returns a linked list of blocks, each 64 bits. 
// Note that, because of the padding that was done by the encryption, the length of 
// this file should always be a multiple of 8 bytes. The output is a linked list of
// 64-bit blocks.
BLOCKLIST read_encrypted_file(FILE *msg_fp) {
    BLOCKLIST blocks = newBlockList();
    uint64_t block;
    while(fread(&block, sizeof(block), 1, msg_fp) > 0){
      addBlockLast(blocks, block, 8);
      block = 0;
    }
    return blocks;
}

// Reads 56-bit key into a 64 bit unsigned int. We will ignore the most significant byte,
// i.e. we'll assume that the top 8 bits are all 0. In real DES, these are used to check 
// that the key hasn't been corrupted in transit. The key file is ASCII, consisting of
// exactly one line. That line has a single hex number on it, the key, such as 0x08AB674D9.
KEYTYPE read_key(FILE *key_fp) {
   char buff[17];
   fgets(buff, 17, key_fp);
   KEYTYPE key = strtol(buff, 0, 16);
   return key;
}

// Write the encrypted blocks to file. The encrypted file is in binary, i.e., you can
// just write each 64-bit block directly to the file, without any conversion.
void write_encrypted_message(FILE *msg_fp, BLOCKLIST msg) {
    assert(msg != NULL);
    BLOCKNODE curr = msg->first;
    char buffer[8];
    while(curr != NULL){
        memcpy(&buffer,&(curr->block),curr->size);
        fwrite(buffer, sizeof(char), 8, msg_fp);
        curr = curr->next;
    }
    return;
}

// Write the encrypted blocks to file. This is called by the decryption routine.
// The output file is a plain ASCII file, containing the decrypted text message.
void write_decrypted_message(FILE *msg_fp, BLOCKLIST msg) {
   assert(msg != NULL);
   BLOCKNODE cur = msg->first;
   int size;
   unsigned int charac;
   while(cur != NULL){
      for(int i=8; i>=1; i--){
         charac = (unsigned int) (((cur->block) >> ((i-1)*8)) & 0xFF);
         fprintf(msg_fp,"%c",charac);
      }
      cur = cur->next;
   }
}


/////////////////////////////////////////////////////////////////////////////
// Encryption
/////////////////////////////////////////////////////////////////////////////

BLOCKTYPE perm_64_64(BLOCKTYPE M, uint64_t *PERM_TABLE){
    BLOCKTYPE M_IP = 0;
    for (int i=0; i<64; i++){
        M_IP = M_IP | ((M>>(PERM_TABLE[i] - 1)) & (1)) << i;
    }
    return M_IP;
}

BLOCKTYPE perm_64_64_final(BLOCKTYPE M, int *FINAL_PERMTABLE){
    BLOCKTYPE FP = 0;
    for (int i=0; i<64; i++){
        FP = FP | ((M>>( (BLOCKTYPE)FINAL_PERMTABLE[i] - 1)) & (1)) << i;
    }
    return FP;
}

BLOCKTYPE perm_32_48(HALFBLOCKTYPE R, uint64_t *EXPAND_BOX){
    BLOCKTYPE EX_M = 0;
    for (int i=0; i<48; i++){
        EX_M = EX_M | ((R>>(EXPAND_BOX[i] - 1)) & (1)) << i;
    }
    return EX_M;
}

HALFBLOCKTYPE perm_32_32(HALFBLOCKTYPE substitutedMessage, uint32_t *PBOX){
    HALFBLOCKTYPE P_M = 0;
    for (int i=0; i<32; i++){
        P_M = P_M | ( (substitutedMessage >> (PBOX[i] - 1)) & (1)) << i;
    }
    return P_M;
}

HALFBLOCKTYPE unitSubstitute(HALFBLOCKTYPE input_6, uint64_t sbox[][16]){
    //printf("[DEBUG: unitSubstitute] Entered unitSubstitute function\n");
    //printf("[DEBUG: unitSubstitute] Value of 6 bit input is 0x%x\n", input_6);
    HALFBLOCKTYPE output_4;
    int row =  (2* ((input_6>>5) & 1)) + (input_6 & 1);
    //printf("[DEBUG: unitSubstitute] Row value is %d\n", row);
    int column = (input_6 >> 1) & 0xf;
    //printf("[DEBUG: unitSubstitute] Column value is %d\n", column);
    output_4 = (HALFBLOCKTYPE) sbox[row][column];
    //printf("[DEBUG: unitSubstitute] Value of calculated 4-bit output is 0x%x\n", output_4);
    return output_4;
}

HALFBLOCKTYPE substitute(BLOCKTYPE exp_message_48){
    HALFBLOCKTYPE substituted_message = 0;
    HALFBLOCKTYPE intermediate_input_6 = 0;
    HALFBLOCKTYPE intermediate_output_4 = 0;

    //printf("[DEBUG: substitute] Starting sbox substitution of 6 bit units\n");
    // Running S1 box
    intermediate_input_6 = (exp_message_48 & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_1);
    substituted_message = substituted_message | intermediate_output_4;

    // Running S2 box
    intermediate_input_6 = ((exp_message_48 >> 6) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_2);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S3 box
    intermediate_input_6 = ((exp_message_48 >> 12) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_3);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S4 box
    intermediate_input_6 = ((exp_message_48 >> 18) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_4);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S5 box
    intermediate_input_6 = ((exp_message_48 >> 24) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_5);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S6 box
    intermediate_input_6 = ((exp_message_48 >> 30) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_6);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S7 box
    intermediate_input_6 = ((exp_message_48 >> 36) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_7);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S8 box
    intermediate_input_6 = ((exp_message_48 >> 42) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_8);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    //printf("[DEBUG: substitute] Finished sbox substitution of 6 bit units\n");

    return substituted_message;
}

HALFBLOCKTYPE fiestel(HALFBLOCKTYPE righthalf, int i){
    // Expansion permutation 32 -> 48
    BLOCKTYPE exp_message = perm_32_48(righthalf, expand_box);

    // XOR with subkey[i]
    exp_message = exp_message ^ (getSubKey(i));

    // 8 S-box substitution 48 -> 32

    HALFBLOCKTYPE substituted_message = substitute(exp_message);


    // P-box permutation 32 -> 32 (return)
    HALFBLOCKTYPE fiestelOutput = perm_32_32(substituted_message, Pbox);

    return fiestelOutput;
}

// Encrypt one block. This is where the main computation takes place. It takes
// one 64-bit block as input, and returns the encrypted 64-bit block. The 
// subkeys needed by the Feistel Network is given by the function getSubKey(i).
BLOCKTYPE des_enc(BLOCKTYPE v){
   // TODO
   HALFBLOCKTYPE L;
   HALFBLOCKTYPE R;
   HALFBLOCKTYPE temp_R = 0;

   // Initial permutation on the Plaintext
   BLOCKTYPE IP = perm_64_64(v, init_perm);

   /*
   if (debug) {
       // print v in binary
       printf("[DEBUG] Printing Message Block\n");
       for(int i=63; i>=0; i--){
           printf("%ld [%d], ", (v>>i)&1, i+1); // Indexed from 1-64
       }
       printf("\n");
       // print IP
       printf("[DEBUG] Printing Message after Initial Permutation\n");
       for (int i=63; i>=0; i--){
           printf("%ld [%ld], ", (IP>>i)&1, init_perm[i]);  // Indexed from 1-64
       }
       printf("\n");
   }
   */

   L = (HALFBLOCKTYPE)(IP >> 32);
   R = (HALFBLOCKTYPE)(IP);

   for (int i = 0; i < 16; i++){
        temp_R = R;
        R = L ^ (fiestel(R, i));
        L = temp_R;
   }

    // Final switching between L and R
    temp_R = R;
    R = L;
    L = temp_R;

   // Final permutation on L and R
   BLOCKTYPE FP = ( ((BLOCKTYPE)L) << 32 ) | (BLOCKTYPE)R;
   FP = perm_64_64_final(FP, final_perm);

   return FP;
}

// Encrypt the blocks in ECB mode. The blocks have already been padded 
// by the input routine. The output is an encrypted list of blocks.
BLOCKLIST des_enc_ECB(BLOCKLIST msg) {
    assert(msg != NULL);
    BLOCKLIST encryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    while(start!=NULL)
    {
      BLOCKTYPE block = start->block;
      BLOCKTYPE encryptedblock = des_enc(block);
      if(debug){
          printf("Encrypted Block[%d] 0x%lx\n", i, encryptedblock);
          i++;
      }
      addBlockLast(encryptedBlockList, encryptedblock, start->size);
      start = start->next;
    }
   return encryptedBlockList;
}

// Same as des_enc_ECB, but encrypt the blocks in Counter mode.
// SEE: https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#Counter_(CTR)
// Start the counter at 0.
BLOCKLIST des_enc_CTR(BLOCKLIST msg) {
    assert(msg != NULL);
    // TODO
    // Should call des_enc in here repeatedly
    BLOCKLIST encryptedBlockList = newBlockList();
    BLOCKNODE start = msg->first;
    int ctr = 0;
    KEYTYPE original_key = key;
    int i = 0; // Used for debugging only
    while(start!=NULL) {
      key = original_key ^ ctr;
      BLOCKTYPE block = start->block;
      BLOCKTYPE encryptedblock = des_enc(block);
      if(debug) {
          printf("Encrypted Block[%d] 0x%lx\n", i, encryptedblock);
          i++;
      }
      addBlockLast(encryptedBlockList, encryptedblock, 8);
      start = start->next;
      key = original_key;
      ctr += 1;
    }
    return encryptedBlockList;
}

/////////////////////////////////////////////////////////////////////////////
// Decryption
/////////////////////////////////////////////////////////////////////////////
// Decrypt one block.
BLOCKTYPE des_dec(BLOCKTYPE v){
    // TODO
    HALFBLOCKTYPE L;
    HALFBLOCKTYPE R;
    HALFBLOCKTYPE temp_R = 0;

    // Initial permutation on the Plaintext
    BLOCKTYPE IP = perm_64_64(v, init_perm);

    /*
    if (debug) {
        // print v in binary
        printf("[DEBUG] Printing Message Block\n");
        for(int i=63; i>=0; i--){
            printf("%ld [%d], ", (v>>i)&1, i+1); // Indexed from 1-64
        }
        printf("\n");
        // print IP
        printf("[DEBUG] Printing Message after Initial Permutation\n");
        for (int i=63; i>=0; i--){
            printf("%ld [%ld], ", (IP>>i)&1, init_perm[i]);  // Indexed from 1-64
        }
        printf("\n");
    }
    */

    L = (HALFBLOCKTYPE)(IP >> 32);
    R = (HALFBLOCKTYPE)(IP);

    for (int i = 15; i >= 0; i--){
        temp_R = R;
        R = L ^ (fiestel(R, i));
        L = temp_R;
    }

    // Final switching between L and R
    temp_R = R;
    R = L;
    L = temp_R;

    // Final permutation on L and R
    BLOCKTYPE FP = ( ((BLOCKTYPE)L) << 32 ) | (BLOCKTYPE)R;
    FP = perm_64_64_final(FP, final_perm);

    return FP;
}

// Decrypt the blocks in ECB mode. The input is a list of encrypted blocks,
// the output a list of plaintext blocks.
BLOCKLIST des_dec_ECB(BLOCKLIST msg) {
    assert(msg != NULL);
    // Should call des_dec in here repeatedly
    BLOCKLIST decryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    while(start!=NULL)
    {
        BLOCKTYPE block = start->block;
        BLOCKTYPE decryptedblock = des_dec(block);
        if(debug){
            printf("Decrypted Block[%d] 0x%lx\n", i, decryptedblock);
            i++;
        }
        addBlockLast(decryptedBlockList, decryptedblock, start->size);
        start = start->next;
    }
    return decryptedBlockList;
}

// Decrypt the blocks in Counter mode
BLOCKLIST des_dec_CTR(BLOCKLIST msg) {
    assert(msg != NULL);
    // Should call des_dec in here repeatedly
    BLOCKLIST decryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    int ctr = 0;
    KEYTYPE original_key = key;
    while(start!=NULL)
    {
        key = original_key ^ ctr;
        BLOCKTYPE block = start->block;
        BLOCKTYPE decryptedblock = des_dec(block);
        if(debug){
            printf("Decrypted Block[%d] 0x%lx\n", i, decryptedblock);
            i++;
        }
        addBlockLast(decryptedBlockList, decryptedblock, start->size);
        start = start->next;
        key = original_key;
        ctr += 1;
    }
    return decryptedBlockList;

}

/////////////////////////////////////////////////////////////////////////////
// Main routine
/////////////////////////////////////////////////////////////////////////////

void encrypt (int argc, char **argv) {
      FILE *msg_fp = fopen("message.txt", "r");
      BLOCKLIST msg = read_cleartext_message(msg_fp);
      fclose(msg_fp);

      BLOCKLIST encrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         encrypted_message = des_enc_ECB(msg);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         encrypted_message = des_enc_CTR(msg);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "wb");
      write_encrypted_message(encrypted_msg_fp, encrypted_message);
      fclose(encrypted_msg_fp);
}

void decrypt (int argc, char **argv) {
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "rb");
      assert(encrypted_msg_fp != NULL);
      BLOCKLIST encrypted_message = read_encrypted_file(encrypted_msg_fp);
      fclose(encrypted_msg_fp);

      assert(encrypted_message != NULL);
      if(debug) {
          printf("[DEBUG: decrypt] Printing encrypted message block from encrypted binary file\n");
          print_blocklist(encrypted_message);
      }

      BLOCKLIST decrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         decrypted_message = des_dec_ECB(encrypted_message);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         decrypted_message = des_dec_CTR(encrypted_message);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };

      if (debug){
          print_blocklist(decrypted_message);
      }

      FILE *decrypted_msg_fp = fopen("decrypted_message.txt", "w");
      write_decrypted_message(decrypted_msg_fp, decrypted_message);
      fclose(decrypted_msg_fp);
}

int main(int argc, char **argv){
   FILE *key_fp = fopen("key.txt","r");
   key = read_key(key_fp);
   //generateSubKeys(key);              // This does nothing right now.
   fclose(key_fp);

   if (argc != 3) {
     printf("Two arguments expected.\n");
     exit(-1);
   }

   if (strncmp(argv[1],"-enc",4) == 0) {
      encrypt(argc, argv);
   } else if (strncmp(argv[1],"-dec",4) == 0) {
      decrypt(argc, argv);
   } else {
     printf("First argument should be -enc or -dec\n"); 
   }
   return 0;
}#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

 /*
 * des takes two arguments on the command line:
 *    des -enc -ecb      -- encrypt in ECB mode
 *    des -enc -ctr      -- encrypt in CTR mode
 *    des -dec -ecb      -- decrypt in ECB mode
 *    des -dec -ctr      -- decrypt in CTR mode
 * des also reads some hardcoded files:
 *    message.txt            -- the ASCII text message to be encrypted,
 *                              read by "des -enc"
 *    encrypted_msg.bin      -- the encrypted message, a binary file,
 *                              written by "des -enc"
 *    decrypted_message.txt  -- the decrypted ASCII text message
 *    key.txt                -- just contains the key, on a line by itself, as an ASCII 
 *                              hex number, such as: 0x34FA879B
*/


uint64_t
u8tou64(uint8_t const u8[static 8]){
  uint64_t u64;
  memcpy(&u64, u8, sizeof u64);
  return u64;
}

unsigned createMask(unsigned a, unsigned b)
{
   unsigned r = 0;
   for (unsigned i=a; i<=b; i++)
       r |= 1 << i;

   return r;
}

/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////
typedef uint64_t KEYTYPE;
typedef uint32_t SUBKEYTYPE;
typedef uint64_t BLOCKTYPE;
typedef uint32_t HALFBLOCKTYPE;

struct BLOCK {
    BLOCKTYPE block;        // the block read
    int size;               // number of "real" bytes in the block, should be 8, unless it's the last block
    struct BLOCK *next;     // pointer to the next block
};

struct BLOCKHEAD {
    struct BLOCK *first;   // first block
    struct BLOCK *last;    // last block
};

typedef struct BLOCK* BLOCKNODE;
typedef struct BLOCKHEAD* BLOCKLIST;


BLOCKLIST newBlockList() {
   BLOCKLIST blocks = (BLOCKLIST)malloc(sizeof(struct BLOCKHEAD));
   blocks->first = NULL;
   blocks->last = NULL;
   return blocks;
}

BLOCKNODE newBlock(uint64_t block, int size){
  BLOCKNODE new_node = (BLOCKNODE) malloc(sizeof(struct BLOCK));
  new_node->block = block;
  new_node->size = size;
  new_node->next = NULL;
  return new_node;
}

void addBlockLast(BLOCKLIST list, BLOCKTYPE block, int size) {
   BLOCKNODE currBlock = newBlock(block, size);
   if (list->first == NULL) {
      list->first = currBlock;
      list->last = currBlock;
    } else {
      list->last->next = currBlock;
      list->last = currBlock;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Debugging
/////////////////////////////////////////////////////////////////////////////
int debug = 1;

void print_blocklist(BLOCKLIST head) {
   BLOCKNODE curr = head->first;
   int x = 0;
   assert(curr != NULL);

   while (curr != NULL) {
           printf("\tBLOCK [%d]: 0x%lx, size=%d\n", x, curr->block,curr->size);
      x++;
      curr = curr->next;
   }
}

void print_block(struct BLOCK* head) {
   
   printf("\tBLOCK : 0x%lx, size=%d\n", head->block,head->size);
      
}

/////////////////////////////////////////////////////////////////////////////
// Initial and final permutation
/////////////////////////////////////////////////////////////////////////////
uint64_t init_perm[] = {
   58,50,42,34,26,18,10,2,
   60,52,44,36,28,20,12,4,
   62,54,46,38,30,22,14,6,
   64,56,48,40,32,24,16,8,
   57,49,41,33,25,17,9,1,
   59,51,43,35,27,19,11,3,
   61,53,45,37,29,21,13,5,
   63,55,47,39,31,23,15,7
};

int final_perm[] = {
   40,8,48,16,56,24,64,32,
   39,7,47,15,55,23,63,31,
   38,6,46,14,54,22,62,30,
   37,5,45,13,53,21,61,29,
   36,4,44,12,52,20,60,28,
   35,3,43,11,51,19,59,27,
   34,2,42,10,50,18,58,26,
   33,1,41,9, 49,17,57,25
};

/////////////////////////////////////////////////////////////////////////////
// Subkey generation
/////////////////////////////////////////////////////////////////////////////

//SUBKEYTYPE subkeys[16];
KEYTYPE key = 0;    // Global variable key

// This function returns the i:th subkey, 48 bits long. To simplify the assignment 
// you can use a trivial implementation: just take the input key and xor it with i.
uint64_t getSubKey(int i) {
   // TODO: return the first 48 bits of the 56 bit DES key, xor:ed with i.
   return ((key ^ i) & 0xffffffffffff);
   //return subkeys[i];
}

// For extra credit, write the real DES key expansion routine!
//SUBKEYTYPE* generateSubKeys(KEYTYPE key) {
//   // TODO for extra credit
//  for( int i = 0; i<16; i++){
//    unsigned r = createMask(0,47);
//    unsigned result = r & (key^i);
//    subkeys[i] = result;
//  }
//  return subkeys;
//}

/////////////////////////////////////////////////////////////////////////////
// P-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t expand_box[] = {
   32,1,2,3,4,5,4,5,6,7,8,9,
   8,9,10,11,12,13,12,13,14,15,16,17,
   16,17,18,19,20,21,20,21,22,23,24,25,
   24,25,26,27,28,29,28,29,30,31,32,1
};

uint32_t Pbox[] = 
{
   16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,
   2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25,
};      

/////////////////////////////////////////////////////////////////////////////
// S-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t sbox_1[4][16] = {
   {14,  4, 13,  1,  2, 15, 11,  8,  3, 10 , 6, 12,  5,  9,  0,  7},
   { 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8},
   { 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0},
   {15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13}};

uint64_t sbox_2[4][16] = {
   {15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5 ,10},
   { 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5},
   { 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15},
   {13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9}};

uint64_t sbox_3[4][16] = {
   {10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8},
   {13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1},
   {13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7},
   { 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12}};


uint64_t sbox_4[4][16] = {
   { 7, 13, 14,  3,  0 , 6,  9, 10,  1 , 2 , 8,  5, 11, 12,  4 ,15},
   {13,  8, 11,  5,  6, 15,  0,  3,  4 , 7 , 2, 12,  1, 10, 14,  9},
   {10,  6,  9 , 0, 12, 11,  7, 13 ,15 , 1 , 3, 14 , 5 , 2,  8,  4},
   { 3, 15,  0,  6, 10,  1, 13,  8,  9 , 4 , 5, 11 ,12 , 7,  2, 14}};
 
 
uint64_t sbox_5[4][16] = {
   { 2, 12,  4,  1 , 7 ,10, 11,  6 , 8 , 5 , 3, 15, 13,  0, 14,  9},
   {14, 11 , 2 ,12 , 4,  7, 13 , 1 , 5 , 0, 15, 10,  3,  9,  8,  6},
   { 4,  2 , 1, 11, 10, 13,  7 , 8 ,15 , 9, 12,  5,  6 , 3,  0, 14},
   {11,  8 ,12 , 7 , 1, 14 , 2 ,13 , 6 ,15,  0,  9, 10 , 4,  5,  3}};


uint64_t sbox_6[4][16] = {
   {12,  1, 10, 15 , 9 , 2 , 6 , 8 , 0, 13 , 3 , 4 ,14 , 7  ,5 ,11},
   {10, 15,  4,  2,  7, 12 , 9 , 5 , 6,  1 ,13 ,14 , 0 ,11 , 3 , 8},
   { 9, 14 ,15,  5,  2,  8 ,12 , 3 , 7 , 0,  4 ,10  ,1 ,13 ,11 , 6},
   { 4,  3,  2, 12 , 9,  5 ,15 ,10, 11 ,14,  1 , 7  ,6 , 0 , 8 ,13}};
 

uint64_t sbox_7[4][16] = {
   { 4, 11,  2, 14, 15,  0 , 8, 13, 3,  12 , 9 , 7,  6 ,10 , 6 , 1},
   {13,  0, 11,  7,  4 , 9,  1, 10, 14 , 3 , 5, 12,  2, 15 , 8 , 6},
   { 1 , 4, 11, 13, 12,  3,  7, 14, 10, 15 , 6,  8,  0,  5 , 9 , 2},
   { 6, 11, 13 , 8,  1 , 4, 10,  7,  9 , 5 , 0, 15, 14,  2 , 3 ,12}};
 
uint64_t sbox_8[4][16] = {
   {13,  2,  8,  4,  6 ,15 ,11,  1, 10,  9 , 3, 14,  5,  0, 12,  7},
   { 1, 15, 13,  8 ,10 , 3  ,7 , 4, 12 , 5,  6 ,11,  0 ,14 , 9 , 2},
   { 7, 11,  4,  1,  9, 12, 14 , 2,  0  ,6, 10 ,13 ,15 , 3  ,5  ,8},
   { 2,  1, 14 , 7 , 4, 10,  8, 13, 15, 12,  9,  0 , 3,  5 , 6 ,11}};

/////////////////////////////////////////////////////////////////////////////
// I/O
/////////////////////////////////////////////////////////////////////////////

// Pad the list of blocks, so that every block is 64 bits, even if the
// file isn't a perfect multiple of 8 bytes long. In the input list of blocks,
// the last block may have "size" < 8. In this case, it needs to be padded. See 
// the slides for how to do this (the last byte of the last block 
// should contain the number if real bytes in the block, add an extra block if
// the file is an exact multiple of 8 bytes long.) The returned
// list of blocks will always have the "size"-field=8.
// Example:
//    1) The last block is 5 bytes long: [10,20,30,40,50]. We pad it with 2 bytes,
//       and set the length to 5: [10,20,30,40,50,0,0,5]. This means that the 
//       first 5 bytes of the block are "real", the last 3 should be discarded.
//    2) The last block is 8 bytes long: [10,20,30,40,50,60,70,80]. We keep this 
//       block as is, and add a new final block: [0,0,0,0,0,0,0,0]. When we decrypt,
//       the entire last block will be discarded since the last byte is 0
BLOCKLIST pad_last_block(BLOCKLIST blocks) {
    // TODO
  struct BLOCK* lastblock = blocks->last;
  if(debug)
      printf("Printing blocklist before padding\n");
      print_blocklist(blocks);

  uint8_t *p = (uint8_t *)&lastblock->block;
  //if you need a copy
  uint8_t result[8];
  for(int i = 0; i < 8; i++) {
      result[i] = p[i];
      if(debug)
      {
        printf("%x ", result[i]);
        printf("\n");
      }
  }
  int blocksize = lastblock->size;
  if( blocksize == 8)
  {
    // create a new empty block
    BLOCKTYPE emptyBlock = 0;
    BLOCKNODE paddedblock = newBlock(emptyBlock, 8);
    lastblock->next = paddedblock;
    lastblock = paddedblock;
    
  }
  else {
    result[7] = blocksize;
  
    uint64_t paddedblock = u8tou64(result);
    if(debug){
      printf("%lx\n", paddedblock);
    }
    lastblock->block = paddedblock;
    if(debug) {
      p = (uint8_t *)&paddedblock;
      //if you need a copy
       result[8];
      for(int i = 0; i < 8; i++) {
          result[i] = p[i];
          if (debug) {
            printf("%x ", result[i]);
            printf("\n");
          }
      }
    }

  }

  if(debug)
      printf("Printing blocklist after padding\n");
      print_blocklist(blocks);

  return blocks;
}

// Reads the message to be encrypted, an ASCII text file, and returns a linked list 
// of BLOCKs, each representing a 64 bit block. In other words, read the first 8 characters
// from the input file, and convert them (just a C cast) to 64 bits; this is your first block.
// Continue to the end of the file.

BLOCKLIST read_cleartext_message(FILE *msg_fp) {
   BLOCKLIST blocks = newBlockList();
   unsigned char byte = 0;
   int retVal = fscanf(msg_fp, "%c", &byte);
   BLOCKTYPE block = 0;
   int counter = 0;
   while (retVal > 0) {
      if (counter == 8) {
         addBlockLast(blocks, block, counter);
         counter = 0;
         block = 0;
      }
      block = (block << 8) | byte;
      counter++;
      retVal = fscanf(msg_fp, "%c", &byte);
   }
   if (counter > 0) {
      addBlockLast(blocks, block, counter);
   }
   blocks = pad_last_block(blocks);
   if (debug) {
      printf("Cleartext Blocks Read\n");
      print_blocklist(blocks);
   }
   return blocks;
}


// Reads the encrypted message, and returns a linked list of blocks, each 64 bits. 
// Note that, because of the padding that was done by the encryption, the length of 
// this file should always be a multiple of 8 bytes. The output is a linked list of
// 64-bit blocks.
BLOCKLIST read_encrypted_file(FILE *msg_fp) {
    BLOCKLIST blocks = newBlockList();
    uint64_t block;
    while(fread(&block, sizeof(block), 1, msg_fp) > 0){
      addBlockLast(blocks, block, 8);
      block = 0;
    }
    return blocks;
}

// Reads 56-bit key into a 64 bit unsigned int. We will ignore the most significant byte,
// i.e. we'll assume that the top 8 bits are all 0. In real DES, these are used to check 
// that the key hasn't been corrupted in transit. The key file is ASCII, consisting of
// exactly one line. That line has a single hex number on it, the key, such as 0x08AB674D9.
KEYTYPE read_key(FILE *key_fp) {
   char buff[17];
   fgets(buff, 17, key_fp);
   KEYTYPE key = strtol(buff, 0, 16);
   return key;
}

// Write the encrypted blocks to file. The encrypted file is in binary, i.e., you can
// just write each 64-bit block directly to the file, without any conversion.
void write_encrypted_message(FILE *msg_fp, BLOCKLIST msg) {
    assert(msg != NULL);
    BLOCKNODE curr = msg->first;
    char buffer[8];
    while(curr != NULL){
        memcpy(&buffer,&(curr->block),curr->size);
        fwrite(buffer, sizeof(char), 8, msg_fp);
        curr = curr->next;
    }
    return;
}

// Write the encrypted blocks to file. This is called by the decryption routine.
// The output file is a plain ASCII file, containing the decrypted text message.
void write_decrypted_message(FILE *msg_fp, BLOCKLIST msg) {
   assert(msg != NULL);
   BLOCKNODE cur = msg->first;
   int size;
   unsigned int charac;
   while(cur != NULL){
      for(int i=8; i>=1; i--){
         charac = (unsigned int) (((cur->block) >> ((i-1)*8)) & 0xFF);
         fprintf(msg_fp,"%c",charac);
      }
      cur = cur->next;
   }
}


/////////////////////////////////////////////////////////////////////////////
// Encryption
/////////////////////////////////////////////////////////////////////////////

BLOCKTYPE perm_64_64(BLOCKTYPE M, uint64_t *PERM_TABLE){
    BLOCKTYPE M_IP = 0;
    for (int i=0; i<64; i++){
        M_IP = M_IP | ((M>>(PERM_TABLE[i] - 1)) & (1)) << i;
    }
    return M_IP;
}

BLOCKTYPE perm_64_64_final(BLOCKTYPE M, int *FINAL_PERMTABLE){
    BLOCKTYPE FP = 0;
    for (int i=0; i<64; i++){
        FP = FP | ((M>>( (BLOCKTYPE)FINAL_PERMTABLE[i] - 1)) & (1)) << i;
    }
    return FP;
}

BLOCKTYPE perm_32_48(HALFBLOCKTYPE R, uint64_t *EXPAND_BOX){
    BLOCKTYPE EX_M = 0;
    for (int i=0; i<48; i++){
        EX_M = EX_M | ((R>>(EXPAND_BOX[i] - 1)) & (1)) << i;
    }
    return EX_M;
}

HALFBLOCKTYPE perm_32_32(HALFBLOCKTYPE substitutedMessage, uint32_t *PBOX){
    HALFBLOCKTYPE P_M = 0;
    for (int i=0; i<32; i++){
        P_M = P_M | ( (substitutedMessage >> (PBOX[i] - 1)) & (1)) << i;
    }
    return P_M;
}

HALFBLOCKTYPE unitSubstitute(HALFBLOCKTYPE input_6, uint64_t sbox[][16]){
    //printf("[DEBUG: unitSubstitute] Entered unitSubstitute function\n");
    //printf("[DEBUG: unitSubstitute] Value of 6 bit input is 0x%x\n", input_6);
    HALFBLOCKTYPE output_4;
    int row =  (2* ((input_6>>5) & 1)) + (input_6 & 1);
    //printf("[DEBUG: unitSubstitute] Row value is %d\n", row);
    int column = (input_6 >> 1) & 0xf;
    //printf("[DEBUG: unitSubstitute] Column value is %d\n", column);
    output_4 = (HALFBLOCKTYPE) sbox[row][column];
    //printf("[DEBUG: unitSubstitute] Value of calculated 4-bit output is 0x%x\n", output_4);
    return output_4;
}

HALFBLOCKTYPE substitute(BLOCKTYPE exp_message_48){
    HALFBLOCKTYPE substituted_message = 0;
    HALFBLOCKTYPE intermediate_input_6 = 0;
    HALFBLOCKTYPE intermediate_output_4 = 0;

    //printf("[DEBUG: substitute] Starting sbox substitution of 6 bit units\n");
    // Running S1 box
    intermediate_input_6 = (exp_message_48 & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_1);
    substituted_message = substituted_message | intermediate_output_4;

    // Running S2 box
    intermediate_input_6 = ((exp_message_48 >> 6) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_2);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S3 box
    intermediate_input_6 = ((exp_message_48 >> 12) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_3);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S4 box
    intermediate_input_6 = ((exp_message_48 >> 18) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_4);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S5 box
    intermediate_input_6 = ((exp_message_48 >> 24) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_5);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S6 box
    intermediate_input_6 = ((exp_message_48 >> 30) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_6);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S7 box
    intermediate_input_6 = ((exp_message_48 >> 36) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_7);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S8 box
    intermediate_input_6 = ((exp_message_48 >> 42) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_8);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    //printf("[DEBUG: substitute] Finished sbox substitution of 6 bit units\n");

    return substituted_message;
}

HALFBLOCKTYPE fiestel(HALFBLOCKTYPE righthalf, int i){
    // Expansion permutation 32 -> 48
    BLOCKTYPE exp_message = perm_32_48(righthalf, expand_box);

    // XOR with subkey[i]
    exp_message = exp_message ^ (getSubKey(i));

    // 8 S-box substitution 48 -> 32

    HALFBLOCKTYPE substituted_message = substitute(exp_message);


    // P-box permutation 32 -> 32 (return)
    HALFBLOCKTYPE fiestelOutput = perm_32_32(substituted_message, Pbox);

    return fiestelOutput;
}

// Encrypt one block. This is where the main computation takes place. It takes
// one 64-bit block as input, and returns the encrypted 64-bit block. The 
// subkeys needed by the Feistel Network is given by the function getSubKey(i).
BLOCKTYPE des_enc(BLOCKTYPE v){
   // TODO
   HALFBLOCKTYPE L;
   HALFBLOCKTYPE R;
   HALFBLOCKTYPE temp_R = 0;

   // Initial permutation on the Plaintext
   BLOCKTYPE IP = perm_64_64(v, init_perm);

   /*
   if (debug) {
       // print v in binary
       printf("[DEBUG] Printing Message Block\n");
       for(int i=63; i>=0; i--){
           printf("%ld [%d], ", (v>>i)&1, i+1); // Indexed from 1-64
       }
       printf("\n");
       // print IP
       printf("[DEBUG] Printing Message after Initial Permutation\n");
       for (int i=63; i>=0; i--){
           printf("%ld [%ld], ", (IP>>i)&1, init_perm[i]);  // Indexed from 1-64
       }
       printf("\n");
   }
   */

   L = (HALFBLOCKTYPE)(IP >> 32);
   R = (HALFBLOCKTYPE)(IP);

   for (int i = 0; i < 16; i++){
        temp_R = R;
        R = L ^ (fiestel(R, i));
        L = temp_R;
   }

    // Final switching between L and R
    temp_R = R;
    R = L;
    L = temp_R;

   // Final permutation on L and R
   BLOCKTYPE FP = ( ((BLOCKTYPE)L) << 32 ) | (BLOCKTYPE)R;
   FP = perm_64_64_final(FP, final_perm);

   return FP;
}

// Encrypt the blocks in ECB mode. The blocks have already been padded 
// by the input routine. The output is an encrypted list of blocks.
BLOCKLIST des_enc_ECB(BLOCKLIST msg) {
    assert(msg != NULL);
    BLOCKLIST encryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    while(start!=NULL)
    {
      BLOCKTYPE block = start->block;
      BLOCKTYPE encryptedblock = des_enc(block);
      if(debug){
          printf("Encrypted Block[%d] 0x%lx\n", i, encryptedblock);
          i++;
      }
      addBlockLast(encryptedBlockList, encryptedblock, start->size);
      start = start->next;
    }
   return encryptedBlockList;
}

// Same as des_enc_ECB, but encrypt the blocks in Counter mode.
// SEE: https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#Counter_(CTR)
// Start the counter at 0.
BLOCKLIST des_enc_CTR(BLOCKLIST msg) {
    assert(msg != NULL);
    // TODO
    // Should call des_enc in here repeatedly
    BLOCKLIST encryptedBlockList = newBlockList();
    BLOCKNODE start = msg->first;
    int ctr = 0;
    KEYTYPE original_key = key;
    int i = 0; // Used for debugging only
    while(start!=NULL) {
      key = original_key ^ ctr;
      BLOCKTYPE block = start->block;
      BLOCKTYPE encryptedblock = des_enc(block);
      if(debug) {
          printf("Encrypted Block[%d] 0x%lx\n", i, encryptedblock);
          i++;
      }
      addBlockLast(encryptedBlockList, encryptedblock, 8);
      start = start->next;
      key = original_key;
      ctr += 1;
    }
    return encryptedBlockList;
}

/////////////////////////////////////////////////////////////////////////////
// Decryption
/////////////////////////////////////////////////////////////////////////////
// Decrypt one block.
BLOCKTYPE des_dec(BLOCKTYPE v){
    // TODO
    HALFBLOCKTYPE L;
    HALFBLOCKTYPE R;
    HALFBLOCKTYPE temp_R = 0;

    // Initial permutation on the Plaintext
    BLOCKTYPE IP = perm_64_64(v, init_perm);

    /*
    if (debug) {
        // print v in binary
        printf("[DEBUG] Printing Message Block\n");
        for(int i=63; i>=0; i--){
            printf("%ld [%d], ", (v>>i)&1, i+1); // Indexed from 1-64
        }
        printf("\n");
        // print IP
        printf("[DEBUG] Printing Message after Initial Permutation\n");
        for (int i=63; i>=0; i--){
            printf("%ld [%ld], ", (IP>>i)&1, init_perm[i]);  // Indexed from 1-64
        }
        printf("\n");
    }
    */

    L = (HALFBLOCKTYPE)(IP >> 32);
    R = (HALFBLOCKTYPE)(IP);

    for (int i = 15; i >= 0; i--){
        temp_R = R;
        R = L ^ (fiestel(R, i));
        L = temp_R;
    }

    // Final switching between L and R
    temp_R = R;
    R = L;
    L = temp_R;

    // Final permutation on L and R
    BLOCKTYPE FP = ( ((BLOCKTYPE)L) << 32 ) | (BLOCKTYPE)R;
    FP = perm_64_64_final(FP, final_perm);

    return FP;
}

// Decrypt the blocks in ECB mode. The input is a list of encrypted blocks,
// the output a list of plaintext blocks.
BLOCKLIST des_dec_ECB(BLOCKLIST msg) {
    assert(msg != NULL);
    // Should call des_dec in here repeatedly
    BLOCKLIST decryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    while(start!=NULL)
    {
        BLOCKTYPE block = start->block;
        BLOCKTYPE decryptedblock = des_dec(block);
        if(debug){
            printf("Decrypted Block[%d] 0x%lx\n", i, decryptedblock);
            i++;
        }
        addBlockLast(decryptedBlockList, decryptedblock, start->size);
        start = start->next;
    }
    return decryptedBlockList;
}

// Decrypt the blocks in Counter mode
BLOCKLIST des_dec_CTR(BLOCKLIST msg) {
    assert(msg != NULL);
    // Should call des_dec in here repeatedly
    BLOCKLIST decryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    int ctr = 0;
    KEYTYPE original_key = key;
    while(start!=NULL)
    {
        key = original_key ^ ctr;
        BLOCKTYPE block = start->block;
        BLOCKTYPE decryptedblock = des_dec(block);
        if(debug){
            printf("Decrypted Block[%d] 0x%lx\n", i, decryptedblock);
            i++;
        }
        addBlockLast(decryptedBlockList, decryptedblock, start->size);
        start = start->next;
        key = original_key;
        ctr += 1;
    }
    return decryptedBlockList;

}

/////////////////////////////////////////////////////////////////////////////
// Main routine
/////////////////////////////////////////////////////////////////////////////

void encrypt (int argc, char **argv) {
      FILE *msg_fp = fopen("message.txt", "r");
      BLOCKLIST msg = read_cleartext_message(msg_fp);
      fclose(msg_fp);

      BLOCKLIST encrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         encrypted_message = des_enc_ECB(msg);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         encrypted_message = des_enc_CTR(msg);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "wb");
      write_encrypted_message(encrypted_msg_fp, encrypted_message);
      fclose(encrypted_msg_fp);
}

void decrypt (int argc, char **argv) {
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "rb");
      assert(encrypted_msg_fp != NULL);
      BLOCKLIST encrypted_message = read_encrypted_file(encrypted_msg_fp);
      fclose(encrypted_msg_fp);

      assert(encrypted_message != NULL);
      if(debug) {
          printf("[DEBUG: decrypt] Printing encrypted message block from encrypted binary file\n");
          print_blocklist(encrypted_message);
      }

      BLOCKLIST decrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         decrypted_message = des_dec_ECB(encrypted_message);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         decrypted_message = des_dec_CTR(encrypted_message);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };

      if (debug){
          print_blocklist(decrypted_message);
      }

      FILE *decrypted_msg_fp = fopen("decrypted_message.txt", "w");
      write_decrypted_message(decrypted_msg_fp, decrypted_message);
      fclose(decrypted_msg_fp);
}

int main(int argc, char **argv){
   FILE *key_fp = fopen("key.txt","r");
   key = read_key(key_fp);
   //generateSubKeys(key);              // This does nothing right now.
   fclose(key_fp);

   if (argc != 3) {
     printf("Two arguments expected.\n");
     exit(-1);
   }

   if (strncmp(argv[1],"-enc",4) == 0) {
      encrypt(argc, argv);
   } else if (strncmp(argv[1],"-dec",4) == 0) {
      decrypt(argc, argv);
   } else {
     printf("First argument should be -enc or -dec\n"); 
   }
   return 0;
}#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

 /*
 * des takes two arguments on the command line:
 *    des -enc -ecb      -- encrypt in ECB mode
 *    des -enc -ctr      -- encrypt in CTR mode
 *    des -dec -ecb      -- decrypt in ECB mode
 *    des -dec -ctr      -- decrypt in CTR mode
 * des also reads some hardcoded files:
 *    message.txt            -- the ASCII text message to be encrypted,
 *                              read by "des -enc"
 *    encrypted_msg.bin      -- the encrypted message, a binary file,
 *                              written by "des -enc"
 *    decrypted_message.txt  -- the decrypted ASCII text message
 *    key.txt                -- just contains the key, on a line by itself, as an ASCII 
 *                              hex number, such as: 0x34FA879B
*/


uint64_t
u8tou64(uint8_t const u8[static 8]){
  uint64_t u64;
  memcpy(&u64, u8, sizeof u64);
  return u64;
}

unsigned createMask(unsigned a, unsigned b)
{
   unsigned r = 0;
   for (unsigned i=a; i<=b; i++)
       r |= 1 << i;

   return r;
}

/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////
typedef uint64_t KEYTYPE;
typedef uint32_t SUBKEYTYPE;
typedef uint64_t BLOCKTYPE;
typedef uint32_t HALFBLOCKTYPE;

struct BLOCK {
    BLOCKTYPE block;        // the block read
    int size;               // number of "real" bytes in the block, should be 8, unless it's the last block
    struct BLOCK *next;     // pointer to the next block
};

struct BLOCKHEAD {
    struct BLOCK *first;   // first block
    struct BLOCK *last;    // last block
};

typedef struct BLOCK* BLOCKNODE;
typedef struct BLOCKHEAD* BLOCKLIST;


BLOCKLIST newBlockList() {
   BLOCKLIST blocks = (BLOCKLIST)malloc(sizeof(struct BLOCKHEAD));
   blocks->first = NULL;
   blocks->last = NULL;
   return blocks;
}

BLOCKNODE newBlock(uint64_t block, int size){
  BLOCKNODE new_node = (BLOCKNODE) malloc(sizeof(struct BLOCK));
  new_node->block = block;
  new_node->size = size;
  new_node->next = NULL;
  return new_node;
}

void addBlockLast(BLOCKLIST list, BLOCKTYPE block, int size) {
   BLOCKNODE currBlock = newBlock(block, size);
   if (list->first == NULL) {
      list->first = currBlock;
      list->last = currBlock;
    } else {
      list->last->next = currBlock;
      list->last = currBlock;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Debugging
/////////////////////////////////////////////////////////////////////////////
int debug = 1;

void print_blocklist(BLOCKLIST head) {
   BLOCKNODE curr = head->first;
   int x = 0;
   assert(curr != NULL);

   while (curr != NULL) {
           printf("\tBLOCK [%d]: 0x%lx, size=%d\n", x, curr->block,curr->size);
      x++;
      curr = curr->next;
   }
}

void print_block(struct BLOCK* head) {
   
   printf("\tBLOCK : 0x%lx, size=%d\n", head->block,head->size);
      
}

/////////////////////////////////////////////////////////////////////////////
// Initial and final permutation
/////////////////////////////////////////////////////////////////////////////
uint64_t init_perm[] = {
   58,50,42,34,26,18,10,2,
   60,52,44,36,28,20,12,4,
   62,54,46,38,30,22,14,6,
   64,56,48,40,32,24,16,8,
   57,49,41,33,25,17,9,1,
   59,51,43,35,27,19,11,3,
   61,53,45,37,29,21,13,5,
   63,55,47,39,31,23,15,7
};

int final_perm[] = {
   40,8,48,16,56,24,64,32,
   39,7,47,15,55,23,63,31,
   38,6,46,14,54,22,62,30,
   37,5,45,13,53,21,61,29,
   36,4,44,12,52,20,60,28,
   35,3,43,11,51,19,59,27,
   34,2,42,10,50,18,58,26,
   33,1,41,9, 49,17,57,25
};

/////////////////////////////////////////////////////////////////////////////
// Subkey generation
/////////////////////////////////////////////////////////////////////////////

//SUBKEYTYPE subkeys[16];
KEYTYPE key = 0;    // Global variable key

// This function returns the i:th subkey, 48 bits long. To simplify the assignment 
// you can use a trivial implementation: just take the input key and xor it with i.
uint64_t getSubKey(int i) {
   // TODO: return the first 48 bits of the 56 bit DES key, xor:ed with i.
   return ((key ^ i) & 0xffffffffffff);
   //return subkeys[i];
}

// For extra credit, write the real DES key expansion routine!
//SUBKEYTYPE* generateSubKeys(KEYTYPE key) {
//   // TODO for extra credit
//  for( int i = 0; i<16; i++){
//    unsigned r = createMask(0,47);
//    unsigned result = r & (key^i);
//    subkeys[i] = result;
//  }
//  return subkeys;
//}

/////////////////////////////////////////////////////////////////////////////
// P-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t expand_box[] = {
   32,1,2,3,4,5,4,5,6,7,8,9,
   8,9,10,11,12,13,12,13,14,15,16,17,
   16,17,18,19,20,21,20,21,22,23,24,25,
   24,25,26,27,28,29,28,29,30,31,32,1
};

uint32_t Pbox[] = 
{
   16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,
   2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25,
};      

/////////////////////////////////////////////////////////////////////////////
// S-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t sbox_1[4][16] = {
   {14,  4, 13,  1,  2, 15, 11,  8,  3, 10 , 6, 12,  5,  9,  0,  7},
   { 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8},
   { 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0},
   {15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13}};

uint64_t sbox_2[4][16] = {
   {15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5 ,10},
   { 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5},
   { 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15},
   {13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9}};

uint64_t sbox_3[4][16] = {
   {10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8},
   {13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1},
   {13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7},
   { 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12}};


uint64_t sbox_4[4][16] = {
   { 7, 13, 14,  3,  0 , 6,  9, 10,  1 , 2 , 8,  5, 11, 12,  4 ,15},
   {13,  8, 11,  5,  6, 15,  0,  3,  4 , 7 , 2, 12,  1, 10, 14,  9},
   {10,  6,  9 , 0, 12, 11,  7, 13 ,15 , 1 , 3, 14 , 5 , 2,  8,  4},
   { 3, 15,  0,  6, 10,  1, 13,  8,  9 , 4 , 5, 11 ,12 , 7,  2, 14}};
 
 
uint64_t sbox_5[4][16] = {
   { 2, 12,  4,  1 , 7 ,10, 11,  6 , 8 , 5 , 3, 15, 13,  0, 14,  9},
   {14, 11 , 2 ,12 , 4,  7, 13 , 1 , 5 , 0, 15, 10,  3,  9,  8,  6},
   { 4,  2 , 1, 11, 10, 13,  7 , 8 ,15 , 9, 12,  5,  6 , 3,  0, 14},
   {11,  8 ,12 , 7 , 1, 14 , 2 ,13 , 6 ,15,  0,  9, 10 , 4,  5,  3}};


uint64_t sbox_6[4][16] = {
   {12,  1, 10, 15 , 9 , 2 , 6 , 8 , 0, 13 , 3 , 4 ,14 , 7  ,5 ,11},
   {10, 15,  4,  2,  7, 12 , 9 , 5 , 6,  1 ,13 ,14 , 0 ,11 , 3 , 8},
   { 9, 14 ,15,  5,  2,  8 ,12 , 3 , 7 , 0,  4 ,10  ,1 ,13 ,11 , 6},
   { 4,  3,  2, 12 , 9,  5 ,15 ,10, 11 ,14,  1 , 7  ,6 , 0 , 8 ,13}};
 

uint64_t sbox_7[4][16] = {
   { 4, 11,  2, 14, 15,  0 , 8, 13, 3,  12 , 9 , 7,  6 ,10 , 6 , 1},
   {13,  0, 11,  7,  4 , 9,  1, 10, 14 , 3 , 5, 12,  2, 15 , 8 , 6},
   { 1 , 4, 11, 13, 12,  3,  7, 14, 10, 15 , 6,  8,  0,  5 , 9 , 2},
   { 6, 11, 13 , 8,  1 , 4, 10,  7,  9 , 5 , 0, 15, 14,  2 , 3 ,12}};
 
uint64_t sbox_8[4][16] = {
   {13,  2,  8,  4,  6 ,15 ,11,  1, 10,  9 , 3, 14,  5,  0, 12,  7},
   { 1, 15, 13,  8 ,10 , 3  ,7 , 4, 12 , 5,  6 ,11,  0 ,14 , 9 , 2},
   { 7, 11,  4,  1,  9, 12, 14 , 2,  0  ,6, 10 ,13 ,15 , 3  ,5  ,8},
   { 2,  1, 14 , 7 , 4, 10,  8, 13, 15, 12,  9,  0 , 3,  5 , 6 ,11}};

/////////////////////////////////////////////////////////////////////////////
// I/O
/////////////////////////////////////////////////////////////////////////////

// Pad the list of blocks, so that every block is 64 bits, even if the
// file isn't a perfect multiple of 8 bytes long. In the input list of blocks,
// the last block may have "size" < 8. In this case, it needs to be padded. See 
// the slides for how to do this (the last byte of the last block 
// should contain the number if real bytes in the block, add an extra block if
// the file is an exact multiple of 8 bytes long.) The returned
// list of blocks will always have the "size"-field=8.
// Example:
//    1) The last block is 5 bytes long: [10,20,30,40,50]. We pad it with 2 bytes,
//       and set the length to 5: [10,20,30,40,50,0,0,5]. This means that the 
//       first 5 bytes of the block are "real", the last 3 should be discarded.
//    2) The last block is 8 bytes long: [10,20,30,40,50,60,70,80]. We keep this 
//       block as is, and add a new final block: [0,0,0,0,0,0,0,0]. When we decrypt,
//       the entire last block will be discarded since the last byte is 0
BLOCKLIST pad_last_block(BLOCKLIST blocks) {
    // TODO
  struct BLOCK* lastblock = blocks->last;
  if(debug)
      printf("Printing blocklist before padding\n");
      print_blocklist(blocks);

  uint8_t *p = (uint8_t *)&lastblock->block;
  //if you need a copy
  uint8_t result[8];
  for(int i = 0; i < 8; i++) {
      result[i] = p[i];
      if(debug)
      {
        printf("%x ", result[i]);
        printf("\n");
      }
  }
  int blocksize = lastblock->size;
  if( blocksize == 8)
  {
    // create a new empty block
    BLOCKTYPE emptyBlock = 0;
    BLOCKNODE paddedblock = newBlock(emptyBlock, 8);
    lastblock->next = paddedblock;
    lastblock = paddedblock;
    
  }
  else {
    result[7] = blocksize;
  
    uint64_t paddedblock = u8tou64(result);
    if(debug){
      printf("%lx\n", paddedblock);
    }
    lastblock->block = paddedblock;
    if(debug) {
      p = (uint8_t *)&paddedblock;
      //if you need a copy
       result[8];
      for(int i = 0; i < 8; i++) {
          result[i] = p[i];
          if (debug) {
            printf("%x ", result[i]);
            printf("\n");
          }
      }
    }

  }

  if(debug)
      printf("Printing blocklist after padding\n");
      print_blocklist(blocks);

  return blocks;
}

// Reads the message to be encrypted, an ASCII text file, and returns a linked list 
// of BLOCKs, each representing a 64 bit block. In other words, read the first 8 characters
// from the input file, and convert them (just a C cast) to 64 bits; this is your first block.
// Continue to the end of the file.

BLOCKLIST read_cleartext_message(FILE *msg_fp) {
   BLOCKLIST blocks = newBlockList();
   unsigned char byte = 0;
   int retVal = fscanf(msg_fp, "%c", &byte);
   BLOCKTYPE block = 0;
   int counter = 0;
   while (retVal > 0) {
      if (counter == 8) {
         addBlockLast(blocks, block, counter);
         counter = 0;
         block = 0;
      }
      block = (block << 8) | byte;
      counter++;
      retVal = fscanf(msg_fp, "%c", &byte);
   }
   if (counter > 0) {
      addBlockLast(blocks, block, counter);
   }
   blocks = pad_last_block(blocks);
   if (debug) {
      printf("Cleartext Blocks Read\n");
      print_blocklist(blocks);
   }
   return blocks;
}


// Reads the encrypted message, and returns a linked list of blocks, each 64 bits. 
// Note that, because of the padding that was done by the encryption, the length of 
// this file should always be a multiple of 8 bytes. The output is a linked list of
// 64-bit blocks.
BLOCKLIST read_encrypted_file(FILE *msg_fp) {
    BLOCKLIST blocks = newBlockList();
    uint64_t block;
    while(fread(&block, sizeof(block), 1, msg_fp) > 0){
      addBlockLast(blocks, block, 8);
      block = 0;
    }
    return blocks;
}

// Reads 56-bit key into a 64 bit unsigned int. We will ignore the most significant byte,
// i.e. we'll assume that the top 8 bits are all 0. In real DES, these are used to check 
// that the key hasn't been corrupted in transit. The key file is ASCII, consisting of
// exactly one line. That line has a single hex number on it, the key, such as 0x08AB674D9.
KEYTYPE read_key(FILE *key_fp) {
   char buff[17];
   fgets(buff, 17, key_fp);
   KEYTYPE key = strtol(buff, 0, 16);
   return key;
}

// Write the encrypted blocks to file. The encrypted file is in binary, i.e., you can
// just write each 64-bit block directly to the file, without any conversion.
void write_encrypted_message(FILE *msg_fp, BLOCKLIST msg) {
    assert(msg != NULL);
    BLOCKNODE curr = msg->first;
    char buffer[8];
    while(curr != NULL){
        memcpy(&buffer,&(curr->block),curr->size);
        fwrite(buffer, sizeof(char), 8, msg_fp);
        curr = curr->next;
    }
    return;
}

// Write the encrypted blocks to file. This is called by the decryption routine.
// The output file is a plain ASCII file, containing the decrypted text message.
void write_decrypted_message(FILE *msg_fp, BLOCKLIST msg) {
   assert(msg != NULL);
   BLOCKNODE cur = msg->first;
   int size;
   unsigned int charac;
   while(cur != NULL){
      for(int i=8; i>=1; i--){
         charac = (unsigned int) (((cur->block) >> ((i-1)*8)) & 0xFF);
         fprintf(msg_fp,"%c",charac);
      }
      cur = cur->next;
   }
}


/////////////////////////////////////////////////////////////////////////////
// Encryption
/////////////////////////////////////////////////////////////////////////////

BLOCKTYPE perm_64_64(BLOCKTYPE M, uint64_t *PERM_TABLE){
    BLOCKTYPE M_IP = 0;
    for (int i=0; i<64; i++){
        M_IP = M_IP | ((M>>(PERM_TABLE[i] - 1)) & (1)) << i;
    }
    return M_IP;
}

BLOCKTYPE perm_64_64_final(BLOCKTYPE M, int *FINAL_PERMTABLE){
    BLOCKTYPE FP = 0;
    for (int i=0; i<64; i++){
        FP = FP | ((M>>( (BLOCKTYPE)FINAL_PERMTABLE[i] - 1)) & (1)) << i;
    }
    return FP;
}

BLOCKTYPE perm_32_48(HALFBLOCKTYPE R, uint64_t *EXPAND_BOX){
    BLOCKTYPE EX_M = 0;
    for (int i=0; i<48; i++){
        EX_M = EX_M | ((R>>(EXPAND_BOX[i] - 1)) & (1)) << i;
    }
    return EX_M;
}

HALFBLOCKTYPE perm_32_32(HALFBLOCKTYPE substitutedMessage, uint32_t *PBOX){
    HALFBLOCKTYPE P_M = 0;
    for (int i=0; i<32; i++){
        P_M = P_M | ( (substitutedMessage >> (PBOX[i] - 1)) & (1)) << i;
    }
    return P_M;
}

HALFBLOCKTYPE unitSubstitute(HALFBLOCKTYPE input_6, uint64_t sbox[][16]){
    //printf("[DEBUG: unitSubstitute] Entered unitSubstitute function\n");
    //printf("[DEBUG: unitSubstitute] Value of 6 bit input is 0x%x\n", input_6);
    HALFBLOCKTYPE output_4;
    int row =  (2* ((input_6>>5) & 1)) + (input_6 & 1);
    //printf("[DEBUG: unitSubstitute] Row value is %d\n", row);
    int column = (input_6 >> 1) & 0xf;
    //printf("[DEBUG: unitSubstitute] Column value is %d\n", column);
    output_4 = (HALFBLOCKTYPE) sbox[row][column];
    //printf("[DEBUG: unitSubstitute] Value of calculated 4-bit output is 0x%x\n", output_4);
    return output_4;
}

HALFBLOCKTYPE substitute(BLOCKTYPE exp_message_48){
    HALFBLOCKTYPE substituted_message = 0;
    HALFBLOCKTYPE intermediate_input_6 = 0;
    HALFBLOCKTYPE intermediate_output_4 = 0;

    //printf("[DEBUG: substitute] Starting sbox substitution of 6 bit units\n");
    // Running S1 box
    intermediate_input_6 = (exp_message_48 & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_1);
    substituted_message = substituted_message | intermediate_output_4;

    // Running S2 box
    intermediate_input_6 = ((exp_message_48 >> 6) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_2);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S3 box
    intermediate_input_6 = ((exp_message_48 >> 12) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_3);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S4 box
    intermediate_input_6 = ((exp_message_48 >> 18) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_4);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S5 box
    intermediate_input_6 = ((exp_message_48 >> 24) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_5);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S6 box
    intermediate_input_6 = ((exp_message_48 >> 30) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_6);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S7 box
    intermediate_input_6 = ((exp_message_48 >> 36) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_7);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S8 box
    intermediate_input_6 = ((exp_message_48 >> 42) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_8);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    //printf("[DEBUG: substitute] Finished sbox substitution of 6 bit units\n");

    return substituted_message;
}

HALFBLOCKTYPE fiestel(HALFBLOCKTYPE righthalf, int i){
    // Expansion permutation 32 -> 48
    BLOCKTYPE exp_message = perm_32_48(righthalf, expand_box);

    // XOR with subkey[i]
    exp_message = exp_message ^ (getSubKey(i));

    // 8 S-box substitution 48 -> 32

    HALFBLOCKTYPE substituted_message = substitute(exp_message);


    // P-box permutation 32 -> 32 (return)
    HALFBLOCKTYPE fiestelOutput = perm_32_32(substituted_message, Pbox);

    return fiestelOutput;
}

// Encrypt one block. This is where the main computation takes place. It takes
// one 64-bit block as input, and returns the encrypted 64-bit block. The 
// subkeys needed by the Feistel Network is given by the function getSubKey(i).
BLOCKTYPE des_enc(BLOCKTYPE v){
   // TODO
   HALFBLOCKTYPE L;
   HALFBLOCKTYPE R;
   HALFBLOCKTYPE temp_R = 0;

   // Initial permutation on the Plaintext
   BLOCKTYPE IP = perm_64_64(v, init_perm);

   /*
   if (debug) {
       // print v in binary
       printf("[DEBUG] Printing Message Block\n");
       for(int i=63; i>=0; i--){
           printf("%ld [%d], ", (v>>i)&1, i+1); // Indexed from 1-64
       }
       printf("\n");
       // print IP
       printf("[DEBUG] Printing Message after Initial Permutation\n");
       for (int i=63; i>=0; i--){
           printf("%ld [%ld], ", (IP>>i)&1, init_perm[i]);  // Indexed from 1-64
       }
       printf("\n");
   }
   */

   L = (HALFBLOCKTYPE)(IP >> 32);
   R = (HALFBLOCKTYPE)(IP);

   for (int i = 0; i < 16; i++){
        temp_R = R;
        R = L ^ (fiestel(R, i));
        L = temp_R;
   }

    // Final switching between L and R
    temp_R = R;
    R = L;
    L = temp_R;

   // Final permutation on L and R
   BLOCKTYPE FP = ( ((BLOCKTYPE)L) << 32 ) | (BLOCKTYPE)R;
   FP = perm_64_64_final(FP, final_perm);

   return FP;
}

// Encrypt the blocks in ECB mode. The blocks have already been padded 
// by the input routine. The output is an encrypted list of blocks.
BLOCKLIST des_enc_ECB(BLOCKLIST msg) {
    assert(msg != NULL);
    BLOCKLIST encryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    while(start!=NULL)
    {
      BLOCKTYPE block = start->block;
      BLOCKTYPE encryptedblock = des_enc(block);
      if(debug){
          printf("Encrypted Block[%d] 0x%lx\n", i, encryptedblock);
          i++;
      }
      addBlockLast(encryptedBlockList, encryptedblock, start->size);
      start = start->next;
    }
   return encryptedBlockList;
}

// Same as des_enc_ECB, but encrypt the blocks in Counter mode.
// SEE: https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#Counter_(CTR)
// Start the counter at 0.
BLOCKLIST des_enc_CTR(BLOCKLIST msg) {
    assert(msg != NULL);
    // TODO
    // Should call des_enc in here repeatedly
    BLOCKLIST encryptedBlockList = newBlockList();
    BLOCKNODE start = msg->first;
    int ctr = 0;
    KEYTYPE original_key = key;
    int i = 0; // Used for debugging only
    while(start!=NULL) {
      key = original_key ^ ctr;
      BLOCKTYPE block = start->block;
      BLOCKTYPE encryptedblock = des_enc(block);
      if(debug) {
          printf("Encrypted Block[%d] 0x%lx\n", i, encryptedblock);
          i++;
      }
      addBlockLast(encryptedBlockList, encryptedblock, 8);
      start = start->next;
      key = original_key;
      ctr += 1;
    }
    return encryptedBlockList;
}

/////////////////////////////////////////////////////////////////////////////
// Decryption
/////////////////////////////////////////////////////////////////////////////
// Decrypt one block.
BLOCKTYPE des_dec(BLOCKTYPE v){
    // TODO
    HALFBLOCKTYPE L;
    HALFBLOCKTYPE R;
    HALFBLOCKTYPE temp_R = 0;

    // Initial permutation on the Plaintext
    BLOCKTYPE IP = perm_64_64(v, init_perm);

    /*
    if (debug) {
        // print v in binary
        printf("[DEBUG] Printing Message Block\n");
        for(int i=63; i>=0; i--){
            printf("%ld [%d], ", (v>>i)&1, i+1); // Indexed from 1-64
        }
        printf("\n");
        // print IP
        printf("[DEBUG] Printing Message after Initial Permutation\n");
        for (int i=63; i>=0; i--){
            printf("%ld [%ld], ", (IP>>i)&1, init_perm[i]);  // Indexed from 1-64
        }
        printf("\n");
    }
    */

    L = (HALFBLOCKTYPE)(IP >> 32);
    R = (HALFBLOCKTYPE)(IP);

    for (int i = 15; i >= 0; i--){
        temp_R = R;
        R = L ^ (fiestel(R, i));
        L = temp_R;
    }

    // Final switching between L and R
    temp_R = R;
    R = L;
    L = temp_R;

    // Final permutation on L and R
    BLOCKTYPE FP = ( ((BLOCKTYPE)L) << 32 ) | (BLOCKTYPE)R;
    FP = perm_64_64_final(FP, final_perm);

    return FP;
}

// Decrypt the blocks in ECB mode. The input is a list of encrypted blocks,
// the output a list of plaintext blocks.
BLOCKLIST des_dec_ECB(BLOCKLIST msg) {
    assert(msg != NULL);
    // Should call des_dec in here repeatedly
    BLOCKLIST decryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    while(start!=NULL)
    {
        BLOCKTYPE block = start->block;
        BLOCKTYPE decryptedblock = des_dec(block);
        if(debug){
            printf("Decrypted Block[%d] 0x%lx\n", i, decryptedblock);
            i++;
        }
        addBlockLast(decryptedBlockList, decryptedblock, start->size);
        start = start->next;
    }
    return decryptedBlockList;
}

// Decrypt the blocks in Counter mode
BLOCKLIST des_dec_CTR(BLOCKLIST msg) {
    assert(msg != NULL);
    // Should call des_dec in here repeatedly
    BLOCKLIST decryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    int ctr = 0;
    KEYTYPE original_key = key;
    while(start!=NULL)
    {
        key = original_key ^ ctr;
        BLOCKTYPE block = start->block;
        BLOCKTYPE decryptedblock = des_dec(block);
        if(debug){
            printf("Decrypted Block[%d] 0x%lx\n", i, decryptedblock);
            i++;
        }
        addBlockLast(decryptedBlockList, decryptedblock, start->size);
        start = start->next;
        key = original_key;
        ctr += 1;
    }
    return decryptedBlockList;

}

/////////////////////////////////////////////////////////////////////////////
// Main routine
/////////////////////////////////////////////////////////////////////////////

void encrypt (int argc, char **argv) {
      FILE *msg_fp = fopen("message.txt", "r");
      BLOCKLIST msg = read_cleartext_message(msg_fp);
      fclose(msg_fp);

      BLOCKLIST encrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         encrypted_message = des_enc_ECB(msg);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         encrypted_message = des_enc_CTR(msg);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "wb");
      write_encrypted_message(encrypted_msg_fp, encrypted_message);
      fclose(encrypted_msg_fp);
}

void decrypt (int argc, char **argv) {
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "rb");
      assert(encrypted_msg_fp != NULL);
      BLOCKLIST encrypted_message = read_encrypted_file(encrypted_msg_fp);
      fclose(encrypted_msg_fp);

      assert(encrypted_message != NULL);
      if(debug) {
          printf("[DEBUG: decrypt] Printing encrypted message block from encrypted binary file\n");
          print_blocklist(encrypted_message);
      }

      BLOCKLIST decrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         decrypted_message = des_dec_ECB(encrypted_message);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         decrypted_message = des_dec_CTR(encrypted_message);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };

      if (debug){
          print_blocklist(decrypted_message);
      }

      FILE *decrypted_msg_fp = fopen("decrypted_message.txt", "w");
      write_decrypted_message(decrypted_msg_fp, decrypted_message);
      fclose(decrypted_msg_fp);
}

int main(int argc, char **argv){
   FILE *key_fp = fopen("key.txt","r");
   key = read_key(key_fp);
   //generateSubKeys(key);              // This does nothing right now.
   fclose(key_fp);

   if (argc != 3) {
     printf("Two arguments expected.\n");
     exit(-1);
   }

   if (strncmp(argv[1],"-enc",4) == 0) {
      encrypt(argc, argv);
   } else if (strncmp(argv[1],"-dec",4) == 0) {
      decrypt(argc, argv);
   } else {
     printf("First argument should be -enc or -dec\n"); 
   }
   return 0;
}#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

 /*
 * des takes two arguments on the command line:
 *    des -enc -ecb      -- encrypt in ECB mode
 *    des -enc -ctr      -- encrypt in CTR mode
 *    des -dec -ecb      -- decrypt in ECB mode
 *    des -dec -ctr      -- decrypt in CTR mode
 * des also reads some hardcoded files:
 *    message.txt            -- the ASCII text message to be encrypted,
 *                              read by "des -enc"
 *    encrypted_msg.bin      -- the encrypted message, a binary file,
 *                              written by "des -enc"
 *    decrypted_message.txt  -- the decrypted ASCII text message
 *    key.txt                -- just contains the key, on a line by itself, as an ASCII 
 *                              hex number, such as: 0x34FA879B
*/


uint64_t
u8tou64(uint8_t const u8[static 8]){
  uint64_t u64;
  memcpy(&u64, u8, sizeof u64);
  return u64;
}

unsigned createMask(unsigned a, unsigned b)
{
   unsigned r = 0;
   for (unsigned i=a; i<=b; i++)
       r |= 1 << i;

   return r;
}

/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////
typedef uint64_t KEYTYPE;
typedef uint32_t SUBKEYTYPE;
typedef uint64_t BLOCKTYPE;
typedef uint32_t HALFBLOCKTYPE;

struct BLOCK {
    BLOCKTYPE block;        // the block read
    int size;               // number of "real" bytes in the block, should be 8, unless it's the last block
    struct BLOCK *next;     // pointer to the next block
};

struct BLOCKHEAD {
    struct BLOCK *first;   // first block
    struct BLOCK *last;    // last block
};

typedef struct BLOCK* BLOCKNODE;
typedef struct BLOCKHEAD* BLOCKLIST;


BLOCKLIST newBlockList() {
   BLOCKLIST blocks = (BLOCKLIST)malloc(sizeof(struct BLOCKHEAD));
   blocks->first = NULL;
   blocks->last = NULL;
   return blocks;
}

BLOCKNODE newBlock(uint64_t block, int size){
  BLOCKNODE new_node = (BLOCKNODE) malloc(sizeof(struct BLOCK));
  new_node->block = block;
  new_node->size = size;
  new_node->next = NULL;
  return new_node;
}

void addBlockLast(BLOCKLIST list, BLOCKTYPE block, int size) {
   BLOCKNODE currBlock = newBlock(block, size);
   if (list->first == NULL) {
      list->first = currBlock;
      list->last = currBlock;
    } else {
      list->last->next = currBlock;
      list->last = currBlock;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Debugging
/////////////////////////////////////////////////////////////////////////////
int debug = 1;

void print_blocklist(BLOCKLIST head) {
   BLOCKNODE curr = head->first;
   int x = 0;
   assert(curr != NULL);

   while (curr != NULL) {
           printf("\tBLOCK [%d]: 0x%lx, size=%d\n", x, curr->block,curr->size);
      x++;
      curr = curr->next;
   }
}

void print_block(struct BLOCK* head) {
   
   printf("\tBLOCK : 0x%lx, size=%d\n", head->block,head->size);
      
}

/////////////////////////////////////////////////////////////////////////////
// Initial and final permutation
/////////////////////////////////////////////////////////////////////////////
uint64_t init_perm[] = {
   58,50,42,34,26,18,10,2,
   60,52,44,36,28,20,12,4,
   62,54,46,38,30,22,14,6,
   64,56,48,40,32,24,16,8,
   57,49,41,33,25,17,9,1,
   59,51,43,35,27,19,11,3,
   61,53,45,37,29,21,13,5,
   63,55,47,39,31,23,15,7
};

int final_perm[] = {
   40,8,48,16,56,24,64,32,
   39,7,47,15,55,23,63,31,
   38,6,46,14,54,22,62,30,
   37,5,45,13,53,21,61,29,
   36,4,44,12,52,20,60,28,
   35,3,43,11,51,19,59,27,
   34,2,42,10,50,18,58,26,
   33,1,41,9, 49,17,57,25
};

/////////////////////////////////////////////////////////////////////////////
// Subkey generation
/////////////////////////////////////////////////////////////////////////////

//SUBKEYTYPE subkeys[16];
KEYTYPE key = 0;    // Global variable key

// This function returns the i:th subkey, 48 bits long. To simplify the assignment 
// you can use a trivial implementation: just take the input key and xor it with i.
uint64_t getSubKey(int i) {
   // TODO: return the first 48 bits of the 56 bit DES key, xor:ed with i.
   return ((key ^ i) & 0xffffffffffff);
   //return subkeys[i];
}

// For extra credit, write the real DES key expansion routine!
//SUBKEYTYPE* generateSubKeys(KEYTYPE key) {
//   // TODO for extra credit
//  for( int i = 0; i<16; i++){
//    unsigned r = createMask(0,47);
//    unsigned result = r & (key^i);
//    subkeys[i] = result;
//  }
//  return subkeys;
//}

/////////////////////////////////////////////////////////////////////////////
// P-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t expand_box[] = {
   32,1,2,3,4,5,4,5,6,7,8,9,
   8,9,10,11,12,13,12,13,14,15,16,17,
   16,17,18,19,20,21,20,21,22,23,24,25,
   24,25,26,27,28,29,28,29,30,31,32,1
};

uint32_t Pbox[] = 
{
   16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,
   2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25,
};      

/////////////////////////////////////////////////////////////////////////////
// S-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t sbox_1[4][16] = {
   {14,  4, 13,  1,  2, 15, 11,  8,  3, 10 , 6, 12,  5,  9,  0,  7},
   { 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8},
   { 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0},
   {15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13}};

uint64_t sbox_2[4][16] = {
   {15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5 ,10},
   { 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5},
   { 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15},
   {13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9}};

uint64_t sbox_3[4][16] = {
   {10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8},
   {13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1},
   {13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7},
   { 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12}};


uint64_t sbox_4[4][16] = {
   { 7, 13, 14,  3,  0 , 6,  9, 10,  1 , 2 , 8,  5, 11, 12,  4 ,15},
   {13,  8, 11,  5,  6, 15,  0,  3,  4 , 7 , 2, 12,  1, 10, 14,  9},
   {10,  6,  9 , 0, 12, 11,  7, 13 ,15 , 1 , 3, 14 , 5 , 2,  8,  4},
   { 3, 15,  0,  6, 10,  1, 13,  8,  9 , 4 , 5, 11 ,12 , 7,  2, 14}};
 
 
uint64_t sbox_5[4][16] = {
   { 2, 12,  4,  1 , 7 ,10, 11,  6 , 8 , 5 , 3, 15, 13,  0, 14,  9},
   {14, 11 , 2 ,12 , 4,  7, 13 , 1 , 5 , 0, 15, 10,  3,  9,  8,  6},
   { 4,  2 , 1, 11, 10, 13,  7 , 8 ,15 , 9, 12,  5,  6 , 3,  0, 14},
   {11,  8 ,12 , 7 , 1, 14 , 2 ,13 , 6 ,15,  0,  9, 10 , 4,  5,  3}};


uint64_t sbox_6[4][16] = {
   {12,  1, 10, 15 , 9 , 2 , 6 , 8 , 0, 13 , 3 , 4 ,14 , 7  ,5 ,11},
   {10, 15,  4,  2,  7, 12 , 9 , 5 , 6,  1 ,13 ,14 , 0 ,11 , 3 , 8},
   { 9, 14 ,15,  5,  2,  8 ,12 , 3 , 7 , 0,  4 ,10  ,1 ,13 ,11 , 6},
   { 4,  3,  2, 12 , 9,  5 ,15 ,10, 11 ,14,  1 , 7  ,6 , 0 , 8 ,13}};
 

uint64_t sbox_7[4][16] = {
   { 4, 11,  2, 14, 15,  0 , 8, 13, 3,  12 , 9 , 7,  6 ,10 , 6 , 1},
   {13,  0, 11,  7,  4 , 9,  1, 10, 14 , 3 , 5, 12,  2, 15 , 8 , 6},
   { 1 , 4, 11, 13, 12,  3,  7, 14, 10, 15 , 6,  8,  0,  5 , 9 , 2},
   { 6, 11, 13 , 8,  1 , 4, 10,  7,  9 , 5 , 0, 15, 14,  2 , 3 ,12}};
 
uint64_t sbox_8[4][16] = {
   {13,  2,  8,  4,  6 ,15 ,11,  1, 10,  9 , 3, 14,  5,  0, 12,  7},
   { 1, 15, 13,  8 ,10 , 3  ,7 , 4, 12 , 5,  6 ,11,  0 ,14 , 9 , 2},
   { 7, 11,  4,  1,  9, 12, 14 , 2,  0  ,6, 10 ,13 ,15 , 3  ,5  ,8},
   { 2,  1, 14 , 7 , 4, 10,  8, 13, 15, 12,  9,  0 , 3,  5 , 6 ,11}};

/////////////////////////////////////////////////////////////////////////////
// I/O
/////////////////////////////////////////////////////////////////////////////

// Pad the list of blocks, so that every block is 64 bits, even if the
// file isn't a perfect multiple of 8 bytes long. In the input list of blocks,
// the last block may have "size" < 8. In this case, it needs to be padded. See 
// the slides for how to do this (the last byte of the last block 
// should contain the number if real bytes in the block, add an extra block if
// the file is an exact multiple of 8 bytes long.) The returned
// list of blocks will always have the "size"-field=8.
// Example:
//    1) The last block is 5 bytes long: [10,20,30,40,50]. We pad it with 2 bytes,
//       and set the length to 5: [10,20,30,40,50,0,0,5]. This means that the 
//       first 5 bytes of the block are "real", the last 3 should be discarded.
//    2) The last block is 8 bytes long: [10,20,30,40,50,60,70,80]. We keep this 
//       block as is, and add a new final block: [0,0,0,0,0,0,0,0]. When we decrypt,
//       the entire last block will be discarded since the last byte is 0
BLOCKLIST pad_last_block(BLOCKLIST blocks) {
    // TODO
  struct BLOCK* lastblock = blocks->last;
  if(debug)
      printf("Printing blocklist before padding\n");
      print_blocklist(blocks);

  uint8_t *p = (uint8_t *)&lastblock->block;
  //if you need a copy
  uint8_t result[8];
  for(int i = 0; i < 8; i++) {
      result[i] = p[i];
      if(debug)
      {
        printf("%x ", result[i]);
        printf("\n");
      }
  }
  int blocksize = lastblock->size;
  if( blocksize == 8)
  {
    // create a new empty block
    BLOCKTYPE emptyBlock = 0;
    BLOCKNODE paddedblock = newBlock(emptyBlock, 8);
    lastblock->next = paddedblock;
    lastblock = paddedblock;
    
  }
  else {
    result[7] = blocksize;
  
    uint64_t paddedblock = u8tou64(result);
    if(debug){
      printf("%lx\n", paddedblock);
    }
    lastblock->block = paddedblock;
    if(debug) {
      p = (uint8_t *)&paddedblock;
      //if you need a copy
       result[8];
      for(int i = 0; i < 8; i++) {
          result[i] = p[i];
          if (debug) {
            printf("%x ", result[i]);
            printf("\n");
          }
      }
    }

  }

  if(debug)
      printf("Printing blocklist after padding\n");
      print_blocklist(blocks);

  return blocks;
}

// Reads the message to be encrypted, an ASCII text file, and returns a linked list 
// of BLOCKs, each representing a 64 bit block. In other words, read the first 8 characters
// from the input file, and convert them (just a C cast) to 64 bits; this is your first block.
// Continue to the end of the file.

BLOCKLIST read_cleartext_message(FILE *msg_fp) {
   BLOCKLIST blocks = newBlockList();
   unsigned char byte = 0;
   int retVal = fscanf(msg_fp, "%c", &byte);
   BLOCKTYPE block = 0;
   int counter = 0;
   while (retVal > 0) {
      if (counter == 8) {
         addBlockLast(blocks, block, counter);
         counter = 0;
         block = 0;
      }
      block = (block << 8) | byte;
      counter++;
      retVal = fscanf(msg_fp, "%c", &byte);
   }
   if (counter > 0) {
      addBlockLast(blocks, block, counter);
   }
   blocks = pad_last_block(blocks);
   if (debug) {
      printf("Cleartext Blocks Read\n");
      print_blocklist(blocks);
   }
   return blocks;
}


// Reads the encrypted message, and returns a linked list of blocks, each 64 bits. 
// Note that, because of the padding that was done by the encryption, the length of 
// this file should always be a multiple of 8 bytes. The output is a linked list of
// 64-bit blocks.
BLOCKLIST read_encrypted_file(FILE *msg_fp) {
    BLOCKLIST blocks = newBlockList();
    uint64_t block;
    while(fread(&block, sizeof(block), 1, msg_fp) > 0){
      addBlockLast(blocks, block, 8);
      block = 0;
    }
    return blocks;
}

// Reads 56-bit key into a 64 bit unsigned int. We will ignore the most significant byte,
// i.e. we'll assume that the top 8 bits are all 0. In real DES, these are used to check 
// that the key hasn't been corrupted in transit. The key file is ASCII, consisting of
// exactly one line. That line has a single hex number on it, the key, such as 0x08AB674D9.
KEYTYPE read_key(FILE *key_fp) {
   char buff[17];
   fgets(buff, 17, key_fp);
   KEYTYPE key = strtol(buff, 0, 16);
   return key;
}

// Write the encrypted blocks to file. The encrypted file is in binary, i.e., you can
// just write each 64-bit block directly to the file, without any conversion.
void write_encrypted_message(FILE *msg_fp, BLOCKLIST msg) {
    assert(msg != NULL);
    BLOCKNODE curr = msg->first;
    char buffer[8];
    while(curr != NULL){
        memcpy(&buffer,&(curr->block),curr->size);
        fwrite(buffer, sizeof(char), 8, msg_fp);
        curr = curr->next;
    }
    return;
}

// Write the encrypted blocks to file. This is called by the decryption routine.
// The output file is a plain ASCII file, containing the decrypted text message.
void write_decrypted_message(FILE *msg_fp, BLOCKLIST msg) {
   assert(msg != NULL);
   BLOCKNODE cur = msg->first;
   int size;
   unsigned int charac;
   while(cur != NULL){
      for(int i=8; i>=1; i--){
         charac = (unsigned int) (((cur->block) >> ((i-1)*8)) & 0xFF);
         fprintf(msg_fp,"%c",charac);
      }
      cur = cur->next;
   }
}


/////////////////////////////////////////////////////////////////////////////
// Encryption
/////////////////////////////////////////////////////////////////////////////

BLOCKTYPE perm_64_64(BLOCKTYPE M, uint64_t *PERM_TABLE){
    BLOCKTYPE M_IP = 0;
    for (int i=0; i<64; i++){
        M_IP = M_IP | ((M>>(PERM_TABLE[i] - 1)) & (1)) << i;
    }
    return M_IP;
}

BLOCKTYPE perm_64_64_final(BLOCKTYPE M, int *FINAL_PERMTABLE){
    BLOCKTYPE FP = 0;
    for (int i=0; i<64; i++){
        FP = FP | ((M>>( (BLOCKTYPE)FINAL_PERMTABLE[i] - 1)) & (1)) << i;
    }
    return FP;
}

BLOCKTYPE perm_32_48(HALFBLOCKTYPE R, uint64_t *EXPAND_BOX){
    BLOCKTYPE EX_M = 0;
    for (int i=0; i<48; i++){
        EX_M = EX_M | ((R>>(EXPAND_BOX[i] - 1)) & (1)) << i;
    }
    return EX_M;
}

HALFBLOCKTYPE perm_32_32(HALFBLOCKTYPE substitutedMessage, uint32_t *PBOX){
    HALFBLOCKTYPE P_M = 0;
    for (int i=0; i<32; i++){
        P_M = P_M | ( (substitutedMessage >> (PBOX[i] - 1)) & (1)) << i;
    }
    return P_M;
}

HALFBLOCKTYPE unitSubstitute(HALFBLOCKTYPE input_6, uint64_t sbox[][16]){
    //printf("[DEBUG: unitSubstitute] Entered unitSubstitute function\n");
    //printf("[DEBUG: unitSubstitute] Value of 6 bit input is 0x%x\n", input_6);
    HALFBLOCKTYPE output_4;
    int row =  (2* ((input_6>>5) & 1)) + (input_6 & 1);
    //printf("[DEBUG: unitSubstitute] Row value is %d\n", row);
    int column = (input_6 >> 1) & 0xf;
    //printf("[DEBUG: unitSubstitute] Column value is %d\n", column);
    output_4 = (HALFBLOCKTYPE) sbox[row][column];
    //printf("[DEBUG: unitSubstitute] Value of calculated 4-bit output is 0x%x\n", output_4);
    return output_4;
}

HALFBLOCKTYPE substitute(BLOCKTYPE exp_message_48){
    HALFBLOCKTYPE substituted_message = 0;
    HALFBLOCKTYPE intermediate_input_6 = 0;
    HALFBLOCKTYPE intermediate_output_4 = 0;

    //printf("[DEBUG: substitute] Starting sbox substitution of 6 bit units\n");
    // Running S1 box
    intermediate_input_6 = (exp_message_48 & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_1);
    substituted_message = substituted_message | intermediate_output_4;

    // Running S2 box
    intermediate_input_6 = ((exp_message_48 >> 6) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_2);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S3 box
    intermediate_input_6 = ((exp_message_48 >> 12) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_3);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S4 box
    intermediate_input_6 = ((exp_message_48 >> 18) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_4);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S5 box
    intermediate_input_6 = ((exp_message_48 >> 24) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_5);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S6 box
    intermediate_input_6 = ((exp_message_48 >> 30) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_6);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S7 box
    intermediate_input_6 = ((exp_message_48 >> 36) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_7);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S8 box
    intermediate_input_6 = ((exp_message_48 >> 42) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_8);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    //printf("[DEBUG: substitute] Finished sbox substitution of 6 bit units\n");

    return substituted_message;
}

HALFBLOCKTYPE fiestel(HALFBLOCKTYPE righthalf, int i){
    // Expansion permutation 32 -> 48
    BLOCKTYPE exp_message = perm_32_48(righthalf, expand_box);

    // XOR with subkey[i]
    exp_message = exp_message ^ (getSubKey(i));

    // 8 S-box substitution 48 -> 32

    HALFBLOCKTYPE substituted_message = substitute(exp_message);


    // P-box permutation 32 -> 32 (return)
    HALFBLOCKTYPE fiestelOutput = perm_32_32(substituted_message, Pbox);

    return fiestelOutput;
}

// Encrypt one block. This is where the main computation takes place. It takes
// one 64-bit block as input, and returns the encrypted 64-bit block. The 
// subkeys needed by the Feistel Network is given by the function getSubKey(i).
BLOCKTYPE des_enc(BLOCKTYPE v){
   // TODO
   HALFBLOCKTYPE L;
   HALFBLOCKTYPE R;
   HALFBLOCKTYPE temp_R = 0;

   // Initial permutation on the Plaintext
   BLOCKTYPE IP = perm_64_64(v, init_perm);

   /*
   if (debug) {
       // print v in binary
       printf("[DEBUG] Printing Message Block\n");
       for(int i=63; i>=0; i--){
           printf("%ld [%d], ", (v>>i)&1, i+1); // Indexed from 1-64
       }
       printf("\n");
       // print IP
       printf("[DEBUG] Printing Message after Initial Permutation\n");
       for (int i=63; i>=0; i--){
           printf("%ld [%ld], ", (IP>>i)&1, init_perm[i]);  // Indexed from 1-64
       }
       printf("\n");
   }
   */

   L = (HALFBLOCKTYPE)(IP >> 32);
   R = (HALFBLOCKTYPE)(IP);

   for (int i = 0; i < 16; i++){
        temp_R = R;
        R = L ^ (fiestel(R, i));
        L = temp_R;
   }

    // Final switching between L and R
    temp_R = R;
    R = L;
    L = temp_R;

   // Final permutation on L and R
   BLOCKTYPE FP = ( ((BLOCKTYPE)L) << 32 ) | (BLOCKTYPE)R;
   FP = perm_64_64_final(FP, final_perm);

   return FP;
}

// Encrypt the blocks in ECB mode. The blocks have already been padded 
// by the input routine. The output is an encrypted list of blocks.
BLOCKLIST des_enc_ECB(BLOCKLIST msg) {
    assert(msg != NULL);
    BLOCKLIST encryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    while(start!=NULL)
    {
      BLOCKTYPE block = start->block;
      BLOCKTYPE encryptedblock = des_enc(block);
      if(debug){
          printf("Encrypted Block[%d] 0x%lx\n", i, encryptedblock);
          i++;
      }
      addBlockLast(encryptedBlockList, encryptedblock, start->size);
      start = start->next;
    }
   return encryptedBlockList;
}

// Same as des_enc_ECB, but encrypt the blocks in Counter mode.
// SEE: https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#Counter_(CTR)
// Start the counter at 0.
BLOCKLIST des_enc_CTR(BLOCKLIST msg) {
    assert(msg != NULL);
    // TODO
    // Should call des_enc in here repeatedly
    BLOCKLIST encryptedBlockList = newBlockList();
    BLOCKNODE start = msg->first;
    int ctr = 0;
    KEYTYPE original_key = key;
    int i = 0; // Used for debugging only
    while(start!=NULL) {
      key = original_key ^ ctr;
      BLOCKTYPE block = start->block;
      BLOCKTYPE encryptedblock = des_enc(block);
      if(debug) {
          printf("Encrypted Block[%d] 0x%lx\n", i, encryptedblock);
          i++;
      }
      addBlockLast(encryptedBlockList, encryptedblock, 8);
      start = start->next;
      key = original_key;
      ctr += 1;
    }
    return encryptedBlockList;
}

/////////////////////////////////////////////////////////////////////////////
// Decryption
/////////////////////////////////////////////////////////////////////////////
// Decrypt one block.
BLOCKTYPE des_dec(BLOCKTYPE v){
    // TODO
    HALFBLOCKTYPE L;
    HALFBLOCKTYPE R;
    HALFBLOCKTYPE temp_R = 0;

    // Initial permutation on the Plaintext
    BLOCKTYPE IP = perm_64_64(v, init_perm);

    /*
    if (debug) {
        // print v in binary
        printf("[DEBUG] Printing Message Block\n");
        for(int i=63; i>=0; i--){
            printf("%ld [%d], ", (v>>i)&1, i+1); // Indexed from 1-64
        }
        printf("\n");
        // print IP
        printf("[DEBUG] Printing Message after Initial Permutation\n");
        for (int i=63; i>=0; i--){
            printf("%ld [%ld], ", (IP>>i)&1, init_perm[i]);  // Indexed from 1-64
        }
        printf("\n");
    }
    */

    L = (HALFBLOCKTYPE)(IP >> 32);
    R = (HALFBLOCKTYPE)(IP);

    for (int i = 15; i >= 0; i--){
        temp_R = R;
        R = L ^ (fiestel(R, i));
        L = temp_R;
    }

    // Final switching between L and R
    temp_R = R;
    R = L;
    L = temp_R;

    // Final permutation on L and R
    BLOCKTYPE FP = ( ((BLOCKTYPE)L) << 32 ) | (BLOCKTYPE)R;
    FP = perm_64_64_final(FP, final_perm);

    return FP;
}

// Decrypt the blocks in ECB mode. The input is a list of encrypted blocks,
// the output a list of plaintext blocks.
BLOCKLIST des_dec_ECB(BLOCKLIST msg) {
    assert(msg != NULL);
    // Should call des_dec in here repeatedly
    BLOCKLIST decryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    while(start!=NULL)
    {
        BLOCKTYPE block = start->block;
        BLOCKTYPE decryptedblock = des_dec(block);
        if(debug){
            printf("Decrypted Block[%d] 0x%lx\n", i, decryptedblock);
            i++;
        }
        addBlockLast(decryptedBlockList, decryptedblock, start->size);
        start = start->next;
    }
    return decryptedBlockList;
}

// Decrypt the blocks in Counter mode
BLOCKLIST des_dec_CTR(BLOCKLIST msg) {
    assert(msg != NULL);
    // Should call des_dec in here repeatedly
    BLOCKLIST decryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    int ctr = 0;
    KEYTYPE original_key = key;
    while(start!=NULL)
    {
        key = original_key ^ ctr;
        BLOCKTYPE block = start->block;
        BLOCKTYPE decryptedblock = des_dec(block);
        if(debug){
            printf("Decrypted Block[%d] 0x%lx\n", i, decryptedblock);
            i++;
        }
        addBlockLast(decryptedBlockList, decryptedblock, start->size);
        start = start->next;
        key = original_key;
        ctr += 1;
    }
    return decryptedBlockList;

}

/////////////////////////////////////////////////////////////////////////////
// Main routine
/////////////////////////////////////////////////////////////////////////////

void encrypt (int argc, char **argv) {
      FILE *msg_fp = fopen("message.txt", "r");
      BLOCKLIST msg = read_cleartext_message(msg_fp);
      fclose(msg_fp);

      BLOCKLIST encrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         encrypted_message = des_enc_ECB(msg);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         encrypted_message = des_enc_CTR(msg);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "wb");
      write_encrypted_message(encrypted_msg_fp, encrypted_message);
      fclose(encrypted_msg_fp);
}

void decrypt (int argc, char **argv) {
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "rb");
      assert(encrypted_msg_fp != NULL);
      BLOCKLIST encrypted_message = read_encrypted_file(encrypted_msg_fp);
      fclose(encrypted_msg_fp);

      assert(encrypted_message != NULL);
      if(debug) {
          printf("[DEBUG: decrypt] Printing encrypted message block from encrypted binary file\n");
          print_blocklist(encrypted_message);
      }

      BLOCKLIST decrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         decrypted_message = des_dec_ECB(encrypted_message);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         decrypted_message = des_dec_CTR(encrypted_message);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };

      if (debug){
          print_blocklist(decrypted_message);
      }

      FILE *decrypted_msg_fp = fopen("decrypted_message.txt", "w");
      write_decrypted_message(decrypted_msg_fp, decrypted_message);
      fclose(decrypted_msg_fp);
}

int main(int argc, char **argv){
   FILE *key_fp = fopen("key.txt","r");
   key = read_key(key_fp);
   //generateSubKeys(key);              // This does nothing right now.
   fclose(key_fp);

   if (argc != 3) {
     printf("Two arguments expected.\n");
     exit(-1);
   }

   if (strncmp(argv[1],"-enc",4) == 0) {
      encrypt(argc, argv);
   } else if (strncmp(argv[1],"-dec",4) == 0) {
      decrypt(argc, argv);
   } else {
     printf("First argument should be -enc or -dec\n"); 
   }
   return 0;
}#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

 /*
 * des takes two arguments on the command line:
 *    des -enc -ecb      -- encrypt in ECB mode
 *    des -enc -ctr      -- encrypt in CTR mode
 *    des -dec -ecb      -- decrypt in ECB mode
 *    des -dec -ctr      -- decrypt in CTR mode
 * des also reads some hardcoded files:
 *    message.txt            -- the ASCII text message to be encrypted,
 *                              read by "des -enc"
 *    encrypted_msg.bin      -- the encrypted message, a binary file,
 *                              written by "des -enc"
 *    decrypted_message.txt  -- the decrypted ASCII text message
 *    key.txt                -- just contains the key, on a line by itself, as an ASCII 
 *                              hex number, such as: 0x34FA879B
*/


uint64_t
u8tou64(uint8_t const u8[static 8]){
  uint64_t u64;
  memcpy(&u64, u8, sizeof u64);
  return u64;
}

unsigned createMask(unsigned a, unsigned b)
{
   unsigned r = 0;
   for (unsigned i=a; i<=b; i++)
       r |= 1 << i;

   return r;
}

/////////////////////////////////////////////////////////////////////////////
// Type definitions
/////////////////////////////////////////////////////////////////////////////
typedef uint64_t KEYTYPE;
typedef uint32_t SUBKEYTYPE;
typedef uint64_t BLOCKTYPE;
typedef uint32_t HALFBLOCKTYPE;

struct BLOCK {
    BLOCKTYPE block;        // the block read
    int size;               // number of "real" bytes in the block, should be 8, unless it's the last block
    struct BLOCK *next;     // pointer to the next block
};

struct BLOCKHEAD {
    struct BLOCK *first;   // first block
    struct BLOCK *last;    // last block
};

typedef struct BLOCK* BLOCKNODE;
typedef struct BLOCKHEAD* BLOCKLIST;


BLOCKLIST newBlockList() {
   BLOCKLIST blocks = (BLOCKLIST)malloc(sizeof(struct BLOCKHEAD));
   blocks->first = NULL;
   blocks->last = NULL;
   return blocks;
}

BLOCKNODE newBlock(uint64_t block, int size){
  BLOCKNODE new_node = (BLOCKNODE) malloc(sizeof(struct BLOCK));
  new_node->block = block;
  new_node->size = size;
  new_node->next = NULL;
  return new_node;
}

void addBlockLast(BLOCKLIST list, BLOCKTYPE block, int size) {
   BLOCKNODE currBlock = newBlock(block, size);
   if (list->first == NULL) {
      list->first = currBlock;
      list->last = currBlock;
    } else {
      list->last->next = currBlock;
      list->last = currBlock;
    }
}

/////////////////////////////////////////////////////////////////////////////
// Debugging
/////////////////////////////////////////////////////////////////////////////
int debug = 1;

void print_blocklist(BLOCKLIST head) {
   BLOCKNODE curr = head->first;
   int x = 0;
   assert(curr != NULL);

   while (curr != NULL) {
           printf("\tBLOCK [%d]: 0x%lx, size=%d\n", x, curr->block,curr->size);
      x++;
      curr = curr->next;
   }
}

void print_block(struct BLOCK* head) {
   
   printf("\tBLOCK : 0x%lx, size=%d\n", head->block,head->size);
      
}

/////////////////////////////////////////////////////////////////////////////
// Initial and final permutation
/////////////////////////////////////////////////////////////////////////////
uint64_t init_perm[] = {
   58,50,42,34,26,18,10,2,
   60,52,44,36,28,20,12,4,
   62,54,46,38,30,22,14,6,
   64,56,48,40,32,24,16,8,
   57,49,41,33,25,17,9,1,
   59,51,43,35,27,19,11,3,
   61,53,45,37,29,21,13,5,
   63,55,47,39,31,23,15,7
};

int final_perm[] = {
   40,8,48,16,56,24,64,32,
   39,7,47,15,55,23,63,31,
   38,6,46,14,54,22,62,30,
   37,5,45,13,53,21,61,29,
   36,4,44,12,52,20,60,28,
   35,3,43,11,51,19,59,27,
   34,2,42,10,50,18,58,26,
   33,1,41,9, 49,17,57,25
};

/////////////////////////////////////////////////////////////////////////////
// Subkey generation
/////////////////////////////////////////////////////////////////////////////

//SUBKEYTYPE subkeys[16];
KEYTYPE key = 0;    // Global variable key

// This function returns the i:th subkey, 48 bits long. To simplify the assignment 
// you can use a trivial implementation: just take the input key and xor it with i.
uint64_t getSubKey(int i) {
   // TODO: return the first 48 bits of the 56 bit DES key, xor:ed with i.
   return ((key ^ i) & 0xffffffffffff);
   //return subkeys[i];
}

// For extra credit, write the real DES key expansion routine!
//SUBKEYTYPE* generateSubKeys(KEYTYPE key) {
//   // TODO for extra credit
//  for( int i = 0; i<16; i++){
//    unsigned r = createMask(0,47);
//    unsigned result = r & (key^i);
//    subkeys[i] = result;
//  }
//  return subkeys;
//}

/////////////////////////////////////////////////////////////////////////////
// P-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t expand_box[] = {
   32,1,2,3,4,5,4,5,6,7,8,9,
   8,9,10,11,12,13,12,13,14,15,16,17,
   16,17,18,19,20,21,20,21,22,23,24,25,
   24,25,26,27,28,29,28,29,30,31,32,1
};

uint32_t Pbox[] = 
{
   16,7,20,21,29,12,28,17,1,15,23,26,5,18,31,10,
   2,8,24,14,32,27,3,9,19,13,30,6,22,11,4,25,
};      

/////////////////////////////////////////////////////////////////////////////
// S-boxes
/////////////////////////////////////////////////////////////////////////////
uint64_t sbox_1[4][16] = {
   {14,  4, 13,  1,  2, 15, 11,  8,  3, 10 , 6, 12,  5,  9,  0,  7},
   { 0, 15,  7,  4, 14,  2, 13,  1, 10,  6, 12, 11,  9,  5,  3,  8},
   { 4,  1, 14,  8, 13,  6,  2, 11, 15, 12,  9,  7,  3, 10,  5,  0},
   {15, 12,  8,  2,  4,  9,  1,  7,  5, 11,  3, 14, 10,  0,  6, 13}};

uint64_t sbox_2[4][16] = {
   {15,  1,  8, 14,  6, 11,  3,  4,  9,  7,  2, 13, 12,  0,  5 ,10},
   { 3, 13,  4,  7, 15,  2,  8, 14, 12,  0,  1, 10,  6,  9, 11,  5},
   { 0, 14,  7, 11, 10,  4, 13,  1,  5,  8, 12,  6,  9,  3,  2, 15},
   {13,  8, 10,  1,  3, 15,  4,  2, 11,  6,  7, 12,  0,  5, 14,  9}};

uint64_t sbox_3[4][16] = {
   {10,  0,  9, 14,  6,  3, 15,  5,  1, 13, 12,  7, 11,  4,  2,  8},
   {13,  7,  0,  9,  3,  4,  6, 10,  2,  8,  5, 14, 12, 11, 15,  1},
   {13,  6,  4,  9,  8, 15,  3,  0, 11,  1,  2, 12,  5, 10, 14,  7},
   { 1, 10, 13,  0,  6,  9,  8,  7,  4, 15, 14,  3, 11,  5,  2, 12}};


uint64_t sbox_4[4][16] = {
   { 7, 13, 14,  3,  0 , 6,  9, 10,  1 , 2 , 8,  5, 11, 12,  4 ,15},
   {13,  8, 11,  5,  6, 15,  0,  3,  4 , 7 , 2, 12,  1, 10, 14,  9},
   {10,  6,  9 , 0, 12, 11,  7, 13 ,15 , 1 , 3, 14 , 5 , 2,  8,  4},
   { 3, 15,  0,  6, 10,  1, 13,  8,  9 , 4 , 5, 11 ,12 , 7,  2, 14}};
 
 
uint64_t sbox_5[4][16] = {
   { 2, 12,  4,  1 , 7 ,10, 11,  6 , 8 , 5 , 3, 15, 13,  0, 14,  9},
   {14, 11 , 2 ,12 , 4,  7, 13 , 1 , 5 , 0, 15, 10,  3,  9,  8,  6},
   { 4,  2 , 1, 11, 10, 13,  7 , 8 ,15 , 9, 12,  5,  6 , 3,  0, 14},
   {11,  8 ,12 , 7 , 1, 14 , 2 ,13 , 6 ,15,  0,  9, 10 , 4,  5,  3}};


uint64_t sbox_6[4][16] = {
   {12,  1, 10, 15 , 9 , 2 , 6 , 8 , 0, 13 , 3 , 4 ,14 , 7  ,5 ,11},
   {10, 15,  4,  2,  7, 12 , 9 , 5 , 6,  1 ,13 ,14 , 0 ,11 , 3 , 8},
   { 9, 14 ,15,  5,  2,  8 ,12 , 3 , 7 , 0,  4 ,10  ,1 ,13 ,11 , 6},
   { 4,  3,  2, 12 , 9,  5 ,15 ,10, 11 ,14,  1 , 7  ,6 , 0 , 8 ,13}};
 

uint64_t sbox_7[4][16] = {
   { 4, 11,  2, 14, 15,  0 , 8, 13, 3,  12 , 9 , 7,  6 ,10 , 6 , 1},
   {13,  0, 11,  7,  4 , 9,  1, 10, 14 , 3 , 5, 12,  2, 15 , 8 , 6},
   { 1 , 4, 11, 13, 12,  3,  7, 14, 10, 15 , 6,  8,  0,  5 , 9 , 2},
   { 6, 11, 13 , 8,  1 , 4, 10,  7,  9 , 5 , 0, 15, 14,  2 , 3 ,12}};
 
uint64_t sbox_8[4][16] = {
   {13,  2,  8,  4,  6 ,15 ,11,  1, 10,  9 , 3, 14,  5,  0, 12,  7},
   { 1, 15, 13,  8 ,10 , 3  ,7 , 4, 12 , 5,  6 ,11,  0 ,14 , 9 , 2},
   { 7, 11,  4,  1,  9, 12, 14 , 2,  0  ,6, 10 ,13 ,15 , 3  ,5  ,8},
   { 2,  1, 14 , 7 , 4, 10,  8, 13, 15, 12,  9,  0 , 3,  5 , 6 ,11}};

/////////////////////////////////////////////////////////////////////////////
// I/O
/////////////////////////////////////////////////////////////////////////////

// Pad the list of blocks, so that every block is 64 bits, even if the
// file isn't a perfect multiple of 8 bytes long. In the input list of blocks,
// the last block may have "size" < 8. In this case, it needs to be padded. See 
// the slides for how to do this (the last byte of the last block 
// should contain the number if real bytes in the block, add an extra block if
// the file is an exact multiple of 8 bytes long.) The returned
// list of blocks will always have the "size"-field=8.
// Example:
//    1) The last block is 5 bytes long: [10,20,30,40,50]. We pad it with 2 bytes,
//       and set the length to 5: [10,20,30,40,50,0,0,5]. This means that the 
//       first 5 bytes of the block are "real", the last 3 should be discarded.
//    2) The last block is 8 bytes long: [10,20,30,40,50,60,70,80]. We keep this 
//       block as is, and add a new final block: [0,0,0,0,0,0,0,0]. When we decrypt,
//       the entire last block will be discarded since the last byte is 0
BLOCKLIST pad_last_block(BLOCKLIST blocks) {
    // TODO
  struct BLOCK* lastblock = blocks->last;
  if(debug)
      printf("Printing blocklist before padding\n");
      print_blocklist(blocks);

  uint8_t *p = (uint8_t *)&lastblock->block;
  //if you need a copy
  uint8_t result[8];
  for(int i = 0; i < 8; i++) {
      result[i] = p[i];
      if(debug)
      {
        printf("%x ", result[i]);
        printf("\n");
      }
  }
  int blocksize = lastblock->size;
  if( blocksize == 8)
  {
    // create a new empty block
    BLOCKTYPE emptyBlock = 0;
    BLOCKNODE paddedblock = newBlock(emptyBlock, 8);
    lastblock->next = paddedblock;
    lastblock = paddedblock;
    
  }
  else {
    result[7] = blocksize;
  
    uint64_t paddedblock = u8tou64(result);
    if(debug){
      printf("%lx\n", paddedblock);
    }
    lastblock->block = paddedblock;
    if(debug) {
      p = (uint8_t *)&paddedblock;
      //if you need a copy
       result[8];
      for(int i = 0; i < 8; i++) {
          result[i] = p[i];
          if (debug) {
            printf("%x ", result[i]);
            printf("\n");
          }
      }
    }

  }

  if(debug)
      printf("Printing blocklist after padding\n");
      print_blocklist(blocks);

  return blocks;
}

// Reads the message to be encrypted, an ASCII text file, and returns a linked list 
// of BLOCKs, each representing a 64 bit block. In other words, read the first 8 characters
// from the input file, and convert them (just a C cast) to 64 bits; this is your first block.
// Continue to the end of the file.

BLOCKLIST read_cleartext_message(FILE *msg_fp) {
   BLOCKLIST blocks = newBlockList();
   unsigned char byte = 0;
   int retVal = fscanf(msg_fp, "%c", &byte);
   BLOCKTYPE block = 0;
   int counter = 0;
   while (retVal > 0) {
      if (counter == 8) {
         addBlockLast(blocks, block, counter);
         counter = 0;
         block = 0;
      }
      block = (block << 8) | byte;
      counter++;
      retVal = fscanf(msg_fp, "%c", &byte);
   }
   if (counter > 0) {
      addBlockLast(blocks, block, counter);
   }
   blocks = pad_last_block(blocks);
   if (debug) {
      printf("Cleartext Blocks Read\n");
      print_blocklist(blocks);
   }
   return blocks;
}


// Reads the encrypted message, and returns a linked list of blocks, each 64 bits. 
// Note that, because of the padding that was done by the encryption, the length of 
// this file should always be a multiple of 8 bytes. The output is a linked list of
// 64-bit blocks.
BLOCKLIST read_encrypted_file(FILE *msg_fp) {
    BLOCKLIST blocks = newBlockList();
    uint64_t block;
    while(fread(&block, sizeof(block), 1, msg_fp) > 0){
      addBlockLast(blocks, block, 8);
      block = 0;
    }
    return blocks;
}

// Reads 56-bit key into a 64 bit unsigned int. We will ignore the most significant byte,
// i.e. we'll assume that the top 8 bits are all 0. In real DES, these are used to check 
// that the key hasn't been corrupted in transit. The key file is ASCII, consisting of
// exactly one line. That line has a single hex number on it, the key, such as 0x08AB674D9.
KEYTYPE read_key(FILE *key_fp) {
   char buff[17];
   fgets(buff, 17, key_fp);
   KEYTYPE key = strtol(buff, 0, 16);
   return key;
}

// Write the encrypted blocks to file. The encrypted file is in binary, i.e., you can
// just write each 64-bit block directly to the file, without any conversion.
void write_encrypted_message(FILE *msg_fp, BLOCKLIST msg) {
    assert(msg != NULL);
    BLOCKNODE curr = msg->first;
    char buffer[8];
    while(curr != NULL){
        memcpy(&buffer,&(curr->block),curr->size);
        fwrite(buffer, sizeof(char), 8, msg_fp);
        curr = curr->next;
    }
    return;
}

// Write the encrypted blocks to file. This is called by the decryption routine.
// The output file is a plain ASCII file, containing the decrypted text message.
void write_decrypted_message(FILE *msg_fp, BLOCKLIST msg) {
   assert(msg != NULL);
   BLOCKNODE cur = msg->first;
   int size;
   unsigned int charac;
   while(cur != NULL){
      for(int i=8; i>=1; i--){
         charac = (unsigned int) (((cur->block) >> ((i-1)*8)) & 0xFF);
         fprintf(msg_fp,"%c",charac);
      }
      cur = cur->next;
   }
}


/////////////////////////////////////////////////////////////////////////////
// Encryption
/////////////////////////////////////////////////////////////////////////////

BLOCKTYPE perm_64_64(BLOCKTYPE M, uint64_t *PERM_TABLE){
    BLOCKTYPE M_IP = 0;
    for (int i=0; i<64; i++){
        M_IP = M_IP | ((M>>(PERM_TABLE[i] - 1)) & (1)) << i;
    }
    return M_IP;
}

BLOCKTYPE perm_64_64_final(BLOCKTYPE M, int *FINAL_PERMTABLE){
    BLOCKTYPE FP = 0;
    for (int i=0; i<64; i++){
        FP = FP | ((M>>( (BLOCKTYPE)FINAL_PERMTABLE[i] - 1)) & (1)) << i;
    }
    return FP;
}

BLOCKTYPE perm_32_48(HALFBLOCKTYPE R, uint64_t *EXPAND_BOX){
    BLOCKTYPE EX_M = 0;
    for (int i=0; i<48; i++){
        EX_M = EX_M | ((R>>(EXPAND_BOX[i] - 1)) & (1)) << i;
    }
    return EX_M;
}

HALFBLOCKTYPE perm_32_32(HALFBLOCKTYPE substitutedMessage, uint32_t *PBOX){
    HALFBLOCKTYPE P_M = 0;
    for (int i=0; i<32; i++){
        P_M = P_M | ( (substitutedMessage >> (PBOX[i] - 1)) & (1)) << i;
    }
    return P_M;
}

HALFBLOCKTYPE unitSubstitute(HALFBLOCKTYPE input_6, uint64_t sbox[][16]){
    //printf("[DEBUG: unitSubstitute] Entered unitSubstitute function\n");
    //printf("[DEBUG: unitSubstitute] Value of 6 bit input is 0x%x\n", input_6);
    HALFBLOCKTYPE output_4;
    int row =  (2* ((input_6>>5) & 1)) + (input_6 & 1);
    //printf("[DEBUG: unitSubstitute] Row value is %d\n", row);
    int column = (input_6 >> 1) & 0xf;
    //printf("[DEBUG: unitSubstitute] Column value is %d\n", column);
    output_4 = (HALFBLOCKTYPE) sbox[row][column];
    //printf("[DEBUG: unitSubstitute] Value of calculated 4-bit output is 0x%x\n", output_4);
    return output_4;
}

HALFBLOCKTYPE substitute(BLOCKTYPE exp_message_48){
    HALFBLOCKTYPE substituted_message = 0;
    HALFBLOCKTYPE intermediate_input_6 = 0;
    HALFBLOCKTYPE intermediate_output_4 = 0;

    //printf("[DEBUG: substitute] Starting sbox substitution of 6 bit units\n");
    // Running S1 box
    intermediate_input_6 = (exp_message_48 & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_1);
    substituted_message = substituted_message | intermediate_output_4;

    // Running S2 box
    intermediate_input_6 = ((exp_message_48 >> 6) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_2);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S3 box
    intermediate_input_6 = ((exp_message_48 >> 12) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_3);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S4 box
    intermediate_input_6 = ((exp_message_48 >> 18) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_4);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S5 box
    intermediate_input_6 = ((exp_message_48 >> 24) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_5);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S6 box
    intermediate_input_6 = ((exp_message_48 >> 30) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_6);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S7 box
    intermediate_input_6 = ((exp_message_48 >> 36) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_7);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    // Running S8 box
    intermediate_input_6 = ((exp_message_48 >> 42) & 0x3f);
    intermediate_output_4 = unitSubstitute(intermediate_input_6, sbox_8);
    substituted_message = (substituted_message << 4) | intermediate_output_4;

    //printf("[DEBUG: substitute] Finished sbox substitution of 6 bit units\n");

    return substituted_message;
}

HALFBLOCKTYPE fiestel(HALFBLOCKTYPE righthalf, int i){
    // Expansion permutation 32 -> 48
    BLOCKTYPE exp_message = perm_32_48(righthalf, expand_box);

    // XOR with subkey[i]
    exp_message = exp_message ^ (getSubKey(i));

    // 8 S-box substitution 48 -> 32

    HALFBLOCKTYPE substituted_message = substitute(exp_message);


    // P-box permutation 32 -> 32 (return)
    HALFBLOCKTYPE fiestelOutput = perm_32_32(substituted_message, Pbox);

    return fiestelOutput;
}

// Encrypt one block. This is where the main computation takes place. It takes
// one 64-bit block as input, and returns the encrypted 64-bit block. The 
// subkeys needed by the Feistel Network is given by the function getSubKey(i).
BLOCKTYPE des_enc(BLOCKTYPE v){
   // TODO
   HALFBLOCKTYPE L;
   HALFBLOCKTYPE R;
   HALFBLOCKTYPE temp_R = 0;

   // Initial permutation on the Plaintext
   BLOCKTYPE IP = perm_64_64(v, init_perm);

   /*
   if (debug) {
       // print v in binary
       printf("[DEBUG] Printing Message Block\n");
       for(int i=63; i>=0; i--){
           printf("%ld [%d], ", (v>>i)&1, i+1); // Indexed from 1-64
       }
       printf("\n");
       // print IP
       printf("[DEBUG] Printing Message after Initial Permutation\n");
       for (int i=63; i>=0; i--){
           printf("%ld [%ld], ", (IP>>i)&1, init_perm[i]);  // Indexed from 1-64
       }
       printf("\n");
   }
   */

   L = (HALFBLOCKTYPE)(IP >> 32);
   R = (HALFBLOCKTYPE)(IP);

   for (int i = 0; i < 16; i++){
        temp_R = R;
        R = L ^ (fiestel(R, i));
        L = temp_R;
   }

    // Final switching between L and R
    temp_R = R;
    R = L;
    L = temp_R;

   // Final permutation on L and R
   BLOCKTYPE FP = ( ((BLOCKTYPE)L) << 32 ) | (BLOCKTYPE)R;
   FP = perm_64_64_final(FP, final_perm);

   return FP;
}

// Encrypt the blocks in ECB mode. The blocks have already been padded 
// by the input routine. The output is an encrypted list of blocks.
BLOCKLIST des_enc_ECB(BLOCKLIST msg) {
    assert(msg != NULL);
    BLOCKLIST encryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    while(start!=NULL)
    {
      BLOCKTYPE block = start->block;
      BLOCKTYPE encryptedblock = des_enc(block);
      if(debug){
          printf("Encrypted Block[%d] 0x%lx\n", i, encryptedblock);
          i++;
      }
      addBlockLast(encryptedBlockList, encryptedblock, start->size);
      start = start->next;
    }
   return encryptedBlockList;
}

// Same as des_enc_ECB, but encrypt the blocks in Counter mode.
// SEE: https://en.wikipedia.org/wiki/Block_cipher_mode_of_operation#Counter_(CTR)
// Start the counter at 0.
BLOCKLIST des_enc_CTR(BLOCKLIST msg) {
    assert(msg != NULL);
    // TODO
    // Should call des_enc in here repeatedly
    BLOCKLIST encryptedBlockList = newBlockList();
    BLOCKNODE start = msg->first;
    int ctr = 0;
    KEYTYPE original_key = key;
    int i = 0; // Used for debugging only
    while(start!=NULL) {
      key = original_key ^ ctr;
      BLOCKTYPE block = start->block;
      BLOCKTYPE encryptedblock = des_enc(block);
      if(debug) {
          printf("Encrypted Block[%d] 0x%lx\n", i, encryptedblock);
          i++;
      }
      addBlockLast(encryptedBlockList, encryptedblock, 8);
      start = start->next;
      key = original_key;
      ctr += 1;
    }
    return encryptedBlockList;
}

/////////////////////////////////////////////////////////////////////////////
// Decryption
/////////////////////////////////////////////////////////////////////////////
// Decrypt one block.
BLOCKTYPE des_dec(BLOCKTYPE v){
    // TODO
    HALFBLOCKTYPE L;
    HALFBLOCKTYPE R;
    HALFBLOCKTYPE temp_R = 0;

    // Initial permutation on the Plaintext
    BLOCKTYPE IP = perm_64_64(v, init_perm);

    /*
    if (debug) {
        // print v in binary
        printf("[DEBUG] Printing Message Block\n");
        for(int i=63; i>=0; i--){
            printf("%ld [%d], ", (v>>i)&1, i+1); // Indexed from 1-64
        }
        printf("\n");
        // print IP
        printf("[DEBUG] Printing Message after Initial Permutation\n");
        for (int i=63; i>=0; i--){
            printf("%ld [%ld], ", (IP>>i)&1, init_perm[i]);  // Indexed from 1-64
        }
        printf("\n");
    }
    */

    L = (HALFBLOCKTYPE)(IP >> 32);
    R = (HALFBLOCKTYPE)(IP);

    for (int i = 15; i >= 0; i--){
        temp_R = R;
        R = L ^ (fiestel(R, i));
        L = temp_R;
    }

    // Final switching between L and R
    temp_R = R;
    R = L;
    L = temp_R;

    // Final permutation on L and R
    BLOCKTYPE FP = ( ((BLOCKTYPE)L) << 32 ) | (BLOCKTYPE)R;
    FP = perm_64_64_final(FP, final_perm);

    return FP;
}

// Decrypt the blocks in ECB mode. The input is a list of encrypted blocks,
// the output a list of plaintext blocks.
BLOCKLIST des_dec_ECB(BLOCKLIST msg) {
    assert(msg != NULL);
    // Should call des_dec in here repeatedly
    BLOCKLIST decryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    while(start!=NULL)
    {
        BLOCKTYPE block = start->block;
        BLOCKTYPE decryptedblock = des_dec(block);
        if(debug){
            printf("Decrypted Block[%d] 0x%lx\n", i, decryptedblock);
            i++;
        }
        addBlockLast(decryptedBlockList, decryptedblock, start->size);
        start = start->next;
    }
    return decryptedBlockList;
}

// Decrypt the blocks in Counter mode
BLOCKLIST des_dec_CTR(BLOCKLIST msg) {
    assert(msg != NULL);
    // Should call des_dec in here repeatedly
    BLOCKLIST decryptedBlockList = newBlockList();
    // Calls des_enc in here repeatedly
    BLOCKNODE start = msg->first;
    int i = 0; // Used for debugging only
    int ctr = 0;
    KEYTYPE original_key = key;
    while(start!=NULL)
    {
        key = original_key ^ ctr;
        BLOCKTYPE block = start->block;
        BLOCKTYPE decryptedblock = des_dec(block);
        if(debug){
            printf("Decrypted Block[%d] 0x%lx\n", i, decryptedblock);
            i++;
        }
        addBlockLast(decryptedBlockList, decryptedblock, start->size);
        start = start->next;
        key = original_key;
        ctr += 1;
    }
    return decryptedBlockList;

}

/////////////////////////////////////////////////////////////////////////////
// Main routine
/////////////////////////////////////////////////////////////////////////////

void encrypt (int argc, char **argv) {
      FILE *msg_fp = fopen("message.txt", "r");
      BLOCKLIST msg = read_cleartext_message(msg_fp);
      fclose(msg_fp);

      BLOCKLIST encrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         encrypted_message = des_enc_ECB(msg);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         encrypted_message = des_enc_CTR(msg);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "wb");
      write_encrypted_message(encrypted_msg_fp, encrypted_message);
      fclose(encrypted_msg_fp);
}

void decrypt (int argc, char **argv) {
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "rb");
      assert(encrypted_msg_fp != NULL);
      BLOCKLIST encrypted_message = read_encrypted_file(encrypted_msg_fp);
      fclose(encrypted_msg_fp);

      assert(encrypted_message != NULL);
      if(debug) {
          printf("[DEBUG: decrypt] Printing encrypted message block from encrypted binary file\n");
          print_blocklist(encrypted_message);
      }

      BLOCKLIST decrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         decrypted_message = des_dec_ECB(encrypted_message);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         decrypted_message = des_dec_CTR(encrypted_message);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };

      if (debug){
          print_blocklist(decrypted_message);
      }

      FILE *decrypted_msg_fp = fopen("decrypted_message.txt", "w");
      write_decrypted_message(decrypted_msg_fp, decrypted_message);
      fclose(decrypted_msg_fp);
}

int main(int argc, char **argv){
   FILE *key_fp = fopen("key.txt","r");
   key = read_key(key_fp);
   //generateSubKeys(key);              // This does nothing right now.
   fclose(key_fp);

   if (argc != 3) {
     printf("Two arguments expected.\n");
     exit(-1);
   }

   if (strncmp(argv[1],"-enc",4) == 0) {
      encrypt(argc, argv);
   } else if (strncmp(argv[1],"-dec",4) == 0) {
      decrypt(argc, argv);
   } else {
     printf("First argument should be -enc or -dec\n"); 
   }
   return 0;
}
void addBlockLast(BLOCKLIST list, BLOCKTYPE block, int size) {
   BLOCKNODE currBlock = newBlock(block, size);
   if (list->first == NULL) {
      list->first = currBlock;
      list->last = currBlock;
    } else {
      list->last->next = currBlock;
      list->last = currBlock;
    }
}

void decrypt (int argc, char **argv) {
      FILE *encrypted_msg_fp = fopen("encrypted_msg.bin", "rb");
      assert(encrypted_msg_fp != NULL);
      BLOCKLIST encrypted_message = read_encrypted_file(encrypted_msg_fp);
      fclose(encrypted_msg_fp);

      assert(encrypted_message != NULL);
      if(debug) {
          printf("[DEBUG: decrypt] Printing encrypted message block from encrypted binary file\n");
          print_blocklist(encrypted_message);
      }

      BLOCKLIST decrypted_message;
      if (strncmp(argv[2], "-ecb", 4) == 0) {   
         decrypted_message = des_dec_ECB(encrypted_message);
      } else if (strncmp(argv[2], "-ctr", 4) == 0) {   
         decrypted_message = des_dec_CTR(encrypted_message);
      } else {
         printf("No such mode.\n");
         exit(-1);
      };

      if (debug){
          print_blocklist(decrypted_message);
      }

      FILE *decrypted_msg_fp = fopen("decrypted_message.txt", "w");
      write_decrypted_message(decrypted_msg_fp, decrypted_message);
      fclose(decrypted_msg_fp);
}

int main(int argc, char **argv){
   FILE *key_fp = fopen("key.txt","r");
   key = read_key(key_fp);
   //generateSubKeys(key);              // This does nothing right now.
   fclose(key_fp);

   if (argc != 3) {
     printf("Two arguments expected.\n");
     exit(-1);
   }

   if (strncmp(argv[1],"-enc",4) == 0) {
      encrypt(argc, argv);
   } else if (strncmp(argv[1],"-dec",4) == 0) {
      decrypt(argc, argv);
   } else {
     printf("First argument should be -enc or -dec\n"); 
   }
   return 0;
}
