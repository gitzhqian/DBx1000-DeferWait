#include <sched.h>
#include "global.h"
#include "helper.h"
#include "ycsb.h"
#include "wl.h"
#include "thread.h"
#include "table.h"
#include "row.h"
#include "index_hash.h"
#include "index_btree.h"
#include "catalog.h"
#include "manager.h"
#include "row_lock.h"
#include "row_ts.h"
#include "row_mvcc.h"
#include "mem_alloc.h"
#include "query.h"
#include <math.h>


int ycsb_wl::next_tid;

RC ycsb_wl::init() {
    workload::init();
    next_tid = 0;
    string path = "/home/zhangqian/DBx1000-DeferWait/benchmarks/YCSB_schema.txt";
    init_schema( path );

    init_table_parallel();
//	init_table();
    return RCOK;
}

RC ycsb_wl::init_schema(string schema_file) {
    workload::init_schema(schema_file);
    the_table = tables["MAIN_TABLE"];
    the_index = indexes["MAIN_INDEX"];
    return RCOK;
}

int ycsb_wl::key_to_part(uint64_t key) {
    uint64_t rows_per_part = g_synth_table_size / g_part_cnt;
    return key / rows_per_part;
}

RC ycsb_wl::init_table() {
    RC rc = RCOK;
    uint64_t total_row = 0;
    while (true) {
        for (UInt32 part_id = 0; part_id < g_part_cnt; part_id ++) {
            if (total_row > g_synth_table_size)
                goto ins_done;
            row_t * new_row = NULL;
            //zhihan
            uint64_t row_id = get_sys_clock();
            rc = the_table->get_new_row(new_row, part_id, row_id);              // Allocate space for new_row && initialize relevant attributes(manager)
            // TODO insertion of last row may fail after the table_size is updated. So never access the last record in a table

            assert(rc == RCOK);
            uint64_t primary_key = total_row;
            new_row->set_primary_key(primary_key);
            new_row->set_value(0, &primary_key);

            Catalog * schema = the_table->get_schema();
            for (UInt32 fid = 0; fid < schema->get_field_cnt(); fid ++) {
                int field_size = schema->get_field_size(fid);
                char value[field_size];
                for (int i = 0; i < field_size; i++)
                    value[i] = (char)rand() % (1<<8) ;
                new_row->set_value(fid, value);
            }

            itemid_t * m_item = (itemid_t *) mem_allocator.alloc( sizeof(itemid_t), part_id );
            assert(m_item != NULL);
            m_item->type = DT_row;
            m_item->location = new_row;
            m_item->valid = true;

            uint64_t idx_key = primary_key;
            rc = the_index->index_insert(idx_key, m_item, part_id);
            assert(rc == RCOK);

            total_row ++;
        }
    }
    ins_done:
    printf("[YCSB] Table \"MAIN_TABLE\" initialized.\n");
    return rc;

}

// init table in parallel
void ycsb_wl::init_table_parallel() {
    enable_thread_mem_pool = true;
    pthread_t p_thds[g_init_parallelism - 1];
    for (UInt32 i = 0; i < g_init_parallelism - 1; i++)
        pthread_create(&p_thds[i], NULL, threadInitTable, this);
    threadInitTable(this);

    for (uint32_t i = 0; i < g_init_parallelism - 1; i++) {
        int rc = pthread_join(p_thds[i], NULL);
        if (rc) {
            printf("ERROR; return code from pthread_join() is %d\n", rc);
            exit(-1);
        }
    }
    enable_thread_mem_pool = false;
    mem_allocator.unregister();
}

void * ycsb_wl::init_table_slice() {
    UInt32 tid = ATOM_FETCH_ADD(next_tid, 1);
    // set cpu affinity
    set_affinity(tid);

    mem_allocator.register_thread(tid);

//	assert(g_synth_table_size % g_init_parallelism == 0);
//	assert(tid < g_init_parallelism);

    while ((UInt32)ATOM_FETCH_ADD(next_tid, 0) < g_init_parallelism) {}

//	assert((UInt32)ATOM_FETCH_ADD(next_tid, 0) == g_init_parallelism);


    // [BUG In DBx1000] Use uint64_t will lose some tuples.
    double slice_size = (double)g_synth_table_size / (double)g_init_parallelism;
//	uint64_t slice_size = g_synth_table_size / g_init_parallelism;

    /*[BUG]
     * [Problem]: Calculation of double will cause loss of accuracy, so sometimes the result is bigger/smaller than the correct result.
     * [Example]: When table_size = 50,000,000 and thread_cnt = 22, there will be an error, because slise_size*22 = 50000000.000000007 > 50000000, which makes the thread insert a wrong tuple(id = 50,000,000)
     * [Solution]: This error will only happen in the last thread, so we can just modify " slice_size * (tid + 1)" to "min(slice_size * (tid + 1),(double)g_synth_table_size)".
    */
    for (uint64_t key = ceil(slice_size * tid);
         key < min(slice_size * (tid + 1),(double)g_synth_table_size);
         key ++
            ) {
        row_t * new_row = NULL;
        //zhihan uint64_t row_id;
        uint64_t row_id = get_sys_clock();
        int part_id = key_to_part(key);
#ifdef NDEBUG
        the_table->get_new_row(new_row, part_id, row_id);
#else
        RC rc = the_table->get_new_row(new_row, part_id, row_id);
#endif
        assert(rc == RCOK);
        uint64_t primary_key = key;
        new_row->set_primary_key(primary_key);
//		new_row->set_value(0, &primary_key);

        // 2-15[BUG FIX] Set the correct value instead of a pointer to the primary key column.
        char *temp = (char *) _mm_malloc(sizeof(char), 64);
        sprintf(temp,"%lu",primary_key);
        new_row->set_value(0, temp);
        _mm_free(temp);


        Catalog * schema = the_table->get_schema();

        // 2-15[BUG FIX]: Don't overwrite the value of the primary key column.
        for (UInt32 fid = 1; fid < schema->get_field_cnt(); fid ++) {
            char value[6] = "hello";
            new_row->set_value(fid, value);
        }

        itemid_t * m_item =
                (itemid_t *) mem_allocator.alloc( sizeof(itemid_t), part_id );
        assert(m_item != NULL);
        m_item->type = DT_row;
        m_item->location = new_row;
        m_item->valid = true;
        uint64_t idx_key = primary_key;
#ifdef NDEBUG
        the_index->index_insert(idx_key, m_item, part_id);
#else
        rc = the_index->index_insert(idx_key, m_item, part_id);
#endif
        assert(rc == RCOK);
    }
    return NULL;
}

RC ycsb_wl::get_txn_man(txn_man *& txn_manager, thread_t * h_thd){
    txn_manager = (ycsb_txn_man *)_mm_malloc( sizeof(ycsb_txn_man), 64 );
    new(txn_manager) ycsb_txn_man();
    txn_manager->init(h_thd, this, h_thd->get_thd_id());
    return RCOK;
}
