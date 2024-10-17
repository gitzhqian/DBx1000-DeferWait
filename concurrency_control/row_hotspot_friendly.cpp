//
// Created by root on 2022/9/18.
//

#include "manager.h"
#include "row_hotspot_friendly.h"
#include "mem_alloc.h"
#include <mm_malloc.h>

#if CC_ALG == HOTSPOT_FRIENDLY


/**
 * Initialize a row in HOTSPOT_FRIENDLY
 * @param row : pointer of this row(the first version) [Empty: no data]
 */
void Row_hotspot_friendly::init(row_t *row){
    // initialize version header
    version_header = (Version *) _mm_malloc(sizeof(Version), 64);

    version_header->begin_ts = 0;
    version_header->end_ts = INF;

    version_header->dynamic_txn_ts = (volatile ts_t *)_mm_malloc(sizeof(ts_t), 64);
    version_header->dynamic_txn_ts = new ts_t(0);
    version_header->type = XP;
//    version_header->read_queue = new std::vector<Version *>();
    version_header->read_queue = NULL;

    // pointer must be initialized
    version_header->prev = NULL;
    version_header->next = NULL;
    version_header->retire = NULL;
    version_header->retire_ID = 0;

#ifdef ABORT_OPTIMIZATION
    version_header->version_number = 0;
#endif

#if VERSION_CHAIN_CONTROL
    // Restrict the length of version chain.
    threshold = 0;
#endif

//    blatch = false;
#if LATCH == LH_SPINLOCK
    spinlock_row = new pthread_spinlock_t;
    pthread_spin_init(spinlock_row, PTHREAD_PROCESS_SHARED);
#else
    latch_row = new mcslock();
#endif


}

/**
 * Read/Write a row according to the type.
 * @param txn : the txn which accesses the row
 * @param type : operation type(can only be R_REQ, P_REQ)
 * @param row : the row in the Access Object [Empty: no data]
 * @param access : the Access Object
 * @return
 */
