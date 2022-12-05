#include "txn.h"
#include "row.h"
#include "wl.h"
#include "ycsb.h"
#include "thread.h"
#include "mem_alloc.h"
#include "occ.h"
#include "table.h"
#include "catalog.h"
#include "index_btree.h"
#include "index_hash.h"
// for info of lock entry
#include "row_lock.h"
#include "row_bamboo.h"
#include "row_sler.h"

void txn_man::init(thread_t * h_thd, workload * h_wl, uint64_t thd_id) {
    this->h_thd = h_thd;
    this->h_wl = h_wl;
    lock_ready = false;
    lock_abort = false;
    timestamp = 0;
#if PF_ABORT
    abort_chain = 0;
#endif
#if CC_ALG == BAMBOO
    commit_barriers = 0;
    //commit_barriers = g_thread_cnt << 2;
    //tmp_barriers = 0;
    //addr_barriers = &(tmp_barriers);
#endif
    ready_part = 0;
    row_cnt = 0;
    wr_cnt = 0;
    insert_cnt = 0;

    // init accesses
    accesses = (Access **) _mm_malloc(sizeof(Access *) * MAX_ROW_PER_TXN, 64);
    for (int i = 0; i < MAX_ROW_PER_TXN; i++)
        accesses[i] = NULL;

#if CC_ALG == SLER
//    write_row_cnt = 0;
    sler_semaphore = 0;
    sler_serial_id = 0;
    status_latch = false;
    serial_id_latch = false;
//    dependency_latch = false;
//    waiting_set_latch = false;
//    semaphore_latch = false;

    bloom_parameters parameters;
    parameters.projected_element_count = 1000;
    parameters.false_positive_probability = 0.0001;

    parameters.compute_optimal_parameters();

    bloom_filter temp_bloom(parameters);
    sler_waiting_set = temp_bloom;

#endif

    num_accesses_alloc = 0;

#if CC_ALG == TICTOC || CC_ALG == SILO
    _pre_abort = (g_params["pre_abort"] == "true");
    if (g_params["validation_lock"] == "no-wait")
        _validation_no_wait = true;
    else if (g_params["validation_lock"] == "waiting")
        _validation_no_wait = false;
    else
        assert(false);
#endif
#if CC_ALG == TICTOC
    _max_wts = 0;
    _write_copy_ptr = (g_params["write_copy_form"] == "ptr");
    _atomic_timestamp = (g_params["atomic_timestamp"] == "true");
#elif CC_ALG == SILO
    _cur_tid = 0;
#elif CC_ALG == IC3
  depqueue = (TxnEntry **) _mm_malloc(sizeof(void *)*THREAD_CNT, 64);
  for (int i = 0; i < THREAD_CNT; i++)
    depqueue[i] = NULL;
  depqueue_sz = 0;
  piece_starttime = 0;
#endif
}

void txn_man::set_txn_id(txnid_t txn_id) {
#if CC_ALG == WOUND_WAIT || CC_ALG == BAMBOO
    lock_abort = false;
    lock_ready = false;
    status = RUNNING;
    #if CC_ALG == BAMBOO
        commit_barriers = 0;
        //commit_barriers = g_thread_cnt << 2;
        //addr_barriers = &(tmp_barriers);
        if (g_last_retire > 0)
            start_ts = get_sys_clock();
    #endif
#endif
#if CC_ALG == IC3
    status = RUNNING;
    depqueue_sz = 0;
#endif
    this->txn_id = txn_id;
#if LATCH == LH_MCSLOCK
    mcs_node = new mcslock::mcs_node();
#endif
}

txnid_t txn_man::get_txn_id() {
    return this->txn_id;
}

workload * txn_man::get_wl() {
    return h_wl;
}

uint64_t txn_man::get_thd_id() {
    return h_thd->get_thd_id();
}

bool txn_man::atomic_set_ts(ts_t ts) {
    if (ATOM_CAS(timestamp, 0, ts)) {
        return true;
    }
    return false;
}

