#!/bin/bash


sed -i '21s/.*/#define WORKLOAD 					    YCSB/g' config.h
sed -i '147s/.*/#define SYNTH_TABLE_SIZE 			10000000/g' config.h
sed -i '148s/.*/#define ZIPF_THETA 					  0/g' config.h
sed -i '149s/.*/#define READ_PERC 					  1/g' config.h
sed -i '150s/.*/#define WRITE_PERC 					  0/g' config.h
sed -i '139s/.*/#define MAX_TXN_PER_PART 			10000/g' config.h
sed -i '160s/.*/#define LONG_TXN_RATIO              0/g' config.h
sed -i '164s/.*/#define SYNTHETIC_YCSB              true/g' config.h
sed -i '168s/.*/#define NUM_HS                      3/g' config.h


sed -i '60s/.*/#define ABORT_BUFFER_SIZE           1/g' config.h
sed -i '62s/.*/#define ABORT_BUFFER_ENABLE			   true/g' config.h
sed -i '57s/.*/#define ABORT_PENALTY               100000/g' config.h

sed -i '314s/.*/#define TEST_NUM                       1/g' config.h



# WOUND_WAIT NO_WAIT WAIT_DIE BAMBOO SILO TICTOC HEKATON HOTSPOT_FRIENDLY
cc_name=(HOTSPOT_FRIENDLY)
# vary transaction size    random    3 write     last retire = 0
 sed -i '8s/.*/#define   THREAD_CNT                       20/g' config.h
 sed -i '316s/.*/#define TEST_TARGET                      random3W/g' config.h
 sed -i '165s/.*/#define POS_HS                           RANDOM/g' config.h
 sed -i '173s/.*/#define READ_HOTSPOT_RATIO               0.0/g' config.h
 sed -i '171s/.*/#define FIXED_HS                         0/g' config.h
 sed -i '109s/.*/#define BB_LAST_RETIRE                   0/g' config.h
 sed -i '166s/.*/#define SPECIFIED_RATIO                  0/g' config.h

 # vary transaction size  3, 5, 10, 15, 20, 25, 30
 for((a=0;a<${#cc_name[@]};a++))
 do
   sed -i '44s/.*/#define CC_ALG 						            '${cc_name[$a]}'/g' config.h

   sed -i '159s/.*/#define REQ_PER_QUERY				       3/g' config.h
   make clean
   make -j
   ./rundb

   for((i=5;i<=30;i+=5))
   do
     sed -i '159s/.*/#define REQ_PER_QUERY				     '$i'/g' config.h
     make clean
     make -j
     ./rundb
   done
 done



# HOTSPOT_FRIENDLY  BAMBOO
# cc_name=(BAMBOO)
## vary transaction size    random    3 write     last retire = 0.15
# sed -i '8s/.*/#define   THREAD_CNT                       20/g' config.h
# sed -i '316s/.*/#define TEST_TARGET                      random3W/g' config.h
# sed -i '165s/.*/#define POS_HS                           RANDOM/g' config.h
# sed -i '173s/.*/#define READ_HOTSPOT_RATIO               0.0/g' config.h
# sed -i '171s/.*/#define FIXED_HS                         0/g' config.h
# sed -i '109s/.*/#define BB_LAST_RETIRE                   0.15/g' config.h
# sed -i '166s/.*/#define SPECIFIED_RATIO                  0/g' config.h
#
# for((a=0;a<${#cc_name[@]};a++))
# do
#   sed -i '44s/.*/#define CC_ALG 						            '${cc_name[$a]}'/g' config.h
#
#   sed -i '159s/.*/#define REQ_PER_QUERY				       3/g' config.h
#   make clean
#   make -j
#   ./rundb
#
#   for((i=5;i<=30;i+=5))
#   do
#     sed -i '159s/.*/#define REQ_PER_QUERY				     '$i'/g' config.h
#     make clean
#     make -j
#     ./rundb
#   done
# done


# HOTSPOT_FRIENDLY  BAMBOO
#  cc_name=(HOTSPOT_FRIENDLY)
## vary transaction size    random    50% write     last retire = 0
# sed -i '8s/.*/#define   THREAD_CNT                       20/g' config.h
# sed -i '316s/.*/#define TEST_TARGET                      random3WXR/g' config.h
# sed -i '165s/.*/#define POS_HS                           RANDOM/g' config.h
# sed -i '173s/.*/#define READ_HOTSPOT_RATIO               0.5/g' config.h
# sed -i '171s/.*/#define FIXED_HS                         0/g' config.h
# sed -i '109s/.*/#define BB_LAST_RETIRE                   0/g' config.h
# sed -i '166s/.*/#define SPECIFIED_RATIO                  0/g' config.h
#
# for((a=0;a<${#cc_name[@]};a++))
# do
#   sed -i '44s/.*/#define CC_ALG 						            '${cc_name[$a]}'/g' config.h
#
#   for((i=4;i<=30;i+=4))
#   do
#     sed -i '159s/.*/#define REQ_PER_QUERY				     '$i'/g' config.h
#     make clean
#     make -j
#     ./rundb
#   done
# done


# HOTSPOT_FRIENDLY  BAMBOO
#  cc_name=(BAMBOO)
## vary transaction size    random    50% write      last retire = 0.15
# sed -i '8s/.*/#define   THREAD_CNT                       20/g' config.h
# sed -i '316s/.*/#define TEST_TARGET                      random3WXR/g' config.h
# sed -i '165s/.*/#define POS_HS                           RANDOM/g' config.h
# sed -i '173s/.*/#define READ_HOTSPOT_RATIO               0.5/g' config.h
# sed -i '171s/.*/#define FIXED_HS                         0/g' config.h
# sed -i '109s/.*/#define BB_LAST_RETIRE                   0.15/g' config.h
# sed -i '166s/.*/#define SPECIFIED_RATIO                  0/g' config.h
#
# for((a=0;a<${#cc_name[@]};a++))
# do
#   sed -i '44s/.*/#define CC_ALG 						            '${cc_name[$a]}'/g' config.h
#
#   for((i=4;i<=30;i+=4))
#   do
#     sed -i '159s/.*/#define REQ_PER_QUERY				     '$i'/g' config.h
#     make clean
#     make -j
#     ./rundb
#   done
# done


# HOTSPOT_FRIENDLY  BAMBOO
#  cc_name=(HOTSPOT_FRIENDLY)
### vary transaction size    random    random write     last retire = 0
# sed -i '8s/.*/#define   THREAD_CNT                       20/g' config.h
# sed -i '316s/.*/#define TEST_TARGET                      random3WYR/g' config.h
# sed -i '317s/.*/#define RANDOM_READ_SYNTHETIC_YCSB       true/g' config.h
# sed -i '165s/.*/#define POS_HS                           RANDOM/g' config.h
# sed -i '173s/.*/#define READ_HOTSPOT_RATIO               0.5/g' config.h
# sed -i '171s/.*/#define FIXED_HS                         0/g' config.h
# sed -i '109s/.*/#define BB_LAST_RETIRE                   0/g' config.h
# sed -i '166s/.*/#define SPECIFIED_RATIO                  0/g' config.h
#
# for((a=0;a<${#cc_name[@]};a++))
# do
#   sed -i '44s/.*/#define CC_ALG 						            '${cc_name[$a]}'/g' config.h
#
#   for((i=4;i<=30;i+=4))
#   do
#     sed -i '159s/.*/#define REQ_PER_QUERY				     '$i'/g' config.h
#     make clean
#     make -j
#     ./rundb
#   done
# done