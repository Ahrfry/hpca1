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
	uint64_t raw_addr;
	cache_address_t addr;
	bool vld;
	bool dirty;
	uint64_t LRU;
	bool pf;
	uint64_t pos;
}entry_t;

typedef struct set{
	vector<entry_t> slot;
	uint64_t size;
	uint64_t LRU;
	int layer;
}set_t;

vector<set_t> L1;	
vector<set_t> L2;	
vector<entry_t> VC;
uint64_t one = 1;
struct cache_stats_t *stats_g;
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
		set_temp.layer = 1;
		
		for(int j=0; j<blocs; j++){
			entry_t en;
			en.vld = false;
			en.dirty = false;
			en.pf = false;
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
		set_temp.layer = 2;
		for(int j=0; j<blocs; j++){
			entry_t en;
			en.vld = false;
			en.LRU = (uint64_t) j;
			en.pf = false;
			set_temp.slot.push_back(en);
		}

		L2.push_back(set_temp);
	}

	//cout<<"Exiting Init"<<endl;
}



// 0 indexed. begin is starting index of desired bits
// and k is the number of total bits to grab.
// returns as an int
uint64_t extract(uint64_t value, uint64_t begin, uint64_t k)
{
   
	uint64_t mask = (one << k) - one;
   
	return (value >> begin) & mask;

}

// Return the index from an address
uint64_t getIndex(uint64_t arg , uint64_t C, uint64_t B, uint64_t S) {
	uint64_t index;
	index = extract(arg, B, (C- (B+S)));
	
	return index;
}

// Return the tag from an address
uint64_t getTag(uint64_t arg, uint64_t C, uint64_t S) {
	uint64_t tag;
   	tag = extract(arg, (C-S), (0x40 - (C-S)));
 	return tag;
}

static void parse_address(cache_address_t *d_addr, uint64_t addr, uint64_t c , uint64_t b, uint64_t s)
{
	d_addr->tag = getTag(addr , c , s);
	d_addr->index = getIndex(addr , c , b , s);	
	d_addr->offset = 0x1;
}



uint64_t return_lru(set_t &set){
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

void set_access(set_t &set, uint64_t slot_number, char rw){
	
	uint64_t curr_lru = set.slot[slot_number].LRU;
	
	//set LRU to max
	set.slot[slot_number].LRU = set.size - one; 
	
	
	if(rw == 'W' && set.layer == 1){
		set.slot[slot_number].dirty = true;
	}else{
			
		set.slot[slot_number].dirty = false;
	}
	//decrease all other LRUs in the set
	for(uint64_t i = 0 ; i < (set.size); i++){
		
		if(set.slot[i].LRU >= curr_lru && i != slot_number){
				

			set.slot[i].LRU = set.slot[i].LRU - one;
		}

	}
	
}


uint64_t return_pos(set_t &set , cache_address_t cache_addr){
	
	uint64_t pos;
	bool vld;	
	for(uint64_t i = 0; i< set.size; i++){
	
		vld = set.slot[i].vld;	
		if(vld){
			if(cache_addr.tag == set.slot[i].tag){
				pos = i;
			}
		}
	}
	

	return pos;
}

bool check_cache(set_t &set , cache_address_t cache_addr , char rw , bool update){
	
	
	bool hit = false , vld;	
	for(uint64_t i = 0; i< set.size; i++){
	
		vld = set.slot[i].vld;	
		if(vld){
			if(cache_addr.tag == set.slot[i].tag){
				if(update)
					set_access(set , i, rw);
				hit = true;
				if(set.slot[i].pf){
					stats_g->num_useful_prefetches++;			
					set.slot[i].pf = false;
				}
				break;
			}
		}
	}
	

	return hit;
}

//loads and returns an eviction
entry_t load_address(set_t &set, uint64_t addr,  bool update , char rw , bool pf){
	
	
	entry_t evict;
	uint64_t LRU = return_lru(set);
	//cout<<"L2 cache miss " <<"LRU for eviction "<<LRU<<" age "<<set.slot[LRU].LRU<<endl<<flush;
	
	cache_address_t cache_addr;
	if(set.layer == 1){
			
		parse_address(&cache_addr , addr , conf_g->c , conf_g->b, conf_g->s);
	}else{
		parse_address(&cache_addr , addr , conf_g->C , conf_g->b, conf_g->S);
	}

	evict = set.slot[LRU];	
	
	set.slot[LRU].tag = cache_addr.tag;
	set.slot[LRU].vld = true;
	set.slot[LRU].addr = cache_addr;
	set.slot[LRU].pf = pf;
	set.slot[LRU].raw_addr = addr;
	set.slot[LRU].pos = LRU;
	//if is not a fetch k block we update the access
	if(update){
		set_access(set, LRU, rw);
	}
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
	stats_g = stats;	
	stats->num_accesses++;	
	cache_address_t cache_addr , cache_addr2;

	parse_address(&cache_addr , addr , conf_g->c , conf_g->b, conf_g->s);
	parse_address(&cache_addr2 , addr , conf_g->C , conf_g->b, conf_g->S);
	
	//cout<<"L1 tag "<<getTag(addr , conf_g->c , conf_g->s)<<" index "<<getIndex(addr, conf_g->c, conf_g->b , conf_g->s)<<" offset "<<cache_addr.offset<<endl;
	//cout<<"L2 tag "<<cache_addr2.tag<<" index "<<cache_addr2.index<<" offset "<<cache_addr2.offset<<endl;
		
	bool  hit=false;
		
	hit = check_cache(L1[cache_addr.index] , cache_addr , rw, true);	
	entry_t evict; 
	//if not in L1, search L2
	if(!hit){	
		
		if(rw == 'R'){
			stats->num_misses_reads_l1++;
		}else{
				
			stats->num_misses_writes_l1++;
		}
		
			
		hit = check_cache(L2[cache_addr2.index] , cache_addr2,  rw ,  true);
		
		//in L2 we do prefecthing for k blocks
		if(!hit){
			
			if(rw == 'R'){
				stats->num_misses_reads_l2++;
			}else{
				
				stats->num_misses_writes_l2++;
			}

			evict = load_address(L2[cache_addr2.index] , addr, true , rw , false);	
			if(evict.dirty){
				stats->num_write_backs++;
			}
			
			


		}
		
		evict = load_address(L1[cache_addr.index] , addr, true , rw , false);
		//cout<<"evicting "<<hex<<evict.raw_addr<<endl;	
		if(evict.vld && evict.dirty){
			
			cache_address_t cache_addr_tmp;	
			parse_address(&cache_addr_tmp , evict.raw_addr , conf_g->C , conf_g->b, conf_g->S);
			
			
			hit = check_cache(L2[cache_addr_tmp.index] , cache_addr_tmp ,  rw, false);
			
			if(hit){
				uint64_t pos  = return_pos(L2[cache_addr_tmp.index] , cache_addr_tmp);
				L2[cache_addr_tmp.index].slot[pos].dirty = true;
				cout<<"LRU OF BLOCK "<<pos<<" "<<L2[cache_addr_tmp.index].slot[pos].LRU<<" at index "<<cache_addr_tmp.index<<endl;
			}else{
				cout<<"Else"<<endl;
				stats->num_write_backs++;
				evict = load_address(L2[cache_addr_tmp.index] , evict.raw_addr , false , 'W' , false);	
				
				if(evict.dirty){
					stats->num_write_backs++;
				}
			}
				
		}
			
			
	}
	
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
