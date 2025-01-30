//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include "cache.hpp"
#include <math.h>
#include <stdio.h>


//
// TODO:Student Information
//
const char *studentName = "Yung-Hsuan Lin";
const char *studentID   = "A59026378";
const char *email       = "yul275@ucsd.edu";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;      // Number of sets in the I$
uint32_t icacheAssoc;     // Associativity of the I$
uint32_t icacheBlocksize; // Blocksize of the I$
uint32_t icacheHitTime;   // Hit Time of the I$

uint32_t dcacheSets;      // Number of sets in the D$
uint32_t dcacheAssoc;     // Associativity of the D$
uint32_t dcacheBlocksize; // Blocksize of the D$
uint32_t dcacheHitTime;   // Hit Time of the D$

uint32_t l2cacheSets;     // Number of sets in the L2$
uint32_t l2cacheAssoc;    // Associativity of the L2$
uint32_t l2cacheBlocksize;// Blocksize of the L2$
uint32_t l2cacheHitTime;  // Hit Time of the L2$
uint32_t inclusive;       // Indicates if the L2 is inclusive

uint32_t prefetch;        // Indicate if prefetching is enabled

uint32_t memspeed;        // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;       // I$ references
uint64_t icacheMisses;     // I$ misses
uint64_t icachePenalties;  // I$ penalties

uint64_t dcacheRefs;       // D$ references
uint64_t dcacheMisses;     // D$ misses
uint64_t dcachePenalties;  // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

uint64_t compulsory_miss;  // Compulsory misses on all caches
uint64_t other_miss;       // Other misses (Conflict / Capacity miss) on all caches

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//

//
//TODO: Add your Cache data structures here
//

struct Block
{
  uint32_t tagg;
  struct Block *prev, *next;
};

struct Set
{
  uint32_t len;
  Block *first, *last;
};


void Insert(Set* s,  Block *b)
{
  if (s->len == 0)
  {
    s->first = b;
    s->last = b;
  }
  else
  {
    b->next = s->first;
    s->first->prev = b;
    s->first = b;
  }
  (s->len)++;
}

void removeOne(Set* s){
  
  Block *b = s->last;

  if (s->len == 1)
  {
    s->first = NULL;
    s->last = NULL;
  }
  else
  {
    s->last = b->prev;
    s->last->next = NULL;
  }
  
  (s->len)--;
  free(b);
}

Block* removePosition(Set* s, int pos){

  Block *b = s->first;

  if (s->len == 1){
    s->first = NULL;
    s->last = NULL;
  }
  else{
    if (pos == 0)
    {
      s->first = b->next;
      s->first->prev = NULL;
    }
    else if (pos == s->len - 1)
    {
      b = s->last;
      s->last = s->last->prev;
      s->last->next = NULL;
    }
    else
    {
      for(int i = 0; i < pos; i++)
        b = b->next;
      b->prev->next = b->next;
      b->next->prev = b->prev;
    }
  }
  
  b->prev = NULL;
  b->next = NULL;
  (s->len)--;
  return b;
}

Block* createBlock(uint32_t tag)
{
  Block *b = (Block*)malloc(sizeof(Block));
  b->tagg = tag;
  b->prev = NULL;
  b->next = NULL;
  return b;
}

Set *icache;
Set *dcache;
Set *l2cache;

uint32_t ic_bo_len;
uint32_t dc_bo_len;
uint32_t l2_bo_len;
uint32_t ic_index_len;
uint32_t dc_index_len;
uint32_t l2_index_len;

uint32_t ic_index_mask;
uint32_t dc_index_mask;
uint32_t l2_index_mask;

// stride prefetcher
uint32_t* pc_tag;
uint32_t* prev_addr;
uint32_t* stride;
uint32_t* state;
int entries = 64;
int length;
int cnt;

//------------------------------------//
//          Cache Functions           //
//------------------------------------//

