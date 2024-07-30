//
// Created by root on 2022/9/18.
//

#ifndef DBX1000_ROW_HOTSPOT_FRIENDLY_H
#define DBX1000_ROW_HOTSPOT_FRIENDLY_H

#pragma once

#include <stack>
#include "row.h"
#include "txn.h"
#include "global.h"

class table_t;
class Catalog;
class txn_man;

#if CC_ALG == HOTSPOT_FRIENDLY

#define INF UINT64_MAX


/**
 * Version Format in HOTSPOT_FRIENDLY
 */

#ifdef ABORT_OPTIMIZATION
// 24 bit chain_number + 40 bit deep_length
#define CHAIN_NUMBER (((1ULL << 24)-1) << 40)
#define DEEP_LENGTH ((1ULL << 40)-1)
#define CHAIN_NUMBER_ADD_ONE (1ULL << 40)
#endif


struct Version {
    ts_t begin_ts;
    ts_t end_ts;
    access_t type;
    volatile ts_t *dynamic_txn_ts;  //pointing to the created transaction's ts
//    std::vector<Version *> *read_queue;
    Version *read_queue;
    Version* prev;
    Version* next;
    txn_man* retire;      // the txn_man of the uncommitted txn which updates the tuple version
    uint64_t retire_ID;
    row_t* data;
#ifdef ABORT_OPTIMIZATION
    uint64_t version_number;
#endif

};


class Row_hotspot_friendly {
public:
    void init(row_t *row);

    RC access(txn_man *txn, TsType type, Access *access);

    Version *get_version_header() { return this->version_header; }

    volatile bool blatch;
    Version *version_header;          // version header of a row's version chain (N2O)
//    Version *latest_committed_header;
#if LATCH == LH_SPINLOCK
    pthread_spinlock_t * spinlock_row;
#else
    mcslock * latch_row;
#endif

#if VERSION_CHAIN_CONTROL
    // Restrict the length of version chain.
    uint64_t threshold;

    void IncreaseThreshold(){
        ATOM_ADD(threshold,1);
    }

    void DecreaseThreshold(){
        ATOM_SUB(threshold,1);
    }
#endif

    void  lock_row(txn_man * txn) const {
        if (likely(g_thread_cnt > 1)) {
#if LATCH == LH_SPINLOCK
            pthread_spin_lock(spinlock_row);
#else
            latch_row->acquire(txn->mcs_node);
#endif
        }
    };

    void  unlock_row(txn_man * txn) const {
        if (likely(g_thread_cnt > 1)) {
#if LATCH == LH_SPINLOCK
            pthread_spin_unlock(spinlock_row);
#else
            latch_row->release(txn->mcs_node);
#endif
        }
    };

    // check priorities
    inline static bool a_higher_than_b(ts_t a, ts_t b) {
        return a >= b;
    };

    inline static int assign_ts(ts_t ts, txn_man *txn) {
        if (ts == 0) {
            ts = txn->set_next_ts(1);
            // if fail to assign, reload
            if (ts == 0) {
                ts = txn->get_ts();
            }
        }
        return ts;
    };

    bool detect_cycle(txn_man *txn, txn_man *check_txn, uint64_t defer_ts, std::unordered_set<uint64_t> *path) {
        auto dependency_list = txn->hotspot_friendly_dependency;
        uint64_t ts = defer_ts;
        std::stack<txn_man *> dep_stack;
        for (auto &dep_pair: *dependency_list) {
            dep_stack.push(dep_pair.dep_txn);
        }

        while (true){
            if (check_txn == nullptr) return false;
            std::vector<txn_man *> dep_list;
            while (!dep_stack.empty()){
                txn_man *txn_ = dep_stack.top();
                dep_list.push_back(txn_);
                dep_stack.pop();
            }

            bool find = false;
            for (auto &dep_pair: dep_list) {
                auto dep_txn_ = dep_pair;
                if (dep_txn_ == nullptr) continue;
                if (path->find(dep_txn_->hotspot_friendly_txn_id) != path->end()) continue;
                if (check_txn != nullptr && dep_txn_->hotspot_friendly_txn_id == check_txn->hotspot_friendly_txn_id
                    && check_txn->status == RUNNING){
                    check_txn->set_abort();
#if PF_MODEL
                    INC_STATS(txn->get_thd_id(), find_circle_abort, 1);
#endif
                    find = true;
                } else{
                    if (a_higher_than_b(ts, dep_txn_->get_ts()) ){
                        dep_txn_->set_ts(ts);
                        ts++;
                    }
                    auto dep_txn_deps = dep_txn_->hotspot_friendly_dependency;
                    if (!dep_txn_deps->empty()){
                        for (auto &dep_: *dep_txn_deps) {
                            if (dep_.dep_txn != nullptr){
                                dep_stack.push(dep_.dep_txn);
                            }
                        }
                    }
                }
                path->insert(dep_txn_->hotspot_friendly_txn_id);
            }

            if (find){
                return true;
            }
            if (dep_stack.empty()){
                break;
            }
        }

        return false;

    }



private:
    Version * createNewVersion(txn_man *txn, Access *access , TsType type);
    void update_version(txn_man * txn, Version *new_version, access_t type );

};

#endif


#endif //DBX1000_ROW_HOTSPOT_FRIENDLY_H