uint64_t txn_man::set_next_ts(int n) {
    if (atomic_set_ts(h_thd->get_next_n_ts(n))) {
        return this->timestamp;
    } else {
        return 0; // fail to set timestamp
    }
}

void txn_man::reassign_ts() {
    this->timestamp = h_thd->get_next_n_ts(1);
}

void txn_man::set_ts(ts_t timestamp) {
    this->timestamp = timestamp;
}

ts_t txn_man::get_ts() {
    return this->timestamp;
}

void txn_man::cleanup(RC rc) {
#if CC_ALG == SLER

    sler_txn_id = 0;
    sler_serial_id = 0;
    sler_semaphore = 0;
    sler_dependency.clear();
    sler_waiting_set.clear();


    row_cnt = 0;
    wr_cnt = 0;
    insert_cnt = 0;
//    write_row_cnt = 0;

    return;

#else
    #if CC_ALG == HEKATON || CC_ALG == IC3
        row_cnt = 0;
        wr_cnt = 0;
        insert_cnt = 0;
        #if CC_ALG == IC3
            access_marker = 0;
        #endif
        return;
    #endif

    // go through accesses and release
    for (int rid = row_cnt - 1; rid >= 0; rid --) {
        #if (CC_ALG == WOUND_WAIT) || (CC_ALG == BAMBOO)
            if (accesses[rid]->orig_row == NULL) {
                continue;
            }
        #endif

        row_t * orig_r = accesses[rid]->orig_row;
        access_t type = accesses[rid]->type;

        #if COMMUTATIVE_OPS
            if (accesses[rid]->com_op != COM_NONE && (rc != Abort)) {
                if (accesses[rid]->com_op == COM_INC)
                    orig_r->inc_value(accesses[rid]->com_col, accesses[rid]->com_val);
                else
                    orig_r->dec_value(accesses[rid]->com_col, accesses[rid]->com_val);
                accesses[rid]->com_op = COM_NONE;
            }
        #endif

        if (type == WR && rc == Abort)
            type = XP;

        #if (CC_ALG == NO_WAIT || CC_ALG == DL_DETECT) && ISOLATION_LEVEL == REPEATABLE_READ
            if (type == RD) {
                accesses[rid]->data = NULL;
                continue;
            }
        #endif

        #if COMMUTATIVE_OPS && !COMMUTATIVE_LATCH
            if (type != CM) {
        #endif

        #if CC_ALG == BAMBOO
            orig_r->return_row(accesses[rid]->lock_entry, rc);
            accesses[rid]->orig_row = NULL;
        #elif CC_ALG == WOUND_WAIT
            orig_r->return_row(type, accesses[rid]->data, accesses[rid]->lock_entry);
            accesses[rid]->orig_row = NULL;
        #elif CC_ALG == DL_DETECT || CC_ALG == NO_WAIT || CC_ALG == WAIT_DIE
            if (ROLL_BACK && type == XP) {
                orig_r->return_row(type, accesses[rid]->orig_data, accesses[rid]->lock_entry);
            } else {
                orig_r->return_row(type, accesses[rid]->data, accesses[rid]->lock_entry);
            }
        #else
            orig_r->return_row(type, this, accesses[rid]->data);
        #endif

        #if COMMUTATIVE_OPS && !COMMUTATIVE_LATCH
            }
        #endif

        #if CC_ALG != TICTOC && (CC_ALG != SILO) && (CC_ALG != WOUND_WAIT) && (CC_ALG!= BAMBOO)
            // invalidate ptr for cc keeping globally visible ptr
            accesses[rid]->data = NULL;
        #endif
    }

    if (rc == Abort) {
        for (UInt32 i = 0; i < insert_cnt; i ++) {
            row_t * row = insert_rows[i];
            assert(g_part_alloc == false);
            #if CC_ALG != HSTORE && CC_ALG != OCC
                mem_allocator.free(row->manager, 0);
            #endif
            row->free_row();
            mem_allocator.free(row, sizeof(row));
        }
    }

    row_cnt = 0;
    wr_cnt = 0;
    insert_cnt = 0;

    #if CC_ALG == DL_DETECT
        dl_detector.clear_dep(get_txn_id());
    #endif

    #if CC_ALG == BAMBOO                // 12-5: make BamBoo support TEST workload
        commit_barriers = 0;
    #endif

#endif
}