RC Row_hotspot_friendly::access(txn_man * txn, TsType type, Access * access){

    // Optimization for read_only long transaction.
#if READ_ONLY_OPTIMIZATION_ENABLE
    if(txn->is_long && txn->read_only){
        while(!ATOM_CAS(blatch, false, true)){
            PAUSE
        }

        Version* read_only_version = version_header;
        while (read_only_version){
            if(read_only_version->begin_ts != UINT64_MAX){
                assert(read_only_version->retire == NULL);
                access->tuple_version = read_only_version;
                break;
            }
            read_only_version = read_only_version->next;
        }

        blatch = false;
        return RCOK;
    }
#endif

    RC rc = RCOK;
    uint64_t txn_id = txn->get_hotspot_friendly_txn_id();

    Version * new_version = createNewVersion(txn, access, type);

#if PF_MODEL
    uint64_t starttime  = get_sys_clock();
#endif

    lock_row(txn);
    COMPILER_BARRIER

#if PF_MODEL
    uint64_t endtime_lockrow = get_sys_clock();
    INC_STATS(txn->get_thd_id(), time_lockrow, endtime_lockrow - starttime);
    starttime  = endtime_lockrow;
#endif
    //1.assign time
    ts_t ts = txn->get_ts();
    assert(version_header != nullptr);
    auto row_max_ts = version_header->begin_ts;
    auto dep_header_txn = version_header->retire;

    // 1.1 if read the uncommitted, check header and compute the ts
    if (dep_header_txn != nullptr) {
        row_max_ts =  *(version_header->dynamic_txn_ts);
    }
    // 1.2 if the header has latest readers. check readers and compute the ts
    auto readers = version_header->read_queue;
    Version *dep_read_ = nullptr;
    if (type == P_REQ && readers != nullptr) {
        while (true) {
            if (readers->type == RD) {
                row_max_ts = *(readers->dynamic_txn_ts);
                dep_read_ = readers;
                break;
            } else {
                readers = readers->prev;
            }
            if (readers == version_header){break;}
        }
    }
    //2.handle dependency
    txn_man *dep_txn ;
    bool read_uncommit = false;
    bool defer_need = false;
    //2.1 if header is uncommitted or has uncommitted reads
    if (dep_header_txn != nullptr || dep_read_ != nullptr) {
        if (type == P_REQ) {
            read_uncommit = true;
        } else{
            if (dep_header_txn != nullptr){
                read_uncommit = true;
            }
        }
    }
    //3. compute ts
    //3.1 if read the committed
    // assign timestamp at the first conflicts, at the commit if ts =0 then calculate ts
    if (read_uncommit && type == P_REQ){
        if (ts == 0){
            std::stack<txn_man *> dep_txns_stck;
            auto version_next = version_header;
            dep_txn = version_next->retire;
            while (true) {
                if (version_next->begin_ts != UINT64_MAX) break;
                if (dep_txn != nullptr){
                    if (dep_txn->timestamp == 0) {
                        dep_txns_stck.push(dep_txn);
                    }
                }
                version_next = version_next->next;
                dep_txn = version_next->retire;
            }

            while (!dep_txns_stck.empty()){
                auto tp_txn = dep_txns_stck.top();
                assign_ts(0, tp_txn);
                dep_txns_stck.pop();
            }

            ts = assign_ts(ts, txn);
        }

#if NO_DEFER
        auto version_next_1 = version_header;
        dep_txn = version_next_1->retire;
        while (true) {
            if (version_next_1->begin_ts != UINT64_MAX)  break;
            if (dep_txn != nullptr && (dep_txn->status == RUNNING)){
                if ( a_higher_than_b(dep_txn->timestamp, ts) ) {
                    dep_txn->set_abort();
#if PF_MODEL
                    INC_STATS(txn->get_thd_id(), find_circle_abort, 1);
#endif
                }
            }
            version_next_1 = version_next_1->next;
            dep_txn = version_next_1->retire;
        }
#else
        auto version_next_2 = version_header;
        dep_txn = version_next_2->retire;
        while (true) {
            if (version_next_2->begin_ts != UINT64_MAX) break;
            if (dep_txn != nullptr){
                if (a_higher_than_b(dep_txn->timestamp, ts)) {
                    ts = dep_txn->timestamp + 1;
                }
            }
            version_next_2 = version_next_2->next;
            dep_txn = version_next_2->retire;
        }

        defer_need = true;
#endif
    }


#if PF_MODEL
    uint64_t endtime_assign = get_sys_clock();
    INC_STATS(txn->get_thd_id(), time_assign, endtime_assign - starttime);
    starttime  = endtime_assign;
#endif

    //3. defer and detect cycle
    dep_txn = version_header->retire;
    if (defer_need && dep_txn!= nullptr){
        uint64_t defer_ts = ts + 1;
        auto *path = new std::unordered_set<uint64_t>();
        auto ret = detect_cycle(txn, dep_txn, defer_ts, path);
        // dep_txn arise a cycle, then is aborted
        if (type == P_REQ && dep_read_ != nullptr) {
            auto version_next = dep_read_->prev;
            while (true){
                if (version_next->begin_ts != UINT64_MAX) break;
                dep_txn = version_next->retire;
                if (dep_txn != nullptr && dep_txn->status == RUNNING){
                    if (path->find(dep_txn->txn_id) != path->end()){
                        dep_txn->set_abort();
#if PF_MODEL
                        INC_STATS(txn->get_thd_id(), find_circle_abort, 1);
#endif
#if ADAPTIVE
                        auto ret =  txn->insert_hotspot(this->version_header->data->get_primary_key());
                        if (ret){
                            INC_STATS(  txn->get_thd_id(), abort_hotspot, 1);
                        }
                        INC_STATS(  txn->get_thd_id(), abort_position,  txn->row_cnt);
                        INC_STATS(  txn->get_thd_id(), abort_position_cnt, 1);
#endif
                    }
                }

                if (version_next->type == WR){
                    version_next = version_next->next;
                }else{
                    version_next = version_next->prev;
                }
            }
        } else{
            auto version_next = version_header->next;
            while (true){
                if (version_next->begin_ts != UINT64_MAX) break;
                dep_txn = version_next->retire;
                if (dep_txn != nullptr && dep_txn->status == RUNNING){
                    if (path->find(dep_txn->txn_id) != path->end()){
                        dep_txn->set_abort();
#if PF_MODEL
                        INC_STATS(txn->get_thd_id(), find_circle_abort, 1);
#endif
#if ADAPTIVE
                        auto ret =  txn->insert_hotspot(this->version_header->data->get_primary_key());
                        if (ret){
                            INC_STATS(  txn->get_thd_id(), abort_hotspot, 1);
                        }
                        INC_STATS(  txn->get_thd_id(), abort_position,  txn->row_cnt);
                        INC_STATS(  txn->get_thd_id(), abort_position_cnt, 1);
#endif
                    }
                }

                version_next = version_next->next;
            }
        }
    }
#if PF_MODEL
    uint64_t endtime_find = get_sys_clock();
    INC_STATS(txn->get_thd_id(), time_find_circle, endtime_find - starttime );
    starttime  = endtime_find;
#endif

    if (txn->ready_abort){
        rc = Abort;
        unlock_row(txn);
        return rc;
    }

    ts = txn->get_ts();

    txn_man* retire_txn;
    if (type == R_REQ) {
        // This optimization is only available to read-only YCSB workload.
#if WORKLOAD == YCSB
        // This optimization isn't suitable for synthetic YCSB.
#if !SYNTHETIC_YCSB
        if(g_read_perc == 1){
            Version *temp_version = version_header;
            auto temp_retire_txn = temp_version->retire;
            if (!temp_retire_txn) {           // committed version
                rc = RCOK;
//                        access->tuple_version = temp_version;
//                        createNewVersion(txn, access, RD, version_header);
                update_version(txn, new_version, RD);
                ((Version *)access->tuple_version)->begin_ts = temp_version->begin_ts;
                COMPILER_BARRIER
                unlock_row(txn);
                return rc;
//                        return rc;
            } else {           //uncommitted version
                if (temp_retire_txn->status == committing || temp_retire_txn->status == COMMITED) {
//                            access->tuple_version = temp_version;
//                            createNewVersion(txn, access, RD, version_header);
                    update_version(txn, new_version, RD);
                    assert(temp_version->begin_ts != UINT64_MAX);
                    ((Version *)access->tuple_version)->begin_ts = temp_version->begin_ts;
                    COMPILER_BARRIER
                    unlock_row(txn);
                    return rc;
//                            return rc;
                }
            }
        }
#endif
#endif
#if VERSION_CHAIN_CONTROL
        // Restrict the length of version chain. [Judge the priority of txn and threshold of tuple first.]
        uint64_t starttime_read = get_sys_clock();
        double timeout_span = 0.1*1000000UL;
        while (txn->priority < (double)threshold && (double)(get_sys_clock() - starttime_read) < 0.1*1000000000UL){
            PAUSE
        }
#endif

        while (version_header) {
            assert(version_header->end_ts == INF);
            retire_txn = version_header->retire;

            // committed version
            if (!retire_txn) {
                rc = RCOK;
                update_version(txn, new_version, RD);
                assert(version_header->begin_ts != UINT64_MAX);

                unlock_row(txn);
#if PF_MODEL
                uint64_t endtime_exec = get_sys_clock();
                INC_STATS(txn->get_thd_id(), time_exec, endtime_exec - starttime);
#endif
                return rc;
            }
                // uncommitted version
            else {
                assert(retire_txn != txn);
//                assert(version_header->retire_ID == retire_txn->hotspot_friendly_txn_id);
#if DEADLOCK_DETECTION
                // [DeadLock]
                if (retire_txn->WaitingSetContains(txn_id) && retire_txn->set_abort() == RUNNING) {
                    version_header = version_header->next;

    #ifdef ABORT_OPTIMIZATION
                    // Recursively update the chain_number of uncommitted old version and the first committed verison.
                    Version* version_retrieve = version_header;

                    while(version_retrieve->begin_ts == UINT64_MAX){
                        assert(version_retrieve->retire != NULL);

                        version_retrieve->version_number += CHAIN_NUMBER_ADD_ONE;
                        version_retrieve = version_retrieve->next;
                    }

                    // Update the chain-number of the first committed version.
                    assert(version_retrieve->begin_ts != UINT64_MAX && version_retrieve->retire == NULL && version_retrieve->retire_ID == 0);
                    version_retrieve->version_number += CHAIN_NUMBER_ADD_ONE;
    #endif

                    assert(version_header->end_ts == INF && retire_txn->status == ABORTED);
                    continue;
                }
                // [No Deadlock]
                else {
#endif
                status_t temp_status = retire_txn->status;
                //[IMPOSSIBLE]
                /* committing and COMMITTED means version_header.retire == NULL right now, which means retire_txn acquires the blatch.
                 * This is impossible because I acquire the blatch in line 305.
                 * Retire_txn has no chance to acquire the blatch and update the version_header.retire == NULL.
                 */
                if (temp_status == committing || temp_status == COMMITED || temp_status == validating || temp_status == writing) {
                    update_version(txn, new_version, RD);
                    assert(false);

                    unlock_row(txn);
#if PF_MODEL
                    uint64_t endtime_exec = get_sys_clock();
                    INC_STATS(txn->get_thd_id(), time_exec, endtime_exec - starttime);
#endif
                    return rc;
                } else if (temp_status == RUNNING ) {       // record dependency
                    txn->SemaphoreAddOne();
                    retire_txn->PushDependency(txn, txn->get_txn_id(), DepType::WRITE_READ_);  //dependent on me(retire)
                    txn->insert_i_dependency_on(retire_txn, DepType::WRITE_READ_);
#if DEADLOCK_DETECTION
                    retire_txn->PushDependency(txn, txn->get_hotspot_friendly_txn_id(), DepType::WRITE_READ_);
                        if(temp_status == RUNNING) {
                            txn->UnionWaitingSet(retire_txn->hotspot_friendly_waiting_set);
                            if(txn->status == ABORTED){
                                rc = Abort;
                                break;
                            }
                        }
#endif
                    // Record in Access Object
                    update_version(txn, new_version, RD);
                    unlock_row(txn);
#if PF_MODEL
                    uint64_t endtime_exec = get_sys_clock();
                    INC_STATS(txn->get_thd_id(), time_exec, endtime_exec - starttime);
#endif
                    return rc;
                } else if (temp_status == ABORTED) {
                    version_header = version_header->next;
#ifdef ABORT_OPTIMIZATION
                    // Recursively update the chain_number of uncommitted old version and the first committed verison.
                    Version* version_retrieve = version_header;
                    while(version_retrieve->begin_ts == UINT64_MAX){
                        assert(version_retrieve->retire != NULL);
                        version_retrieve->version_number += CHAIN_NUMBER_ADD_ONE;
                        version_retrieve = version_retrieve->next;
                    }
                    // Update the chain-number of the first committed version.
//                        assert(version_retrieve->begin_ts != UINT64_MAX && version_retrieve->retire == NULL && version_retrieve->retire_ID == 0);
                    version_retrieve->version_number += CHAIN_NUMBER_ADD_ONE;
#endif
                    assert(version_header->end_ts == INF && retire_txn->status == ABORTED);
                    continue;
                }
#if DEADLOCK_DETECTION
                }
#endif
            }
        }
    }
    else if (type == P_REQ) {
#if VERSION_CHAIN_CONTROL
        // Restrict the length of version chain. [Judge the priority of txn and threshold of tuple first.]
        uint64_t starttime_write = get_sys_clock();
        double timeout_span = 0.1*1000000UL;
        while (txn->priority < (double)threshold && (double)(get_sys_clock() - starttime_write) < 0.1*100000000UL){
            PAUSE
        }
#endif
        while (version_header) {
            assert(version_header->end_ts == INF);
            retire_txn = version_header->retire;

            // [Error Case]: should not happen
            if (version_header->end_ts != INF) {
                printf("error. \n");
                version_header = version_header->next;
            } else {
                // Todo: should search write set and read set. However, since no record will be accessed twice, we don't have to search these two sets
                rc = RCOK;

                bool dependency_on_rd = false;
                // processing the RW,
                auto readers_ = version_header->read_queue;
                if (readers_ != nullptr){
                    while (true){
                        auto dep_txn = readers_->retire;
                        if (readers_->type == RD && dep_txn!= nullptr && dep_txn->status == RUNNING){
                            txn->SemaphoreAddOne();
                            dep_txn->PushDependency(txn, txn->get_txn_id(), DepType::READ_WRITE_);
                            txn->insert_i_dependency_on(dep_txn, DepType::READ_WRITE_);
                            dependency_on_rd = true;
                        }
                        readers_ = readers_->prev;
                        if (readers_ == version_header) {break;}
                    }
                }

                /*** Deadlock Detection */
                // committed version, there is no uncommitted version, then it will not dirty write
                if (!retire_txn) {
                    rc = RCOK;
                    assert(version_header->begin_ts != UINT64_MAX);
                    update_version(txn, new_version, WR);

                    unlock_row(txn);
#if PF_MODEL
                    uint64_t endtime_exec = get_sys_clock();
                    INC_STATS(txn->get_thd_id(), time_exec, endtime_exec - starttime);
#endif
                    return rc;
                }
                    // uncommitted version, there are uncommitted versions, then it will dirty write
                else {
                    assert(retire_txn != txn);
//                    assert(version_header->retire_ID == retire_txn->hotspot_friendly_txn_id);
#if DEADLOCK_DETECTION
                    // [DeadLock]
                    // deadlock detection:
                    // if find the circle, handling the retire txn:
                    //      (deadlock)---->if the retire txn is running, then abort it;
                    //      (no deadlock)->if the retire txn is aborted or committed, then ok;
                    //      (no deadlock)->if the retire txn is running or validating, then dirty write and handling dependency;
                    //                     ->update my commit semaphore ++
                    //                     ->append me to the retire txn's direct dependency
                    //                     ->update my dependency, combining with the retire txn's dependency
                    if (retire_txn->WaitingSetContains(txn_id) && retire_txn->set_abort() == RUNNING) {
                        version_header = version_header->next;
#ifdef ABORT_OPTIMIZATION
                        // Recursively update the chain_number of uncommitted old version and the first committed verison.
                        Version* version_retrieve = version_header;
                        // update the chain-number of the uncommitted version.
                        while(version_retrieve->begin_ts == UINT64_MAX){
                            assert(version_retrieve->retire != NULL);
                            version_retrieve->version_number += CHAIN_NUMBER_ADD_ONE;
                            version_retrieve = version_retrieve->next;
                        }
                        // Update the chain-number of the first committed version.
                        assert(version_retrieve->begin_ts != UINT64_MAX && version_retrieve->retire == NULL && version_retrieve->retire_ID == 0);
                        version_retrieve->version_number += CHAIN_NUMBER_ADD_ONE;
#endif
                        assert(version_header->end_ts == INF && retire_txn->status == ABORTED);
                        continue;
                    }
                    // [No Deadlock]
                    else {
#endif
                    status_t temp_status = retire_txn->status;
                    //[IMPOSSIBLE]
                    /* committing and COMMITTED means version_header.retire == NULL right now, which means retire_txn acquires the blatch.
                     * This is impossible because I acquire the blatch in line 305.
                     * Retire_txn has no chance to acquire the blatch and update the version_header.retire == NULL.
                     */
                    if (temp_status == committing || temp_status == COMMITED || temp_status == validating || temp_status == writing) {
                        // create new version & record current row in accesses
                        update_version(txn, new_version, WR);
                        unlock_row(txn);
#if PF_MODEL
                        uint64_t endtime_exec = get_sys_clock();
                        INC_STATS(txn->get_thd_id(), time_exec, endtime_exec - starttime);
#endif
                        return rc;
                    } else if (temp_status == RUNNING) {       // record dependency
                        if (!dependency_on_rd){
                            txn->SemaphoreAddOne();
                            retire_txn->PushDependency(txn, txn->get_txn_id(), DepType::WRITE_WRITE_);
                            txn->insert_i_dependency_on(retire_txn, DepType::WRITE_WRITE_);
                        }
#if DEADLOCK_DETECTION
                        retire_txn->PushDependency(txn, txn->get_hotspot_friendly_txn_id(), DepType::WRITE_WRITE_);
                            // Avoid unnecessary data update.
                            if(temp_status == RUNNING) {
                                txn->UnionWaitingSet(retire_txn->hotspot_friendly_waiting_set);
                                if(txn->status == ABORTED){
                                    rc = Abort;
                                    break;
                                }
                            }
#endif
                        update_version(txn, new_version, WR);
                        unlock_row(txn);
#if PF_MODEL
                        uint64_t endtime_exec = get_sys_clock();
                        INC_STATS(txn->get_thd_id(), time_exec, endtime_exec - starttime);
#endif
                        return rc;
                    }  else if (temp_status == ABORTED) {
                        version_header = version_header->next;
#ifdef ABORT_OPTIMIZATION
                        // Recursively update the chain_number of uncommitted old version and the first committed verison.
                        Version* version_retrieve = version_header;
                        while(version_retrieve->begin_ts == UINT64_MAX){
                            assert(version_retrieve->retire != NULL);
                            version_retrieve->version_number += CHAIN_NUMBER_ADD_ONE;
                            version_retrieve = version_retrieve->next;
                        }
                        // Update the chain-number of the first committed version.
//                            assert(version_retrieve->begin_ts != UINT64_MAX && version_retrieve->retire == NULL && version_retrieve->retire_ID == 0);
                        version_retrieve->version_number += CHAIN_NUMBER_ADD_ONE;
#endif
                        assert(version_header->end_ts == INF && retire_txn->status == ABORTED);
                        continue;
                    }
#if DEADLOCK_DETECTION
                    }
#endif
                }
            }
        }

    }
    else {
        assert(false);
    }
#if PF_MODEL
    uint64_t endtime_exec = get_sys_clock();
    INC_STATS(txn->get_thd_id(), time_exec, endtime_exec - starttime);
#endif

    unlock_row(txn);
    COMPILER_BARRIER

    return rc;
}

