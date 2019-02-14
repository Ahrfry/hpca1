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
	cache_address_t addr;
	bool vld;
	bool dirty;
	uint64_t LRU;
}entry_t;

typedef struct set{
	vector<entry_t> slot;
	uint64_t size;
	uint64_t LRU;
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
	
	
	//cout<<"Entenring Init"<<endl;
	/*	
	uint64_t arr[7];
	return_next_block(0x7fe8d76f8bc8 , 30 , arr);
	
	for(int i = 0; i<30 ; i++){
		
		cout<<"0x"<< hex << arr[i]<<" R"<<endl;
	}
	*/
	conf_g = conf;
	int sets = set_count(conf->c , conf->b , conf->s);
	int blocs = blocks_per_set(conf->s);
	
	for(int i = 0; i<sets; i++){
		set_t set_temp;
		set_temp.size = (uint64_t) blocs;
		for(int j=0; j<blocs; j++){
			entry_t en;
			en.vld = false;
			en.dirty = false;
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
		set_temp.LRU = 0;
		for(int j=0; j<blocs; j++){
			entry_t en;
			en.vld = false;
			en.LRU = (uint64_t) j;
			set_temp.slot.push_back(en);
		}

		L2.push_back(set_temp);
	}

	//cout<<"Exiting Init"<<endl;
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



uint64_t return_lru(uint64_t index , set_t &set){
	uint64_t lru;
	//cout<<"set_size "<<set.size<<endl;	
	
	for(uint64_t i = 0; i < set.size; i++){
		if(!set.slot[i].vld){
			return i;
		}
	}		

	for(uint64_t i = 0; i < set.size; i++){
		
		
		if(set.slot[i].LRU == 0){
			lru = i;
			break;
		}
		
	}	
					
	return lru;
}

void set_access(set_t &set, uint64_t index, uint64_t slot_number, char rw){
	
	uint64_t curr_lru = set.slot[slot_number].LRU;
	
	//cout<<"entering set_access()"<<endl;
	//set LRU to max
	set.slot[slot_number].LRU = set.size - one; 
	
	//cout<<"setting lru to "<<set.slot[slot_number].LRU<<" slot at  "<<slot_number <<" at index "<<index<<endl; 
	
	if(rw == 'W'){
		set.slot[slot_number].dirty = true;
	}
	//decrease all other LRUs in the set
	for(uint64_t i = 0 ; i < (set.size); i++){
		
		if(set.slot[i].LRU > curr_lru && i != slot_number){
			set.slot[i].LRU = set.slot[i].LRU - one;
			//cout<<"setting lru "<<i<<" to "<<set.slot[i].LRU<<endl; 
		}

	}
	
	//cout<<"exiting set_access()"<<endl;
}


bool check_cache(set_t &set , cache_address_t cache_addr , char rw){
	
	
	bool hit = false , vld;	
	for(uint64_t i = 0; i< set.size; i++){
	
		vld = set.slot[i].vld;	
		if(vld){
			if(cache_addr.tag == set.slot[i].tag){
				set_access(set, cache_addr.index , i, rw);
				hit = true;
				break;
			}
		}
	}
	

	return hit;
}

//loads and returns an eviction
entry_t load_address(set_t &set, cache_address_t cache_addr){
	
	entry_t evict;
	uint64_t LRU = return_lru(cache_addr.index , set);
	cout<<"L2 cache miss " <<"LRU for eviction "<<LRU<<" age "<<set.slot[LRU].LRU<<endl<<flush;
	
	evict = set.slot[LRU];	
	
	set.slot[LRU].tag = cache_addr.tag;
	set.slot[LRU].vld = true;
	set.slot[LRU].addr = cache_addr;

	//L1[cache_addr.index].slot[LRU].LRU = 0;
	//set_access(L2[cache_addr2.index] , cache_addr.index , LRU , rw);

	return evict;
}

void return_next_block(uint64_t address , int k , cache_address_t arr[]){
	
	uint64_t mask = 0x3F;	
	address = address & ~(mask);
	cache_address_t cache_addr;
	for(int i = 0; i<k ; i++){
		
		address = address + 0x40;
		parse_address(&cache_addr , address , conf_g->C , conf_g->b, conf_g->S);
		arr[i] = cache_addr;
	}
}



void cache_access(uint64_t addr, char rw, struct cache_stats_t *stats)
{
	
	stats->num_accesses++;	
	//cout<<"Entering Cache Access"<<endl;
	cache_address_t cache_addr , cache_addr2;

	parse_address(&cache_addr , addr , conf_g->c , conf_g->b, conf_g->s);
	parse_address(&cache_addr2 , addr , conf_g->C , conf_g->b, conf_g->S);
	
	bool  hit=false;
	
		
	hit = check_cache(L1[cache_addr.index] , cache_addr , rw);	
	
	entry_t evict; 	
	//if not in L1, search L2
	if(!hit){	
		

		hit = check_cache(L2[cache_addr2.index] , cache_addr2,  rw);
		
		//not in L2 we do prefecthing for k blocks
		if(!hit){
			int k = (int) conf_g->k;
			stats->num_misses_l2++;
			if(k == 0){
				
				evict = load_address(L2[cache_addr2.index] , cache_addr2);	
				
				if(evict.dirty){		
					stats->num_write_backs++;	
					//cout<<"Writing Dirty Block To Memory "<<endl;
				}
			}else{
			
				cache_address_t arr[10];

				return_next_block(addr , k , arr);
				
				for(int i=0; i<k; i++){	
					if(!check_cache(L2[arr[i].index] , arr[i] , rw)){	
						evict = load_address(L2[arr[i].index] , arr[i]);	
						
						if(evict.dirty){		
							stats->num_write_backs++;
						}
					}
				}
			}	
		}else{
			evict = load_address(L1[cache_addr.index] , cache_addr);
			//if dirty in L1 and block not present in L2 write to L2
			if(evict.vld && evict.dirty && !check_cache(L2[cache_addr.index] , cache_addr , rw)){		
				//cout<<"Writing Dirty Block To L2 "<<endl;
				evict = load_address(L2[cache_addr2.index] , cache_addr);
				//If dirty in L2 write to memory
				if(evict.dirty){
					
					stats->num_write_backs++;
					//cout<<"Writing Dirty Block To Memory "<<endl;
				}
			}
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