#if CC_ALG == BAMBOO || CC_ALG == WOUND_WAIT || CC_ALG == WAIT_DIE || CC_ALG == NO_WAIT || CC_ALG == DL_DETECT
inline
void txn_man::assign_lock_entry(Access * access) {
#if CC_ALG == BAMBOO
    auto lock_entry = (BBLockEntry *) _mm_malloc(sizeof(BBLockEntry), 64);
    new (lock_entry) BBLockEntry(this, access);
#else
    auto lock_entry = (LockEntry *) _mm_malloc(sizeof(LockEntry), 64);
    new (lock_entry) LockEntry(this, access);
#endif
    access->lock_entry = lock_entry;
    //lock_entry->txn = this;
    //lock_entry->access = access;
}
#endif

/**
 * Record this operation in Accesses set and call corresponding get_row() to actually access the row
 * @param row : the row accesses by the txn [Full: with data]
 * @param type : access type
 * @return
 */
row_t * txn_man::get_row(row_t * row, access_t type) {
    if (CC_ALG == HSTORE)
        return row;

    uint64_t starttime = get_sys_clock();
    RC rc = RCOK;

    /**
     * Allocate space for a Access Object.
     */
    if (accesses[row_cnt] == NULL) {
        assert(row_cnt < MAX_ROW_PER_TXN);
        Access *access = (Access *) _mm_malloc(sizeof(Access), 64);

        #if COMMUTATIVE_OPS
                // init
                access->com_op = COM_NONE;
        #endif

        accesses[row_cnt] = access;

        #if (CC_ALG == SILO || CC_ALG == TICTOC)
            access->data = (row_t *) _mm_malloc(sizeof(row_t), 64);
            access->data->init(MAX_TUPLE_SIZE);
            access->orig_data = (row_t *) _mm_malloc(sizeof(row_t), 64);
            access->orig_data->init(MAX_TUPLE_SIZE);
        #elif (CC_ALG == IC3)
            access->data = (row_t *) _mm_malloc(sizeof(row_t), 64);
            access->data->init(MAX_TUPLE_SIZE);
            #if IC3_FIELD_LOCKING
                access->tids = (ts_t *) _mm_malloc(sizeof(ts_t) * MAX_FIELD_SIZE, 64);
            #else
                access->tid = 0;
            #endif
        #elif (CC_ALG == WOUND_WAIT)
            // allocate lock entry as well
            assign_lock_entry(access);
            // for ww and bb, data is a local copy of original row for txn to work on
            access->data = (row_t *) _mm_malloc(sizeof(row_t), 64);
            access->data->init(MAX_TUPLE_SIZE);
        #elif (CC_ALG == BAMBOO)
            // allocate lock entry as well
            assign_lock_entry(access);
            // data is for making local changes before added to retired
            access->data = (row_t *) _mm_malloc(sizeof(row_t), 64);
            access->data->init(MAX_TUPLE_SIZE);
            access->data->table = row->get_table();
            // orig data is for rollback
            access->orig_data = (row_t *) _mm_malloc(sizeof(row_t), 64);
            access->orig_data->init(MAX_TUPLE_SIZE);
            access->orig_data->table = row->get_table();
        #elif (CC_ALG == DL_DETECT || (CC_ALG == NO_WAIT) || (CC_ALG == WAIT_DIE))
            // allocate lock entry as well
            assign_lock_entry(access);
            access->orig_data = (row_t *) _mm_malloc(sizeof(row_t), 64);
            access->orig_data->init(MAX_TUPLE_SIZE);
//        #elif CC_ALG == SLER        //[SLER]: we don't need to allocate the space of data
//            access->data = (row_t *) _mm_malloc(sizeof(row_t), 64);
//            access->data->init(MAX_TUPLE_SIZE);
        #endif

            num_accesses_alloc++;
    }



    /**
     *  Actually access the row.
     */
    //printf("txn-%lu access(%p) row %p at access[%d]\n", txn_id, accesses[row_cnt], row , row_cnt);
    #if (CC_ALG == WOUND_WAIT) || (CC_ALG == BAMBOO)
        rc = row->get_row(type, this, accesses[ row_cnt ]->orig_row, accesses[row_cnt]);
        if (rc == Abort) {
            accesses[row_cnt]->orig_row = NULL;
            return NULL;
        }
    #elif CC_ALG == DL_DETECT || (CC_ALG == NO_WAIT) || (CC_ALG == WAIT_DIE)
        rc = row->get_row(type, this, accesses[ row_cnt ]->data, accesses[row_cnt]);
          if (rc == Abort)
            return NULL;
          accesses[row_cnt]->orig_row = row;
    #elif CC_ALG == IC3
        assert(rc == RCOK);
        // re-initialize read/write sets for the tuple.
        accesses[row_cnt]->rd_accesses = 0;
        accesses[row_cnt]->wr_accesses = 0;
        accesses[row_cnt]->lk_accesses = 0;
        accesses[row_cnt]->data->init_accesses(accesses[row_cnt]);
        accesses[row_cnt]->data->manager = row->manager;
        accesses[row_cnt]->data->table = row->get_table();
        accesses[row_cnt]->data->orig = row;
        accesses[row_cnt]->orig_row = row;
        #if !IC3_FIELD_LOCKING
            row->get_row(type, this, row, accesses[row_cnt]);
        #endif
    #elif CC_ALG == SLER                // Call get_row to actually access the row
        rc = row->get_row(type, this, accesses[ row_cnt ]->data, accesses[ row_cnt ]);
        if (rc == Abort) {
            return NULL;
        }
        accesses[row_cnt]->orig_row = row;
        auto temp_version = (Version*) accesses[row_cnt]->tuple_version;
        temp_version->data = row;
    #else
      rc = row->get_row(type, this, accesses[ row_cnt ]->data, accesses[row_cnt]);
      if (rc == Abort)
        return NULL;
      accesses[row_cnt]->orig_row = row;
    #endif

    #if (CC_ALG == BAMBOO && BB_OPT_RAW)
        if (rc == FINISH) {
            // RAW optimization
            accesses[row_cnt]->data->table = row->get_table();
        }
    #endif


    /**
     * Set the operation type of a certain access [row->get_row() successfully, so we can set access object directly]
     */
    accesses[row_cnt]->type = type;

    #if CC_ALG == TICTOC
        accesses[row_cnt]->wts = last_wts;
        accesses[row_cnt]->rts = last_rts;
    #elif CC_ALG == SILO
        accesses[row_cnt]->tid = last_tid;
    #elif CC_ALG == HEKATON
        accesses[row_cnt]->history_entry = history_entry;
    #endif


    /**
     * [type==WR]:
     */
    if (type == WR) {
        #if CC_ALG == WOUND_WAIT
            // make local copy to work on
            accesses[row_cnt]->data->table = row->get_table();
            accesses[row_cnt]->data->copy(row);
        #elif CC_ALG == BAMBOO
            // make local copy to work on
            accesses[row_cnt]->data->table = row->get_table();
            accesses[row_cnt]->data->copy(row);
            // make copy to rollback
            accesses[row_cnt]->orig_data->table = row->get_table();
            accesses[row_cnt]->orig_data->copy(row);
        #elif ROLL_BACK && (CC_ALG == DL_DETECT || CC_ALG == NO_WAIT || CC_ALG == WAIT_DIE)
            accesses[row_cnt]->orig_data->table = row->get_table();
            accesses[row_cnt]->orig_data->copy(row);
        #endif
    }

    #if (CC_ALG == NO_WAIT || CC_ALG == DL_DETECT) && ISOLATION_LEVEL == REPEATABLE_READ
        if (type == RD)
            row->return_row(type, accesses[ row_cnt ]->data, accesses[row_cnt]->lock_entry);
    #endif

    /**
     * Update txn statistics
     */
    row_cnt++;
    if (type == WR) {
        wr_cnt++;
    }

    uint64_t timespan = get_sys_clock() - starttime;
    INC_TMP_STATS(get_thd_id(), time_man, timespan);

    #if  (CC_ALG == WOUND_WAIT)
        if (type == WR)
            return accesses[row_cnt - 1]->data;
        else
            return accesses[row_cnt - 1]->orig_row;
    #elif CC_ALG == BAMBOO
        //printf("txn %lu got row %p at %d-th access %p\n", get_txn_id(), (void *)accesses[row_cnt - 1]->orig_row, row_cnt - 1, (void *)accesses[row_cnt - 1]);
        if (type == WR)
            return accesses[row_cnt - 1]->data;
        else {
            if (rc != FINISH)
              return accesses[row_cnt - 1]->orig_row;
            else
              return accesses[row_cnt - 1]->data; // RAW
        }
    #elif CC_ALG == IC3
        return accesses[row_cnt - 1]->data;
    #elif CC_ALG == SLER
    //        return accesses[row_cnt - 1]->data;
    //        return row;
    auto res_version = (Version*) accesses[row_cnt - 1]->tuple_version;

    // DEBUG 12-5
    if(res_version->data == NULL) {
        printf("ERROR!\n");
    }

    return res_version->data;

    #else
      return accesses[row_cnt - 1]->data;
    #endif
}

