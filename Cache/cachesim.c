#include "cachesim.h"
# include <stdio.h>

#define TRUE 1
#define FALSE 0

/**
 * The stuct that you may use to store the metadata for each block in the L1 and L2 caches
 */
typedef struct block_t {
	uint64_t tag; // The tag stored in that block
	uint8_t valid; // Valid bit
	uint8_t dirty; // Dirty bit
	unsigned int counter;
} block;


/**
 * A struct for storing the configuration of both the L1 and L2 caches as passed in the
 * cache_init function. All values represent number of bits. You may add any parameters
 * here, however I strongly suggest not removing anything from the config struct
 */
typedef struct config_t {
	uint64_t C1; /* Size of cache L1 */
	uint64_t C2; /* Size of cache L2 */
	uint64_t S; /* Set associativity of L2 */
	uint64_t B; /* Block size of both caches */
} config;


/****** Do not modify the below function headers ******/
static uint64_t get_tag(uint64_t address, uint64_t C, uint64_t B, uint64_t S);
static uint64_t get_index(uint64_t address, uint64_t C, uint64_t B, uint64_t S);
static uint64_t convert_tag(uint64_t tag, uint64_t index, uint64_t C1, uint64_t C2, uint64_t B, uint64_t S);
static uint64_t convert_index(uint64_t tag, uint64_t index, uint64_t C1, uint64_t C2, uint64_t B, uint64_t S);
static uint64_t convert_tag_l1(uint64_t l2_tag, uint64_t l2_index, uint64_t C1, uint64_t C2, uint64_t B, uint64_t S);
static uint64_t convert_index_l1(uint64_t l2_tag, uint64_t l2_index, uint64_t C1, uint64_t C2, uint64_t B, uint64_t S);

/****** You may add Globals and other function headers that you may need below this line ******/
void writeL2(uint64_t tag2, uint64_t index2, unsigned int blockSet, struct cache_stats_t *stats);
void updateInitialStats(char rw, struct cache_stats_t *stats);
void L1HIT(volatile block* L1BLOCK, char rw, uint64_t addressIndex1, uint64_t addressTag1);
void updateL1Stats(struct cache_stats_t *stats, char rw);
void updateL2Stats(struct cache_stats_t *stats, char rw);
void L1MISSED(volatile block* L1BLOCK, char rw, uint64_t addressIndex1, uint64_t addressTag1, struct cache_stats_t *stats);
void updateL1Cache(volatile block* L1BLOCK, char rw, uint64_t addressIndex1, uint64_t addressTag1);
void writeL1toL2(char rw, uint64_t addressIndex1, uint64_t addressTag1, volatile block* L1BLOCK);
void evictL2(volatile block* LRUblock, uint64_t addressIndex2, struct cache_stats_t *stats);

block *cache1;
block *cache2;
config cacheConfig;
unsigned int mainCounter;




/**
 * Subroutine for initializing your cache with the passed in arguments.
 * You may initialize any globals you might need in this subroutine
 *
 * @param C1 The total size of your L1 cache is 2^C1 bytes
 * @param C2 The tatal size of your L2 cache is 2^C2 bytes
 * @param S The total number of blocks in a line/set of your L2 cache are 2^S
 * @param B The size of your blocks is 2^B bytes
 */
void cache_init(uint64_t C1, uint64_t C2, uint64_t S, uint64_t B)
{
	cache1 = malloc(sizeof(struct block_t)*(1 << (C1 - B)));
	cache2 = malloc(sizeof(struct block_t)*(1 << (C2 - B)));
	for (unsigned int i = 0; i < (1 << (C1 - B)); i++) {
		cache1[i].valid = 0;
		cache1[i].dirty = 0;
	}
	for (unsigned int i = 0; i < (1 << (C2 - B)); i++) {
		cache2[i].valid = 0;
		cache2[i].dirty = 0;
	}
	cacheConfig.C1 = C1;
	cacheConfig.C2 = C2;
	cacheConfig.S = S;
	cacheConfig.B = B;

	mainCounter = 0;
}

/**
 * Subroutine that simulates one cache event at a time.
 * @param rw The type of access, READ or WRITE
 * @param address The address that is being accessed
 * @param stats The struct that you are supposed to store the stats in
 */
