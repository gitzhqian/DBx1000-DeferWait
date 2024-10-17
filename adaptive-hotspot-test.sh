#!/bin/bash

if [ "$#" -ne 4 ]; then
    echo "Usage: $0 [optionCC] [optionHOT] [optionWK] [txnsize]"
    exit 1
fi

optionCC=$1
optionHOT=$2
optionWK=$3
txnsize=$4

if  [ "$optionHOT" = "2" ] && [ "$optionWK" = "A" ]; then
    sed -i '165s/.*/#define POS_HS                           SPECIFIED/g' config.h
    sed -i '173s/.*/#define READ_HOTSPOT_RATIO               0.0/g' config.h
    sed -i '168s/.*/#define NUM_HS                           2/g' config.h
    sed -i '171s/.*/#define FIXED_HS                         0/g' config.h
    sed -i '109s/.*/#define BB_LAST_RETIRE                   0.15/g' config.h
    sed -i '44s/.*/#define CC_ALG 						               '$optionCC'/g' config.h
    sed -i '166s/.*/#define SPECIFIED_RATIO                   0.1/g' config.h

elif  [ "$optionHOT" = "2" ] && [ "$optionWK" = "B" ]; then
    sed -i '165s/.*/#define POS_HS                           SPECIFIED/g' config.h
    sed -i '173s/.*/#define READ_HOTSPOT_RATIO               0.0/g' config.h
    sed -i '168s/.*/#define NUM_HS                           2/g' config.h
    sed -i '171s/.*/#define FIXED_HS                         1/g' config.h
    sed -i '44s/.*/#define CC_ALG 						               '$optionCC'/g' config.h
    sed -i '166s/.*/#define SPECIFIED_RATIO                  1/g' config.h

elif  [ "$optionHOT" = "2" ] && [ "$optionWK" = "C" ]; then
    sed -i '165s/.*/#define POS_HS                           RANDOM/g' config.h
    sed -i '173s/.*/#define READ_HOTSPOT_RATIO               0.0/g' config.h
    sed -i '168s/.*/#define NUM_HS                           2/g' config.h
    sed -i '171s/.*/#define FIXED_HS                         0/g' config.h
    sed -i '109s/.*/#define BB_LAST_RETIRE                   0/g' config.h
    sed -i '166s/.*/#define SPECIFIED_RATIO                  0/g' config.h
    sed -i '44s/.*/#define CC_ALG 						               '$optionCC'/g' config.h

elif  [ "$optionHOT" = "3" ] && [ "$optionWK" = "D" ]; then
    sed -i '165s/.*/#define POS_HS                           RANDOM/g' config.h
    sed -i '173s/.*/#define READ_HOTSPOT_RATIO               0.0/g' config.h
    sed -i '168s/.*/#define NUM_HS                           3/g' config.h
    sed -i '171s/.*/#define FIXED_HS                         0/g' config.h
    sed -i '109s/.*/#define BB_LAST_RETIRE                   0/g' config.h
    sed -i '166s/.*/#define SPECIFIED_RATIO                  0/g' config.h
    sed -i '44s/.*/#define CC_ALG 						               '$optionCC'/g' config.h

else
    echo "Invalid options. Please check your input."
fi


sed -i '21s/.*/#define WORKLOAD 					    YCSB/g' config.h
sed -i '147s/.*/#define SYNTH_TABLE_SIZE 			10000000/g' config.h
sed -i '148s/.*/#define ZIPF_THETA 					  0/g' config.h
sed -i '149s/.*/#define READ_PERC 					  1/g' config.h
sed -i '150s/.*/#define WRITE_PERC 					  0/g' config.h
sed -i '139s/.*/#define MAX_TXN_PER_PART 			'$txnsize'/g' config.h
sed -i '160s/.*/#define LONG_TXN_RATIO              0/g' config.h
sed -i '164s/.*/#define SYNTHETIC_YCSB              true/g' config.h



sed -i '60s/.*/#define ABORT_BUFFER_SIZE           1/g' config.h
sed -i '62s/.*/#define ABORT_BUFFER_ENABLE			   true/g' config.h
sed -i '57s/.*/#define ABORT_PENALTY               100000/g' config.h

sed -i '314s/.*/#define TEST_NUM                       1/g' config.h

sed -i '8s/.*/#define   THREAD_CNT                       20/g' config.h
sed -i '159s/.*/#define REQ_PER_QUERY				             16/g' config.h
sed -i '316s/.*/#define TEST_TARGET                      dynamic/g' config.h


# WOUND_WAIT NO_WAIT WAIT_DIE BAMBOO SILO TICTOC HEKATON HOTSPOT_FRIENDLY

make clean
make -s -j
./rundb