void txn_man::insert_row(row_t * row, table_t * table) {
    if (CC_ALG == HSTORE)
        return;
    assert(insert_cnt < MAX_ROW_PER_TXN);
    insert_rows[insert_cnt ++] = row;
}

void txn_man::index_insert(row_t * row, INDEX * index, idx_key_t key) {
    //TODO(zhihan): insert row in the index.
    uint64_t part_id = get_part_id(row);
    itemid_t * m_item = (itemid_t *) mem_allocator.alloc( sizeof(itemid_t), part_id);
    m_item->init();
    m_item->type = DT_row;
    m_item->location = row;
    m_item->valid = true;
#ifdef NDEBUG
    index->index_insert(key, m_item, part_id);
#else
    assert(index->index_insert(key, m_item, part_id) == RCOK);
#endif
}

itemid_t * txn_man::index_read(INDEX * index, idx_key_t key, int part_id) {
    uint64_t starttime = get_sys_clock();
    itemid_t * item;
    index->index_read(key, item, part_id, get_thd_id());
    INC_TMP_STATS(get_thd_id(), time_index, get_sys_clock() - starttime);
    return item;
}

void txn_man::index_read(INDEX * index, idx_key_t key, int part_id, itemid_t *& item) {
    uint64_t starttime = get_sys_clock();
    index->index_read(key, item, part_id, get_thd_id());
    INC_TMP_STATS(get_thd_id(), time_index, get_sys_clock() - starttime);
}