void cache_access (char rw, uint64_t address, struct cache_stats_t *stats)
{
	updateInitialStats(rw, stats);

	uint64_t addressIndex1 = get_index(address, cacheConfig.C1, cacheConfig.B, 0);
	uint64_t addressTag1 = get_tag(address, cacheConfig.C1, cacheConfig.B, 0);

	volatile block* L1BLOCK = cache1 + addressIndex1;

	if (L1BLOCK->tag == addressTag1 && L1BLOCK->valid > 0) {
		L1HIT(L1BLOCK, rw, addressIndex1, addressTag1);
	} else {
		updateL1Stats(stats, rw);
		updateL1Cache(L1BLOCK, rw, addressIndex1, addressTag1);
		L1MISSED(L1BLOCK, rw, addressIndex1, addressTag1, stats);
		

	}

	/* 
		Suggested approach:
			-> Find the L1 tag and index of the address that is being passed in to the function
			-> Check if there is a hit in the L1 cache
			-> If L1 misses, check the L2 cache for a hit (Hint: If L2 hits, update L1 with new values)
			-> If L2 misses, need to get values from memory, and update L2 and L1 caches
			
			* We will leave it upto you to decide what must be updated and when
	 */

	
}
void writeL1toL2(char rw, uint64_t addressIndex1, uint64_t addressTag1, volatile block* L1BLOCK) {
	if (L1BLOCK->valid > 0 && L1BLOCK->dirty > 0) {
		uint64_t addressIndex2 = convert_index(addressTag1, addressIndex1, cacheConfig.C1, cacheConfig.C2, cacheConfig.B, cacheConfig.S);
		uint64_t addressTag2 = convert_tag(addressTag1, addressIndex1, cacheConfig.C1, cacheConfig.C2, cacheConfig.B, cacheConfig.S);
		unsigned int blockPerSet = 1 << cacheConfig.S;
		for (unsigned int i = 0; i < blockPerSet; i++) {
			block *L2BLOCK = cache2 + addressIndex2*blockPerSet + i;
			if (L2BLOCK->tag == addressTag2 && L2BLOCK->valid > 0) {
				cache2[addressIndex2*blockPerSet + i].dirty = 1;
				break;
			}
		}
	}
}


void updateL1Cache(volatile block* L1BLOCK, char rw, uint64_t addressIndex1, uint64_t addressTag1) {
	//writeL1toL2(rw, addressIndex1, addressTag1, L1BLOCK);
	L1BLOCK->tag = addressTag1;
	L1BLOCK->valid = 1;
	if (rw == WRITE) {
		L1BLOCK->dirty = 1;
	} else {
		L1BLOCK->dirty = 0;
	}
}

void L1MISSED(volatile block* L1BLOCK, char rw, uint64_t addressIndex1, uint64_t addressTag1, struct cache_stats_t *stats) {
	
	uint64_t addressIndex2 = convert_index(addressTag1, addressIndex1, cacheConfig.C1, cacheConfig.C2, cacheConfig.B, cacheConfig.S);
	uint64_t addressTag2 = convert_tag(addressTag1, addressIndex1, cacheConfig.C1, cacheConfig.C2, cacheConfig.B, cacheConfig.S);
	unsigned int blockPerSet = 1 << cacheConfig.S;


	int hit2 = -1;
	int valid2 = -1;
	unsigned int min = mainCounter;
	int LRUblock = -1;

	for (unsigned int i = 0; i < blockPerSet; i++) {
		if (cache2[addressIndex2*blockPerSet + i].tag == addressTag2 && cache2[addressIndex2*blockPerSet + i].valid > 0) {
			hit2 = addressIndex2*blockPerSet + i;
		}
		if (cache2[addressIndex2*blockPerSet + i].valid == 0) {
			valid2 = addressIndex2*blockPerSet + i;
		}
		if (cache2[addressIndex2*blockPerSet + i].counter < min) {
			min = cache2[addressIndex2*blockPerSet + i].counter;
			LRUblock = addressIndex2*blockPerSet + i;
		}
	}

	if (hit2 >= 0) {
		cache2[hit2].counter = mainCounter;
		if (rw == WRITE) {
			cache2[hit2].dirty = 1;
		}
	} else {
		updateL2Stats(stats, rw);
		if (valid2 >= 0) {
			cache2[valid2].tag = addressTag2;
			cache2[valid2].valid = 1;
			cache2[valid2].counter = mainCounter;
			cache2[valid2].dirty = 0;
			if (rw == WRITE) {
				cache2[valid2].dirty = 1;
			}
		} else {
			evictL2(cache2 + LRUblock, addressIndex2, stats);
			cache2[LRUblock].tag = addressTag2;
			cache2[LRUblock].valid = 1;
			cache2[LRUblock].counter = mainCounter;
			cache2[LRUblock].dirty = 0;
			if (rw == WRITE) {
				cache2[LRUblock].dirty = 1;
			}

		}
	}
}

