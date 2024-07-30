#! /bin/bash


zip_theta=(0 0.5 0.7 0.8 0.9 0.99)
read_ratio=(0.1 0.3 0.5 0.7 0.9)
write_ratio=(0.9 0.7 0.5 0.3 0.1)

sed -i '21s/.*/#define  WORKLOAD 					    YCSB/g' config.h
sed -i '147s/.*/#define SYNTH_TABLE_SIZE 			100000000/g' config.h
# sed -i '149s/.*/#define READ_PERC 					  0.5/g' config.h
# sed -i '150s/.*/#define WRITE_PERC 					  0.5/g' config.h
sed -i '159s/.*/#define REQ_PER_QUERY				  16/g' config.h
sed -i '139s/.*/#define MAX_TXN_PER_PART 			10000/g' config.h
sed -i '160s/.*/#define LONG_TXN_RATIO              0/g' config.h
sed -i '164s/.*/#define SYNTHETIC_YCSB              false/g' config.h


sed -i '60s/.*/#define ABORT_BUFFER_SIZE           1/g' config.h
sed -i '62s/.*/#define ABORT_BUFFER_ENABLE			   true/g' config.h
sed -i '57s/.*/#define ABORT_PENALTY               100000/g' config.h



## vary read ratio
# WOUND_WAIT NO_WAIT WAIT_DIE BAMBOO SILO TICTOC HEKATON HOTSPOT_FRIENDLY
#cc_name=(HOTSPOT_FRIENDLY)
#sed -i '109s/.*/#define BB_LAST_RETIRE                   0/g' config.h
#sed -i '8s/.*/#define   THREAD_CNT                       20/g' config.h
#sed -i '316s/.*/#define TEST_TARGET                      ycsbratios/g' config.h
#sed -i '148s/.*/#define ZIPF_THETA 					             0.9/g' config.h
#
#for((a=0;a<${#cc_name[@]};a++))
#do
#  sed -i '44s/.*/#define CC_ALG 						    '${cc_name[$a]}'/g' config.h
#
#  for((i=0;i<${#read_ratio[@]};i++))
#  do
#    sed -i '149s/.*/#define READ_PERC  					'${read_ratio[$i]}'/g' config.h
#    sed -i '150s/.*/#define WRITE_PERC 					'${write_ratio[$i]}'/g' config.h
#    make clean
#    make -j
#    ./rundb
#  done
#done

## vary zif theta
# WOUND_WAIT NO_WAIT WAIT_DIE BAMBOO SILO TICTOC HEKATON HOTSPOT_FRIENDLY
#cc_name=(HOTSPOT_FRIENDLY)
#sed -i '109s/.*/#define BB_LAST_RETIRE                   0/g' config.h
#sed -i '8s/.*/#define   THREAD_CNT                       20/g' config.h
#sed -i '316s/.*/#define TEST_TARGET                      ycsbzif/g' config.h
#sed -i '149s/.*/#define READ_PERC 					             0.5/g' config.h
#sed -i '150s/.*/#define WRITE_PERC 					             0.5/g' config.h
#
#for((a=0;a<${#cc_name[@]};a++))
#do
#  sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#
#  for((i=0;i<${#zip_theta[@]};i++))
#  do
#    sed -i '148s/.*/#define ZIPF_THETA 					'${zip_theta[$i]}'/g' config.h
#    make clean
#    make -j
#    ./rundb
#  done
#done



## vary threads
#  WOUND_WAIT NO_WAIT WAIT_DIE BAMBOO SILO TICTOC HEKATON HOTSPOT_FRIENDLY
 cc_name=(HOTSPOT_FRIENDLY)
 sed -i '148s/.*/#define ZIPF_THETA 					             0.9/g' config.h
 sed -i '316s/.*/#define TEST_TARGET                       ycsbthreads/g' config.h
 sed -i '109s/.*/#define BB_LAST_RETIRE                    0/g' config.h
 sed -i '149s/.*/#define READ_PERC 					               0.5/g' config.h
 sed -i '150s/.*/#define WRITE_PERC 					               0.5/g' config.h

 #1, 10, 20, 30, 40
 for((a=0;a<${#cc_name[@]};a++))
 do
   sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h

   sed -i '8s/.*/#define THREAD_CNT					1/g' config.h
   make clean
   make -j
   ./rundb

   for((i=10;i<=40;i+=10))
   do
     sed -i '8s/.*/#define THREAD_CNT					'$i'/g' config.h
     make clean
     make -j
     ./rundb
   done
 done




## vary zif theta
# HOTSPOT_FRIENDLY  BAMBOO
#cc_name=(BAMBOO)
#sed -i '109s/.*/#define BB_LAST_RETIRE                   0.15/g' config.h
#sed -i '8s/.*/#define   THREAD_CNT                       20/g' config.h
#sed -i '316s/.*/#define TEST_TARGET                      ycsbzif/g' config.h
#
#for((a=0;a<${#cc_name[@]};a++))
#do
#  sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#
#  for((i=0;i<${#zip_theta[@]};i++))
#  do
#    sed -i '148s/.*/#define ZIPF_THETA 					'${zip_theta[$i]}'/g' config.h
#    make clean
#    make -j
#    ./rundb
#  done
#done



## vary threads
# HOTSPOT_FRIENDLY  BAMBOO
#cc_name=(BAMBOO)
#sed -i '148s/.*/#define ZIPF_THETA 					             0.9/g' config.h
#sed -i '316s/.*/#define TEST_TARGET                      ycsbthreads/g' config.h
#sed -i '109s/.*/#define BB_LAST_RETIRE                   0.15/g' config.h
#
#for((a=0;a<${#cc_name[@]};a++))
#do
#  sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#
#  for((i=5;i<=30;i+=5))
#  do
#    sed -i '8s/.*/#define THREAD_CNT					'$i'/g' config.h
#    make clean
#    make -j
#    ./rundb
#  done
#done