RC txn_man::finish(RC rc) {
#if TPCC_USER_ABORT
    RC ret_rc = rc;
  if (rc == ERROR)
    rc = Abort;
#endif

#if THINKTIME > 0
    usleep(THINKTIME);
#endif

#if CC_ALG == HSTORE
    return RCOK;
#endif

    uint64_t starttime = get_sys_clock();
#if CC_ALG == OCC
    if (rc == RCOK)
        rc = occ_man.validate(this);
    else
        cleanup(rc);
#elif CC_ALG == TICTOC
    if (rc == RCOK)
		rc = validate_tictoc();
	else 
		cleanup(rc);
#elif CC_ALG == SILO
  if (rc == RCOK)
		rc = validate_silo();
	else 
		cleanup(rc);
#elif CC_ALG == IC3
  if (rc == RCOK) {
    rc = validate_ic3();
    if (rc == RCOK) {
      if (!ATOM_CAS(status, RUNNING, COMMITED))
        rc = Abort;
    } else {
      status = ABORTED;
    }
  } else { // abort an txn
    // involve cascading aborts
    status = ABORTED; // may overwritten the aborts set by others.
  }
  if (rc == Abort)
    abort_ic3();
  cleanup(rc);
#elif CC_ALG == HEKATON
  rc = validate_hekaton(rc);
	cleanup(rc);
#elif CC_ALG == SLER
	rc = validate_sler(rc);
    cleanup(rc);
#elif CC_ALG == WOUND_WAIT
  if (rc == RCOK) {
        if (!ATOM_CAS(status, RUNNING, COMMITED))
            rc = Abort;
	}
	cleanup(rc);
#elif CC_ALG == BAMBOO
  if (rc == Abort)
      status = ABORTED;
  else {
    uint64_t starttime = get_sys_clock();
    //int times = 0;
    // aggregate barrier
    // addr_barriers = &(commit_barriers);
    // COMPILER_BARRIER
    // ATOM_ADD(commit_barriers, tmp_barriers);
    // ATOM_SUB(commit_barriers, g_thread_cnt << 2);
    while (!ATOM_CAS(commit_barriers, 0, COMMITED)) {
        if (commit_barriers & ABORTED) {
            rc = Abort;
            break;
        }
        if (g_last_retire > 0 && (retire_threshold < row_cnt - 1)) {
            //times++;
            //if (times >= 10) {
                uint64_t lapse = get_sys_clock();
                if ((lapse - starttime) >= (lapse - start_ts) * g_last_retire) {
            //printf("late retire\n");
                    for (int rid = row_cnt - 1; rid > retire_threshold; rid--) {
                        if (accesses[rid]->lock_entry->type == LOCK_SH)
                            continue;
                        accesses[rid]->orig_row->retire_row(accesses[rid]->lock_entry);
                    }
                    retire_threshold = row_cnt - 1;
                }
            //if ( (double)(lapse-starttime)/(lapse - start_ts) > 0)
            //    printf("%.6f\n", (lapse - starttime) / (lapse - start_ts));
           //     times = 0;
           // }
        }
    }
#if PF_BASIC 
    INC_STATS(get_thd_id(), time_commit, get_sys_clock() - starttime);
#endif
  }
  cleanup(rc);
#else
  cleanup(rc);
#endif

    uint64_t timespan = get_sys_clock() - starttime;
    INC_TMP_STATS(get_thd_id(), time_man,  timespan);
    INC_STATS(get_thd_id(), time_cleanup,  timespan);

#if TPCC_USER_ABORT
    if (rc == Abort && (ret_rc == ERROR)) {
  //printf("txn-%lu user init abort! \n", txn_id);
    return ret_rc;
  }
#endif
    //printf("txn-%lu finished status=%d\n", txn_id, status);
    return rc;
}

void txn_man::release() {
#if CC_ALG != SLER
    for (int i = 0; i < num_accesses_alloc; i++) {
    #if CC_ALG == BAMOO || CC_ALG == NO_WAIT || CC_ALG == WOUND_WAIT || CC_ALG == WAIT_DIE || CC_ALG == DL_DETEC
        delete accesses[i]->lock_entry;
    #endif
        mem_allocator.free(accesses[i], 0);
    }
    mem_allocator.free(accesses, 0);
    delete mcs_node;
#endif
}

#if COMMUTATIVE_OPS
void txn_man::inc_value(int col, uint64_t val) {
  // store operation and execute at commit time
  Access * access = accesses[row_cnt-1];
  access->com_op = COM_INC;
  access->com_val = val;
  access->com_col = col;
}

void txn_man::dec_value(int col, uint64_t val) {
  // store operation and execute at commit time
  Access * access = accesses[row_cnt-1];
  access->com_op = COM_DEC;
  access->com_val = val;
  access->com_col = col;
}
#endif