void evictL2(volatile block* LRUblock, uint64_t addressIndex2, struct cache_stats_t *stats) {
	uint64_t addressTag1 = convert_tag_l1(LRUblock->tag, addressIndex2, cacheConfig.C1, cacheConfig.C2,cacheConfig.B, cacheConfig.S);
	uint64_t addressIndex1 = convert_index_l1(LRUblock->tag, addressIndex2, cacheConfig.C1, cacheConfig.C2,cacheConfig.B, cacheConfig.S);
	unsigned int found = 0;
	if (cache1[addressIndex1].tag == addressTag1 && cache1[addressIndex1].valid > 0) {
		if (cache1[addressIndex1].dirty > 0) {
			found = 1;
		}
		cache1[addressIndex1].valid = 0;
		cache1[addressIndex1].dirty = 0;
	}
	if (LRUblock->dirty > 0 || found > 0) {
		stats->write_backs = stats->write_backs + 1;
	}
}

void updateL2Stats(struct cache_stats_t *stats, char rw) {
	if (rw == READ) {
		stats->l2_read_misses = stats->l2_read_misses + 1;
	} else {
		stats->l2_write_misses = stats->l2_write_misses + 1;
	}
}


void updateL1Stats(struct cache_stats_t *stats, char rw) {
	if (rw == READ) {
		stats->l1_read_misses = stats->l1_read_misses + 1;
	} else {
		stats->l1_write_misses = stats->l1_write_misses + 1;
	}
}


void updateInitialStats(char rw, struct cache_stats_t *stats) {
	if (rw == READ) {
		stats->reads = stats->reads + 1;
	} else {
		stats->writes = stats->writes + 1;
	}

	mainCounter = mainCounter + 1;
	stats->accesses = stats->accesses + 1;
}



void L1HIT(volatile block* L1BLOCK, char rw, uint64_t addressIndex1, uint64_t addressTag1) {
	uint64_t addressIndex2 = convert_index(addressTag1, addressIndex1, cacheConfig.C1, cacheConfig.C2, cacheConfig.B, cacheConfig.S);
	uint64_t addressTag2 = convert_tag(addressTag1, addressIndex1, cacheConfig.C1, cacheConfig.C2, cacheConfig.B, cacheConfig.S);
	unsigned int blockPerSet = 1 << cacheConfig.S;

	if (rw == WRITE) {
		L1BLOCK->dirty = 1;
	}
	for (unsigned int i = 0; i < blockPerSet; i++) {
		block *L2BLOCK = cache2 + addressIndex2*blockPerSet + i;
		if (L2BLOCK->tag == addressTag2 && L2BLOCK->valid > 0) {
			cache2[addressIndex2*blockPerSet + i].counter = mainCounter;
			if (rw == WRITE) {
				cache2[addressIndex2*blockPerSet + i].dirty = 1;
			}
			break;
		}
	}

}



/**
 * Subroutine for freeing up memory, and performing any final calculations before the statistics
 * are outputed by the driver
 */
void cache_cleanup (struct cache_stats_t *stats)
{
	free(cache1);
	free(cache2);

	stats->read_misses = stats->l1_read_misses + stats->l2_read_misses;
	stats->write_misses = stats->l1_write_misses + stats->l2_write_misses;
	stats->misses = stats->read_misses + stats->write_misses;

	stats->l1_miss_rate = ((double)stats->l1_read_misses + (double)stats->l1_write_misses) / (double)stats->accesses;
	stats->l2_miss_rate = ((double)stats->l2_read_misses + (double)stats->l2_write_misses) / ((double)stats->l1_read_misses + (double)stats->l1_write_misses);
	stats->miss_rate = (double)stats->misses / (double)stats->accesses;

	stats->l2_avg_access_time = (double)stats->l2_access_time + stats->l2_miss_rate*stats->memory_access_time;
	stats->avg_access_time = (double)stats->l1_access_time + stats->l1_miss_rate*stats->l2_avg_access_time;
}

/**
 * Subroutine to compute the Tag of a given address based on the parameters passed in
 *
 * @param address The address whose tag is to be computed
 * @param C The size of the cache in bits (i.e. Size of cache is 2^C)
 * @param B The size of the cache block in bits (i.e. Size of block is 2^B)
 * @param S The set associativity of the cache in bits (i.e. Set-Associativity is 2^S)
 * 
 * @return The computed tag
 */
