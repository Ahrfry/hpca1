#include <stdio.h>
#include <cstring>
#include "cache.hpp"
#include <iostream>
#include <vector>
using namespace std;


typedef struct cache_address {
	uint64_t tag;
	uint64_t index;
	uint64_t offset;
} cache_address_t;

typedef struct entry{
	uint64_t tag;
	bool vld;
	bool dirty;
	uint64_t LRU;
}entry_t;

typedef struct set{
	vector<entry_t> slot;
	uint64_t size;
}set_t;

vector<set_t> L1;	
vector<set_t> L2;	
vector<entry_t> VC;
uint64_t one = 1;

static int set_count(uint64_t c , uint64_t b , uint64_t s)
{
	return 1 << (c - b - s);
}

static int blocks_per_set(uint64_t s)
{
	if (!s)
		return 0;
	return 1 << s;
}

//current cache config, set upon unitialization of the cache, used until we access the last address
struct cache_config_t *conf_g;

void cache_init(struct cache_config_t *conf)
{
	conf_g = conf;
	int sets = set_count(conf->c , conf->b , conf->s);
	int blocs = blocks_per_set(conf->s);
	
	for(int i = 0; i<sets; i++){
		set_t set_temp;
		set_temp.size = (uint64_t) blocs;
		for(int j=0; j<blocs; j++){
			entry_t en;
			en.vld = false;
			en.LRU = (uint64_t) j;
			set_temp.slot.push_back(en);
		}

		L1.push_back(set_temp);
	}

	sets = set_count(conf->C , conf->b , conf->S);
	blocs = blocks_per_set(conf->S);

	for(int i = 0; i<sets; i++){
		set_t set_temp;
		set_temp.size = (uint64_t) blocs;
		for(int j=0; j<blocs; j++){
			entry_t en;
			en.vld = false;
			en.LRU = (uint64_t) j;
			set_temp.slot.push_back(en);
		}

		L2.push_back(set_temp);
	}
}


static void parse_address(cache_address_t *d_addr, uint64_t addr, uint64_t c , uint64_t b, uint64_t s)
{
	// can't do bit-ops on a pointer
	memset(d_addr, 0, sizeof(*d_addr));
	d_addr->tag = addr >> (c - s);
	uint64_t mask = (1u << (c - s)) - 1u;
	d_addr->index = (addr & mask) >> b;
	mask = (1u << b) - 1u;
	d_addr->offset = addr & mask;	
}

uint64_t return_lru(uint64_t index , vector<set_t> LAYER){
	uint64_t lru;
	
	for(uint64_t i = 0; i < LAYER[index].size; i++){
		
		//cout<<"checking lru "<<LAYER[index].slot[i].LRU <<endl;
		
		if(LAYER[index].slot[i].LRU == 0){
			lru = i;
			break;
		}
		
	
	}	
					
	//cout<<"returned lru "<<lru <<endl;
	return lru;
}

void set_access(set_t &set, uint64_t index, uint64_t slot_number, char rw){
	
	uint64_t curr_lru = set.slot[slot_number].LRU;
	//set LRU to max
	set.slot[slot_number].LRU = set.size - one; 
	
	//cout<<"setting lru to "<<set.slot[slot_number].LRU<<" slot at  "<<slot_number <<" at index "<<index<<endl; 
	
	if(rw == 'W'){
		set.slot[slot_number].dirty = true;
	}
	if(set.size > 20){
		cout<<"something went wrong"<<endl;
		return;
	}
	//decrease all other LRUs in the set
	for(uint64_t i = curr_lru ; i < (set.size); i++){
		
		if(set.slot[i].LRU > 0 && i != slot_number){
			set.slot[i].LRU = set.slot[i].LRU - one;
			//cout<<"setting lru "<<i<<" to "<<set.slot[i].LRU<<endl; 
		}

	}

}

void cache_access(uint64_t addr, char rw, struct cache_stats_t *stats)
{
	cache_address_t cache_addr , cache_addr2;

	parse_address(&cache_addr , addr , conf_g->c , conf_g->b, conf_g->s);
	parse_address(&cache_addr2 , addr , conf_g->C , conf_g->b, conf_g->S);
	
	bool vld, hit=false;
	
	uint64_t set_size = (uint64_t) blocks_per_set(conf_g->s);
	
	for(uint64_t i = 0; i<set_size; i++){
	
		vld = L1[cache_addr.index].slot[i].vld;	
		if(vld){
			if(cache_addr.tag == L1[cache_addr.index].slot[i].tag){
				set_access(L1[cache_addr.index] , cache_addr.index , i, rw);
			}
		}
				
		
	}
	//if not in L1, search L2
	if(!hit){	
		set_size = (uint64_t) blocks_per_set(conf_g->S);
		for(uint64_t i = 0; i<set_size; i++){
		
			vld = L2[cache_addr2.index].slot[i].vld;	
			if(vld){
				if(cache_addr2.tag == L2[cache_addr2.index].slot[i].tag){
					set_access(L2[cache_addr2.index], cache_addr.index , i, rw);
				}
			}
		}
		//not in L2
		if(!hit){
			uint64_t LRU = return_lru(cache_addr2.index , L2);
			
			cout<<"LRU for eviction "<<LRU<<" age "<<L2[cache_addr2.index].slot[LRU].LRU<<endl<<flush;
			L2[cache_addr2.index].slot[LRU].tag = cache_addr2.tag;
			L2[cache_addr2.index].slot[LRU].vld = true;
			set_access(L2[cache_addr2.index] , cache_addr.index , LRU , rw);
			
			cout<<"set to "<<L2[cache_addr2.index].slot[LRU].LRU<<" slot at "<<LRU<< " at index "<<cache_addr.index<<endl; 
			
		}	
	}
	
	//cout<<"tag "<<cache_addr.tag<<" index "<<cache_addr.index<<" offset "<<cache_addr.offset<<endl;
}


/** @brief Function to free any allocated memory and finalize statistics
 *
 *  @param stats pointer to the cache statistics structure
 *
 */
void cache_cleanup(struct cache_stats_t *stats)
{
	L1.clear();
	L2.clear();
}
