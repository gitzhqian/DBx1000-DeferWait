#include "global.h"
#include "index_hash.h"
#include "mem_alloc.h"
#include "table.h"

RC IndexHash::init(uint64_t bucket_cnt, int part_cnt) {
  _bucket_cnt = bucket_cnt;
  _bucket_cnt_per_part = bucket_cnt / part_cnt;
  _buckets = new BucketHeader * [part_cnt];
  for (int i = 0; i < part_cnt; i++) {
    _buckets[i] = (BucketHeader *) _mm_malloc(sizeof(BucketHeader) * _bucket_cnt_per_part, 64);
    for (uint32_t n = 0; n < _bucket_cnt_per_part; n ++)
      _buckets[i][n].init();
  }
  return RCOK;
}

RC
IndexHash::init(int part_cnt, table_t * table, uint64_t bucket_cnt) {
  init(bucket_cnt, part_cnt);
  this->table = table;
  return RCOK;
}

bool IndexHash::index_exist(idx_key_t key) {
  assert(false);
  return false;
}

void
IndexHash::get_latch(BucketHeader * bucket) {
  while (!ATOM_CAS(bucket->locked, false, true)) {}
}

void
IndexHash::release_latch(BucketHeader * bucket) {
    bool ok = ATOM_CAS(bucket->locked, true, false);
    assert(ok);
  // XXX(zhihan): change to read/write lock
  //pthread_rwlock_unlock(bucket->rwlock);
}

void
IndexHash::get_latch(BucketHeader * bucket, access_t access) {
  while (!ATOM_CAS(bucket->locked, false, true)) {}
  /*
  // XXX(zhihan): rwlock
  if (access == RD)
    pthread_rwlock_rdlock(bucket->rwlock);
  else
    pthread_rwlock_wrlock(bucket->rwlock);
  */
}

RC IndexHash::index_insert(idx_key_t key, itemid_t * item, int part_id) {
  RC rc = RCOK;
  uint64_t bkt_idx = hash(key);
  assert(bkt_idx < _bucket_cnt_per_part);
  BucketHeader * cur_bkt = &_buckets[part_id][bkt_idx];
  // 1. get the ex latch
  get_latch(cur_bkt, WR);
  // 2. update the latch list
  cur_bkt->insert_item(key, item, part_id);
  // 3. release the latch
  release_latch(cur_bkt);
  return rc;
}

RC IndexHash::index_read(idx_key_t key, itemid_t * &item, int part_id) {
  //TODO(zhihan): take read lock
  uint64_t bkt_idx = hash(key);
  assert(bkt_idx < _bucket_cnt_per_part);
  BucketHeader * cur_bkt = &_buckets[part_id][bkt_idx];
  RC rc = RCOK;
  // 1. get the sh latch
  //get_latch(cur_bkt, RD);
  cur_bkt->read_item(key, item, table->get_table_name());
  // 3. release the latch
  //release_latch(cur_bkt);
  return rc;

}

RC IndexHash::index_read(idx_key_t key, itemid_t * &item,
                         int part_id, int thd_id) {
  uint64_t bkt_idx = hash(key);
  assert(bkt_idx < _bucket_cnt_per_part);
  BucketHeader * cur_bkt = &_buckets[part_id][bkt_idx];
  RC rc = RCOK;
  // 1. get the sh latch
  //get_latch(cur_bkt, RD);
  cur_bkt->read_item(key, item, table->get_table_name());
  // 3. release the latch
  //release_latch(cur_bkt);
  return rc;
}

/************** BucketHeader Operations ******************/

void BucketHeader::init() {
  node_cnt = 0;
  first_node = NULL;
  locked = false;
  // XXX(zhihan): init another rw lock
  rwlock = new pthread_rwlock_t;
  pthread_rwlock_init(rwlock, NULL);
}

void BucketHeader::insert_item(idx_key_t key,
                               itemid_t * item,
                               int part_id)
{
  BucketNode * cur_node = first_node;
  BucketNode * prev_node = NULL;
  while (cur_node != NULL) {
    if (cur_node->key == key)
      break;
    prev_node = cur_node;
    cur_node = cur_node->next;
  }
  if (cur_node == NULL) {
    BucketNode * new_node = (BucketNode *)
        mem_allocator.alloc(sizeof(BucketNode), part_id );
    new_node->init(key);
    new_node->items = item;
    if (prev_node != NULL) {
      new_node->next = prev_node->next;
      prev_node->next = new_node;
    } else {
      new_node->next = first_node;
      first_node = new_node;
    }
  } else {
    item->next = cur_node->items;
    cur_node->items = item;
  }
}

void BucketHeader::read_item(idx_key_t key, itemid_t * &item, const char * tname)
{
  BucketNode * cur_node = first_node;
  while (cur_node != NULL) {
    if (cur_node->key == key)
      break;
    cur_node = cur_node->next;
  }
  //ADD: cur_node can be NULL
  if(!cur_node) {
      item = NULL;
      return;
  }

  M_ASSERT(cur_node->key == key, "Key does not exist!");
  item = cur_node->items;
}
