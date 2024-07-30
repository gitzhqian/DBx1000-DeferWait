#! /bin/bash

# cc_name=(NO_WAIT WAIT_DIE HEKATON SILO TICTOC SLER)

sed -i '21s/.*/#define WORKLOAD 					          TPCC/g' config.h
sed -i '217s/.*/#define TPCC_USER_ABORT             true/g' config.h
sed -i '139s/.*/#define MAX_TXN_PER_PART 			      10000/g' config.h

sed -i '60s/.*/#define ABORT_BUFFER_SIZE           1/g' config.h
sed -i '62s/.*/#define ABORT_BUFFER_ENABLE			   true/g' config.h
sed -i '57s/.*/#define ABORT_PENALTY               100000/g' config.h

sed -i '109s/.*/#define BB_LAST_RETIRE                   0/g' config.h

# default does not statistic the blind wound in Bamboo
sed -i '179s/.*/#define TEST_BB_ABORT                    false/g' config.h

sed -i '188s/.*/#define TPCC_SMALL                       true/g' config.h


# WOUND_WAIT NO_WAIT WAIT_DIE BAMBOO SILO TICTOC HEKATON HOTSPOT_FRIENDLY
#cc_name=(HOTSPOT_FRIENDLY)
#sed -i '316s/.*/#define TEST_TARGET                       tpccthreads/g' config.h
#sed -i '207s/.*/#define PERC_PAYMENT                      0.5/g' config.h
#sed -i '194s/.*/#define NUM_WH 						                1/g' config.h
#sed -i '318s/.*/#define PERC_QUERY2 						          false/g' config.h
#sed -i '320s/.*/#define OL_CNT_ST 						            5/g' config.h
#sed -i '321s/.*/#define OL_CNT_ED 						            15/g' config.h
#
##vary threads 1, 10, 20, 30, 40
#for((a=0;a<${#cc_name[@]};a++))
#do
# sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#
# sed -i '8s/.*/#define THREAD_CNT					1/g' config.h
# make -j
# ./rundb
#
# for((i=10;i<=40;i+=10))
# do
#   sed -i '8s/.*/#define THREAD_CNT					'$i'/g' config.h
#   make -j
#   ./rundb
# done
#done


# WOUND_WAIT NO_WAIT WAIT_DIE BAMBOO SILO TICTOC HEKATON HOTSPOT_FRIENDLY
 cc_name=(HOTSPOT_FRIENDLY)
 sed -i '316s/.*/#define TEST_TARGET                       tpccwhousesitm/g' config.h
 sed -i '207s/.*/#define PERC_PAYMENT                      0.5/g' config.h
 sed -i '194s/.*/#define NUM_WH 						                1/g' config.h
 sed -i '318s/.*/#define PERC_QUERY2 						          false/g' config.h
 sed -i '8s/.*/#define   THREAD_CNT					                20/g' config.h

 #vary order line count in one new order
 ol_st=(5 15 25 35 45)
 ol_ed=(15 25 35 45 55)
 for((a=0;a<${#cc_name[@]};a++))
 do
  sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h

  for((i=0;i<${#ol_st[@]};i++))
  do
    sed -i '320s/.*/#define OL_CNT_ST					    '${ol_st[$i]}'/g' config.h
    sed -i '321s/.*/#define OL_CNT_ED 					  '${ol_ed[$i]}'/g' config.h
    make -j
    ./rundb
  done
 done


# WOUND_WAIT NO_WAIT WAIT_DIE BAMBOO SILO TICTOC HEKATON HOTSPOT_FRIENDLY
# cc_name=(HOTSPOT_FRIENDLY)
# sed -i '316s/.*/#define TEST_TARGET                       tpccwhouses/g' config.h
# sed -i '207s/.*/#define PERC_PAYMENT                      0.5/g' config.h
# sed -i '8s/.*/#define THREAD_CNT					                 20/g' config.h
# sed -i '318s/.*/#define PERC_QUERY2 						           false/g' config.h
# sed -i '320s/.*/#define OL_CNT_ST 						             5/g' config.h
# sed -i '321s/.*/#define OL_CNT_ED 						             15/g' config.h
#
# #vary warehouse 2, 4, 8, 16, 32, 40
# for((a=0;a<${#cc_name[@]};a++))
# do
#   sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#   for((i=2;i<=40;i*=2))
#   do
#     sed -i '194s/.*/#define NUM_WH 						'$i'/g' config.h
#     make -j
#     ./rundb
#   done
#
#   sed -i '194s/.*/#define NUM_WH 						  40/g' config.h
#   make -j
#   ./rundb
# done






#sed -i '316s/.*/#define TEST_TARGET                       tpccthreads2q/g' config.h
#sed -i '207s/.*/#define PERC_PAYMENT                      0.5/g' config.h
#sed -i '194s/.*/#define NUM_WH 						                1/g' config.h
#sed -i '318s/.*/#define PERC_QUERY2 						          true/g' config.h
#
#for((a=0;a<${#cc_name[@]};a++))
#do
#  sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#
#  for((i=5;i<=40;i+=5))
#  do
#    sed -i '8s/.*/#define THREAD_CNT					'$i'/g' config.h
#    make -j
#    ./rundb
#  done
#done


#sed -i '316s/.*/#define TEST_TARGET                       tpccwhouses2q/g' config.h
#sed -i '207s/.*/#define PERC_PAYMENT                      0.5/g' config.h
#sed -i '8s/.*/#define THREAD_CNT					                20/g' config.h
#sed -i '318s/.*/#define PERC_QUERY2 						          true/g' config.h
#
#for((a=0;a<${#cc_name[@]};a++))
#do
#  sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#
#  for((i=2;i<=20;i+=2))
#  do
#    sed -i '194s/.*/#define NUM_WH 						'$i'/g' config.h
#    make -j
#    ./rundb
#  done
#done


# HOTSPOT_FRIENDLY  BAMBOO
#cc_name=(HOTSPOT_FRIENDLY)
#sed -i '316s/.*/#define TEST_TARGET                       tpccthreads2/g' config.h
#sed -i '207s/.*/#define PERC_PAYMENT                      0.2/g' config.h
#sed -i '194s/.*/#define NUM_WH 						                1/g' config.h
#sed -i '318s/.*/#define PERC_QUERY2 						          false/g' config.h
#
#for((a=0;a<${#cc_name[@]};a++))
#do
#  sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#
#  for((i=25;i<=40;i+=5))
#  do
#    sed -i '8s/.*/#define THREAD_CNT					'$i'/g' config.h
#    make -j
#    ./rundb
#  done
#done


# HOTSPOT_FRIENDLY  BAMBOO
#cc_name=(BAMBOO)
#sed -i '316s/.*/#define TEST_TARGET                       tpccwhouses2/g' config.h
#sed -i '207s/.*/#define PERC_PAYMENT                      0.2/g' config.h
#sed -i '8s/.*/#define THREAD_CNT					                20/g' config.h
#sed -i '318s/.*/#define PERC_QUERY2 						          false/g' config.h
#
#for((a=0;a<${#cc_name[@]};a++))
#do
#  sed -i '44s/.*/#define CC_ALG 						'${cc_name[$a]}'/g' config.h
#
#  for((i=2;i<=20;i+=2))
#  do
#    sed -i '194s/.*/#define NUM_WH 						'$i'/g' config.h
#    make -j
#    ./rundb
#  done
#done