static uint64_t get_tag(uint64_t address, uint64_t C, uint64_t B, uint64_t S)
{
	return address >> (C - S);
}

/**
 * Subroutine to compute the Index of a given address based on the parameters passed in
 *
 * @param address The address whose tag is to be computed
 * @param C The size of the cache in bits (i.e. Size of cache is 2^C)
 * @param B The size of the cache block in bits (i.e. Size of block is 2^B)
 * @param S The set associativity of the cache in bits (i.e. Set-Associativity is 2^S)
 *
 * @return The computed index
 */
static uint64_t get_index(uint64_t address, uint64_t C, uint64_t B, uint64_t S)
{
	return (address >> B) & ((1 << (C-B-S))-1);
}


/**** DO NOT MODIFY CODE BELOW THIS LINE UNLESS YOU ARE ABSOLUTELY SURE OF WHAT YOU ARE DOING ****/

/*
	Note:   The below functions will be useful in converting the L1 tag and index into corresponding L2
			tag and index. These should be used when you are evicitng a block from the L1 cache, and
			you need to update the block in L2 cache that corresponds to the evicted block.

			The newly added functions will be useful for converting L2 indecies ang tags into the corresponding
			L1 index and tags. Make sure to understand how they are working.
*/

/**
 * This function converts the tag stored in an L1 block and the index of that L1 block into corresponding
 * tag of the L2 block
 *
 * @param tag The tag that needs to be converted (i.e. L1 tag)
 * @param index The index of the L1 cache (i.e. The index from which the tag was found)
 * @param C1 The size of the L1 cache in bits
 * @param C2 The size of the l2 cache in bits
 * @param B The size of the block in bits
 * @param S The set associativity of the L2 cache
 */
static uint64_t convert_tag(uint64_t tag, uint64_t index, uint64_t C1, uint64_t C2, uint64_t B, uint64_t S)
{
	uint64_t reconstructed_address = (tag << (C1 - B)) | index;
	return reconstructed_address >> (C2 - B - S);
}

/**
 * This function converts the tag stored in an L1 block and the index of that L1 block into corresponding
 * index of the L2 block
 *
 * @param tag The tag stored in the L1 index
 * @param index The index of the L1 cache (i.e. The index from which the tag was found)
 * @param C1 The size of the L1 cache in bits
 * @param C2 The size of the l2 cache in bits
 * @param B The size of the block in bits
 * @param S The set associativity of the L2 cache
 */
static uint64_t convert_index(uint64_t tag, uint64_t index, uint64_t C1, uint64_t C2, uint64_t B,  uint64_t S)
{
	// Reconstructed address without the block offset bits
	uint64_t reconstructed_address = (tag << (C1 - B)) | index;
	// Create index mask for L2 without including the block offset bits
	return reconstructed_address & ((1 << (C2 - S - B)) - 1);
}

/**
 * This function converts the tag stored in an L2 block and the index of that L2 block into corresponding
 * tag of the L1 cache
 *
 * @param l2_tag The L2 tag
 * @param l2_index The index of the L2 block
 * @param C1 The size of the L1 cache in bits
 * @param C2 The size of the l2 cache in bits
 * @param B The size of the block in bits
 * @param S The set associativity of the L2 cache
 * @return The L1 tag linked to the L2 index and tag
 */
static uint64_t convert_tag_l1(uint64_t l2_tag, uint64_t l2_index, uint64_t C1, uint64_t C2, uint64_t B, uint64_t S) {
	uint64_t reconstructed_address = (l2_tag << (C2 - B - S)) | l2_index;
	return reconstructed_address >> (C1 - B);
}

/**
 * This function converts the tag stored in an L2 block and the index of that L2 block into corresponding
 * index of the L1 block
 *
 * @param l2_tag The L2 tag
 * @param l2_index The index of the L2 block
 * @param C1 The size of the L1 cache in bits
 * @param C2 The size of the l2 cache in bits
 * @param B The size of the block in bits
 * @param S The set associativity of the L2 cache
 * @return The L1 index of the L2 block
 */
static uint64_t convert_index_l1(uint64_t l2_tag, uint64_t l2_index, uint64_t C1, uint64_t C2, uint64_t B, uint64_t S) {
	uint64_t reconstructed_address = (l2_tag << (C2 - B - S)) | l2_index;
	return reconstructed_address & ((1 << (C1 - B)) - 1);
}
