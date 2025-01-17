#ifndef _TPCC_H_
#define _TPCC_H_

#include "wl.h"
#include "txn.h"
#include "tpcc_const.h"

class table_t;
class INDEX;
class tpcc_query;

#define IC3_TPCC_NEW_ORDER_PIECES   8
#define IC3_TPCC_PAYMENT_PIECES     4
#define IC3_TPCC_DELIVERY_PIECES    4

class tpcc_wl : public workload {
public:
    RC init();
    RC init_table();
    RC init_schema(const char * schema_file);
    RC get_txn_man(txn_man *& txn_manager, thread_t * h_thd);
    table_t * 		t_warehouse;
    table_t * 		t_district;
    table_t * 		t_customer;
    table_t *		t_history;
    table_t *		t_neworder;
    table_t *		t_order;
    table_t *		t_orderline;
    table_t *		t_item;
    table_t *		t_stock;

    INDEX * 	i_item;
    INDEX * 	i_warehouse;
    INDEX * 	i_district;
    INDEX * 	i_customer_id;
    INDEX * 	i_customer_last;
    INDEX * 	i_stock;
    INDEX * 	i_order; // key = (w_id, d_id, o_id)
    INDEX * 	i_orderline; // key = (w_id, d_id, o_id, ol_number)
    INDEX * 	i_neworder;  // key = (w_id, d_id, o_id)
    INDEX * 	i_orderline_wd; // key = (w_id, d_id).

    bool ** delivering;
    uint32_t next_tid;
#if CC_ALG == IC3
    void init_scgraph();
	SC_PIECE * get_cedges(TPCCTxnType txn_type, int piece_id);
	SC_PIECE *** sc_graph;
#endif

    std::map<uint64_t, std::map<uint64_t, std::map<uint64_t ,uint64_t>>> w_d_cid_oid;
    std::map<uint64_t, Region *> ch_regions;
    std::map<uint64_t, Nation *> ch_nations;
    std::map<uint64_t, Supplier *> ch_suppliers;
    std::vector<std::vector<std::pair<int32_t, int32_t>>> supp_stock_map ; // ,w_id,i_id

private:
    uint64_t num_wh;
    void init_tab_item();
    void init_tab_wh(uint32_t wid);
    void init_tab_dist(uint64_t w_id);
    void init_tab_stock(uint64_t w_id);
    void init_tab_cust(uint64_t d_id, uint64_t w_id);
    void init_tab_hist(uint64_t c_id, uint64_t d_id, uint64_t w_id);
    void init_tab_order(uint64_t d_id, uint64_t w_id);
    void init_tab_region();
    void init_tab_nation();
    void init_tab_supplier();

    void init_permutation(uint64_t * perm_c_id, uint64_t wid);

    static void * threadInitItem(void * This);
    static void * threadInitWh(void * This);
    static void * threadInitDist(void * This);
    static void * threadInitStock(void * This);
    static void * threadInitCust(void * This);
    static void * threadInitHist(void * This);
    static void * threadInitOrder(void * This);

    static void * threadInitWarehouse(void * This);
};

class tpcc_txn_man : public txn_man
{
public:
    void init(thread_t * h_thd, workload * h_wl, uint64_t part_id);
    RC run_txn(base_query * query);
private:
    tpcc_wl * _wl;
    RC run_payment(tpcc_query * m_query);
    RC run_new_order(tpcc_query * m_query);
    RC run_order_status(tpcc_query * query);
    RC run_delivery(tpcc_query * query);
    RC run_stock_level(tpcc_query * query);
    RC run_query2(tpcc_query * query );

    bool has_local_row(row_t * location, access_t type, row_t * local, access_t local_type) {
        if (location == local) {
            if ((type == local_type) || (local_type == WR)) {
                return true;
            } else if (type == WR) {
                return false;
            }
        }
        return false;
    };
};

#endif
