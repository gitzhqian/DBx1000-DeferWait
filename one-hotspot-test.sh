#!/bin/bash
# WOUND_WAIT NO_WAIT WAIT_DIE BAMBOO SILO TICTOC HEKATON HOTSPOT_FRIENDLY
cc_name=(BAMBOO)
hot_pos=(0 0.25 0.5 0.75 1)

sed -i '21s/.*/#define WORKLOAD 					    YCSB/g' config.h
sed -i '147s/.*/#define SYNTH_TABLE_SIZE 			10000000/g' config.h
sed -i '148s/.*/#define ZIPF_THETA 					  0/g' config.h
sed -i '149s/.*/#define READ_PERC 					  1/g' config.h
sed -i '150s/.*/#define WRITE_PERC 					  0/g' config.h
sed -i '139s/.*/#define MAX_TXN_PER_PART 			10000/g' config.h
sed -i '160s/.*/#define LONG_TXN_RATIO              0/g' config.h
sed -i '164s/.*/#define SYNTHETIC_YCSB              true/g' config.h
sed -i '168s/.*/#define NUM_HS                      1/g' config.h
sed -i '173s/.*/#define READ_HOTSPOT_RATIO          0.0/g' config.h

sed -i '60s/.*/#define ABORT_BUFFER_SIZE           1/g' config.h
sed -i '62s/.*/#define ABORT_BUFFER_ENABLE			   true/g' config.h
sed -i '57s/.*/#define ABORT_PENALTY               100000/g' config.h

sed -i '314s/.*/#define TEST_NUM                       1/g' config.h



#sed -i '159s/.*/#define REQ_PER_QUERY				           16/g' config.h
#sed -i '316s/.*/#define TEST_TARGET                    threads1W/g' config.h
#sed -i '165s/.*/#define POS_HS                         RANDOM/g' config.h
## vary thread number
## 1, 10, 20, 30, 40
# for ((a=0; a<${#cc_name[@]}; a++))
# do
#   sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#
#   sed -i '8s/.*/#define THREAD_CNT					 1/g' config.h
#   make clean
#   make -j
#   ./rundb
#
#   for ((i=10; i<=40; i+=10))
#   do
#     sed -i '8s/.*/#define THREAD_CNT					'$i'/g' config.h
#     make clean
#     make -j
#     ./rundb
#   done
# done



#sed -i '8s/.*/#define THREAD_CNT                       20/g' config.h
#sed -i '316s/.*/#define TEST_TARGET                    txnsize1W/g' config.h
#sed -i '165s/.*/#define POS_HS                         RANDOM/g' config.h
#
### vary transaction size
### 1, 5, 10, 15, 20, 25
#for((a=0;a<${#cc_name[@]};a++))
#do
#  sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#
#  sed -i '159s/.*/#define REQ_PER_QUERY				1/g' config.h
#  make clean
#  make -j
#  ./rundb
#
#  for((i=5;i<=30;i+=5))
#  do
#    sed -i '159s/.*/#define REQ_PER_QUERY				'$i'/g' config.h
#    make -j
#    ./rundb
#  done
#done




sed -i '8s/.*/#define   THREAD_CNT                       20/g' config.h
sed -i '159s/.*/#define REQ_PER_QUERY				             16/g' config.h
sed -i '316s/.*/#define TEST_TARGET                      position1W/g' config.h
sed -i '165s/.*/#define POS_HS                           SPECIFIED/g' config.h

## vary hotspot position
## 0.0, 0.25, 0.5, 0.75, 1
for((a=0;a<${#cc_name[@]};a++))
do
  sed -i '44s/.*/#define CC_ALG 						            '${cc_name[$a]}'/g' config.h

  for((i=0;i<${#hot_pos[@]};i++))
  do
    sed -i '166s/.*/#define SPECIFIED_RATIO             '${hot_pos[$i]}'/g' config.h
    make clean
    make -j
    ./rundb
  done
done