Version *Row_hotspot_friendly::createNewVersion(txn_man * txn, Access * access , TsType type) {
#if PF_MODEL
    uint64_t starttime  = get_sys_clock();
#endif
    // create a new Version Object & row object
    Version* new_version = (Version *) _mm_malloc(sizeof(Version), 64);
    new_version->prev = NULL;
    new_version->begin_ts = INF;
    new_version->end_ts = INF;
    new_version->retire = txn;
    new_version->retire_ID = txn->get_txn_id();
    access->tuple_version = new_version;

    new_version->dynamic_txn_ts = (volatile ts_t *)&txn->timestamp;
    new_version->read_queue = nullptr;

#if PF_MODEL
    INC_STATS(txn->get_thd_id(), time_creat_version, get_sys_clock() - starttime);
#endif

    return new_version;
}
void Row_hotspot_friendly::update_version(txn_man * txn, Version *new_version, access_t type ) {
#if PF_MODEL
//    uint64_t starttime  = get_sys_clock();
#endif
    //for read, only RD->XP/AT; for write, XP->WR->AT
    if (type == RD){
        new_version->type = RD;
        new_version->next = version_header;
        if (version_header->read_queue == nullptr){
            new_version->prev = version_header;
            version_header->read_queue = new_version;
        }else{
            auto read_queue = version_header->read_queue;
            new_version->prev = read_queue;
            version_header->read_queue = new_version;
        }

    } else if (type == WR){
        new_version->type = WR;

#ifdef ABORT_OPTIMIZATION
        new_version->version_number = version_header->version_number + 1;
#endif

#if VERSION_CHAIN_CONTROL
        // Restrict the length of version chain. [Update the count of uncommitted versions.]
      IncreaseThreshold();
    txn->PriorityAddOne();
#endif

        auto readers = version_header->read_queue;
        if ( readers != nullptr){
            while (true){
                readers->next = new_version;
                readers = readers->prev;
                if (readers == version_header){break;}
            }
        }

        new_version->data = (row_t *) _mm_malloc(sizeof(row_t), 64);
        new_version->data->init(MAX_TUPLE_SIZE);

        new_version->next = version_header;
        // update the cur_row of txn, record the object of update operation [new version]
        // set the meta-data of old_version
        version_header->prev = new_version;
        // relocate version header
        version_header = new_version;
        assert(version_header->end_ts == INF);
    }
#if PF_MODEL
//    INC_STATS(txn->get_thd_id(), time_creat_version, get_sys_clock() - starttime);
#endif
}


#endif