// Initialize the Cache Hierarchy
//
void
init_cache()
{
  // Initialize cache stats
  icacheRefs        = 0;
  icacheMisses      = 0;
  icachePenalties   = 0;
  dcacheRefs        = 0;
  dcacheMisses      = 0;
  dcachePenalties   = 0;
  l2cacheRefs       = 0;
  l2cacheMisses     = 0;
  l2cachePenalties  = 0;

  compulsory_miss = 0;
  other_miss = 0;
  
  //
  //TODO: Initialize Cache Simulator Data Structures
  //
  icache = (Set*)malloc(sizeof(Set) * icacheSets);
  dcache = (Set*)malloc(sizeof(Set) * dcacheSets);
  l2cache = (Set*)malloc(sizeof(Set) * l2cacheSets);


  for(int i = 0; i < icacheSets; i++)
  {
    icache[i].len = 0;
    icache[i].first = NULL;
    icache[i].last = NULL;
  }
  for(int i = 0; i < dcacheSets; i++)
  {
    dcache[i].len = 0;
    dcache[i].first = NULL;
    dcache[i].last = NULL;
  }
  for(int i = 0; i < l2cacheSets; i++)
  {
    l2cache[i].len = 0;
    l2cache[i].first = NULL;
    l2cache[i].last = NULL;
  }

  pc_tag = (uint32_t*)malloc(sizeof(uint32_t) * entries);
  prev_addr = (uint32_t*)malloc(sizeof(uint32_t) * entries);
  stride = (uint32_t*)malloc(sizeof(uint32_t) * entries);
  state = (uint32_t*)malloc(sizeof(uint32_t) * entries);
  length = 0;
  cnt = 0;

  for(int i = 0; i < entries; i++)
  {
    pc_tag[i] = 0;
    prev_addr[i] = 0;
    stride[i] = 0;
    state[i] = 0;
  }
  

  ic_bo_len = log2(icacheBlocksize);
  if ((1 << ic_bo_len) != icacheBlocksize)
    ic_bo_len++;
  dc_bo_len = log2(dcacheBlocksize);
  if ((1 << dc_bo_len) != dcacheBlocksize)
    dc_bo_len++;
  l2_bo_len = log2(l2cacheBlocksize);
  if ((1 << l2_bo_len) != l2cacheBlocksize)
    l2_bo_len++;
  ic_index_len = log2(icacheSets);
  if ((1 << ic_index_len) != icacheSets)
    ic_index_len++;
  dc_index_len = log2(dcacheSets);
  if ((1 << dc_index_len) != dcacheSets)
    dc_index_len++;
  l2_index_len = log2(l2cacheSets);
  if ((1 << l2_index_len) != l2cacheSets)
    l2_index_len++;

  ic_index_mask = (1 << ic_index_len) - 1;
  ic_index_mask = ic_index_mask << ic_bo_len;
  dc_index_mask = (1 << dc_index_len) - 1;
  dc_index_mask = dc_index_mask << dc_bo_len;
  l2_index_mask = (1 << l2_index_len) - 1;
  l2_index_mask = l2_index_mask << l2_bo_len;
}

// Clean Up the Cache Hierarchy
//
void
clean_cache()
{
  //
  //TODO: Clean Up Cache Simulator Data Structures
  //
  free(icache);
  free(dcache);
  free(l2cache);
  free(pc_tag);
  free(prev_addr);
  free(stride);
  free(state);
}

// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
icache_access(uint32_t addr)
{
  //
  //TODO: Implement I$
  //

  uint32_t blockoffset = addr % icacheBlocksize;
  uint32_t index = (addr & ic_index_mask) >> ic_bo_len;
  uint32_t tag = addr >> (ic_index_len + ic_bo_len);


  icacheRefs++;
  if (icache[index].len){
    Block *target = icache[index].first;
    for(int i = 0; i < icache[index].len; i++){
      if(target->tagg == tag)
      {
        Block *b = removePosition(&icache[index], i);
        Insert(&icache[index],  b);
        return icacheHitTime;
      }
      target = target->next;
    }
  }

  if (icache[index].len == 0)
    compulsory_miss++;
  else
    other_miss++;
  uint32_t l2time = l2cache_access(addr);
  icacheMisses++;
  icachePenalties += l2time;
  Block *bb = createBlock(tag);
  if(icache[index].len == icacheAssoc)
    removeOne(&icache[index]);
  Insert(&icache[index],  bb);
  
  return icacheHitTime + l2time;
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
dcache_access(uint32_t addr)
{
  //
  //TODO: Implement D$
  //
  uint32_t blockoffset = addr % dcacheBlocksize;
  uint32_t index = (addr & dc_index_mask) >> dc_bo_len;
  uint32_t tag = addr >> (dc_index_len + dc_bo_len);

  dcacheRefs++;
  if (dcache[index].len){
    Block *target = dcache[index].first;
    for(int i = 0; i < dcache[index].len; i++){
      if(target->tagg == tag)
      {
        Block *b = removePosition(&dcache[index], i);
        Insert(&dcache[index], b);
        return dcacheHitTime;
      }
      target = target->next;
    }
  }

  if (dcache[index].len == 0)
    compulsory_miss++;
  else
    other_miss++;
  uint32_t l2time = l2cache_access(addr);
  dcacheMisses++;
  dcachePenalties += l2time;
  Block *bb = createBlock(tag);
  if(dcache[index].len == dcacheAssoc)
    removeOne(&dcache[index]);
  Insert(&dcache[index],  bb);
  
  return dcacheHitTime + l2time;
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  //
  //TODO: Implement L2$
  //
  uint32_t blockoffset = addr % l2cacheBlocksize;
  uint32_t index = (addr & l2_index_mask) >> l2_bo_len;
  uint32_t tag = addr >> (l2_index_len + l2_bo_len);

  l2cacheRefs++;
  if (l2cache[index].len){
    Block *target = l2cache[index].first;
    for(int i = 0; i < l2cache[index].len; i++){
      if(target->tagg == tag)
      {
        Block *b = removePosition(&l2cache[index], i);
        Insert(&l2cache[index],  b);
        return l2cacheHitTime;
      }
      target = target->next;
    }
  }

  if (l2cache[index].len == 0)
    compulsory_miss++;
  else
    other_miss++;
  l2cacheMisses++;
  l2cachePenalties += memspeed;
  Block *bb = createBlock(tag);
  if(l2cache[index].len == l2cacheAssoc)
    removeOne(&l2cache[index]);
  Insert(&l2cache[index],  bb);

  return l2cacheHitTime + memspeed;
}


// Predict an address to prefetch on icache with the information of last icache access:
// 'pc':     Program Counter of the instruction of last icache access
// 'addr':   Accessed Address of last icache access
// 'r_or_w': Read/Write of last icache access
uint32_t
icache_prefetch_addr(uint32_t pc, uint32_t addr, char r_or_w)
{
  return addr + icacheBlocksize; // Next line prefetching
  //
  //TODO: Implement a better prefetching strategy
  //
  

}

// Predict an address to prefetch on dcache with the information of last dcache access:
// 'pc':     Program Counter of the instruction of last dcache access
// 'addr':   Accessed Address of last dcache access
// 'r_or_w': Read/Write of last dcache access
uint32_t
dcache_prefetch_addr(uint32_t pc, uint32_t addr, char r_or_w)
{
  //return addr + dcacheBlocksize; // Next line prefetching
  //
  //TODO: Implement a better prefetching strategy
  //
  if (length == 0){
    pc_tag[0] = pc;
    prev_addr[0] = addr;
    stride[0] = 0;
    state[0] = 2;
    length++;
    return addr + dcacheBlocksize;
  }

  int i = 0;
  for (i = 0; i < length; i++){
    if (pc_tag[i] == pc){
      if (state[i] == 3){
        uint32_t target_addr;
        if (addr - prev_addr[i] == stride[i]){
          prev_addr[i] = addr;
          target_addr = prev_addr[i] + stride[i];
          return target_addr;
        }
        else{
          prev_addr[i] = addr;
          target_addr = prev_addr[i] + stride[i];
          state[i] = 2;
          return addr + dcacheBlocksize;
        }
      }
      else if (state[i] == 2){
        uint32_t target_addr;
        if (addr - prev_addr[i] == stride[i]){
          prev_addr[i] = addr;
          target_addr = prev_addr[i] + stride[i];
          state[i] = 3;
        }
        else{
          stride[i] = addr - prev_addr[i];
          prev_addr[i] = addr;
          target_addr = prev_addr[i] + stride[i];
          state[i] = 1;
        }
        return addr + dcacheBlocksize;
      }
      else if (state[i] == 1){
        uint32_t target_addr;
        if (addr - prev_addr[i] == stride[i]){
          prev_addr[i] = addr;
          target_addr = prev_addr[i] + stride[i];
          state[i] = 3;
        }
        else{
          stride[i] = addr - prev_addr[i];
          prev_addr[i] = addr;
          target_addr = prev_addr[i] + stride[i];
          state[i] = 0;
        }
        
        return addr + dcacheBlocksize;
      }
      else{
        uint32_t target_addr;
        if (addr - prev_addr[i] == stride[i]){
          prev_addr[i] = addr;
          target_addr = prev_addr[i] + stride[i];
          state[i] = 1;
        }
        else{
          stride[i] = addr - prev_addr[i];
          prev_addr[i] = target_addr;
          target_addr = prev_addr[i] + stride[i];
          state[i] = 0;
        }
        
        return addr + dcacheBlocksize;
      }
    }
  }
  if (i == entries - 1){
    pc_tag[cnt] = pc;
    prev_addr[cnt] = addr;
    stride[cnt] = 0;
    state[cnt] = 2;
    cnt = (cnt == entries - 1) ? 0 : cnt+1;
    return addr + dcacheBlocksize;
  }
  else{
    pc_tag[i+1] = pc;
    prev_addr[i+1] = addr;
    stride[i+1] = 0;
    state[i+1] = 2;
    length++;
    return addr + dcacheBlocksize;
  }
  
}

// Perform a prefetch operation to I$ for the address 'addr'
void
icache_prefetch(uint32_t addr)
{
  //
  //TODO: Implement I$ prefetch operation
  //
  uint32_t ic_index = (addr & ic_index_mask) >> ic_bo_len;
  uint32_t ic_tag = addr >> (ic_index_len + ic_bo_len);
  uint32_t l2_index = (addr & l2_index_mask) >> l2_bo_len;
  uint32_t l2_tag = addr >> (l2_index_len + l2_bo_len);

  Block *target1 = icache[ic_index].first;
  for(int i = 0; i < icache[ic_index].len; i++){
    if(target1->tagg == ic_tag)
    {
      return;
    }
    target1 = target1->next;
  }
  Block *b1 = createBlock(ic_tag);
  if(icache[ic_index].len == icacheAssoc)
    removeOne(&icache[ic_index]);
  Insert(&icache[ic_index],  b1);

  Block *target2 = l2cache[l2_index].first;
    for(int i = 0; i < l2cache[l2_index].len; i++){
      if(target2->tagg == l2_tag)
      {
        return;
      }
      target2 = target2->next;
    }
  Block *b2 = createBlock(l2_tag);
  if(l2cache[l2_index].len == l2cacheAssoc)
    removeOne(&l2cache[l2_index]);
  Insert(&l2cache[l2_index],  b2);
}

// Perform a prefetch operation to D$ for the address 'addr'
void
dcache_prefetch(uint32_t addr)
{
  //
  //TODO: Implement D$ prefetch operation
  //
  uint32_t dc_index = (addr & dc_index_mask) >> dc_bo_len;
  uint32_t dc_tag = addr >> (dc_index_len + dc_bo_len);
  uint32_t l2_index = (addr & l2_index_mask) >> l2_bo_len;
  uint32_t l2_tag = addr >> (l2_index_len + l2_bo_len);

  Block *target1 = dcache[dc_index].first;
  for(int i = 0; i < dcache[dc_index].len; i++){
    if(target1->tagg == dc_tag)
    {
      return;
    }
    target1 = target1->next;
  }
  Block *b1 = createBlock(dc_tag);
  if(dcache[dc_index].len == dcacheAssoc)
    removeOne(&dcache[dc_index]);
  Insert(&dcache[dc_index],  b1);

  Block *target2 = l2cache[l2_index].first;
    for(int i = 0; i < l2cache[l2_index].len; i++){
      if(target2->tagg == l2_tag)
      {
        return;
      }
      target2 = target2->next;
    }
  Block *b2 = createBlock(l2_tag);
  if(l2cache[l2_index].len == l2cacheAssoc)
    removeOne(&l2cache[l2_index]);
  Insert(&l2cache[l2_index],  b2);
}
