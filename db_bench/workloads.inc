#!/bin/echo This script should be sourced in a shell, not executed directly

#**********************************************************************************************
# Percona Database Benchmark scripts
# workloads.inc - parameters for sysbench workloads
#**********************************************************************************************

# Define aliases for workloads
declare -A aliases
aliases["reads"]="POINT_SELECT,SELECT_RANDOM_POINTS,SELECT_HOT_POINTS,POINTS_COVERED,POINTS_COVERED_SI,POINTS_NOTCOVERED,POINTS_NOTCOVERED_SI,RANGE_COVERED,RANGE_COVERED_SI,RANGE_NOTCOVERED,RANGE_NOTCOVERED_SI,SIMPLE_RANGES,SUM_RANGES,ORDER_RANGES,DISTINCT_RANGES,COUNT_RANGES"
aliases["writes"]="WR_INLIST_UPDATE,WR_INLIST_UPDATE_HOT_POINTS,WR_UPDATE_INDEX,WR_UPDATE_INDEX_LIMIT,WR_UPDATE_NON_INDEX,WR_DELETE_INSERTS,WR_DELETE,WR_INSERT,WR_REPLACE"
aliases["make_snapshot"]="INLIST_UPDATE,UPDATE_INDEX,UPDATE_NON_INDEX,DELETE_INSERTS,snapshot"

# Aliases for windmills
aliases["select_run"]="SELECT_RUN_POINT_SELECT,SELECT_RUN_RANGE_SIMPLE,SELECT_RUN_RANGE_SUM,SELECT_RUN_RANGE_ORDER,SELECT_RUN_RANGE_DISTINCT,SELECT_RUN_RANGE_ALL,SELECT_RUN_INLIST,SELECT_RUN_INLIST_HOTSPOT"
aliases["write_run"]="WRITE_RUN_INLIST_UPDATE,WRITE_RUN_INLIST_UPDATE_HOTSPOT,WRITE_RUN_UPDATE_NO_INDEX_SINGLE,WRITE_RUN_UPDATE_NO_INDEX_MULTI,WRITE_RUN_UPDATE_NO_INDEX_MULTI_SPECIAL,WRITE_RUN_UPDATE_WITH_INDEX_SINGLE,WRITE_RUN_UPDATE_WITH_INDEX_MULTI,WRITE_RUN_UPDATE_WITH_INDEX_MULTI_SPECIAL,WRITE_RUN_INSERT_DELETE_SINGLE,WRITE_RUN_INSERT_DELETE_MULTI,WRITE_RUN_REPLACE_DELETE_SINGLE,WRITE_RUN_REPLACE_DELETE_MULTI,WRITE_RUN_WRITE_ALL_NO_TRX,WRITE_RUN_WRITE_ALL_WITH_TRX,WRITE_RUN_WRITE_ALL_WITH_TRX_SPECIAL,WRITE_RUN_RW_WITH_RANGE_100,WRITE_RUN_RW_WITH_RANGE_1000"
aliases["write_single"]="WRITE_RUN_INLIST_UPDATE,WRITE_RUN_INLIST_UPDATE_HOTSPOT,WRITE_RUN_UPDATE_NO_INDEX_SINGLE,WRITE_RUN_UPDATE_WITH_INDEX_SINGLE,WRITE_RUN_INSERT_DELETE_SINGLE,WRITE_RUN_REPLACE_DELETE_SINGLE"
aliases["windmills_snapshot"]="RUN_INLIST_UPDATE,RUN_INLIST_UPDATE_HOTSPOT,RUN_UPDATE_NO_INDEX_SINGLE,RUN_UPDATE_WITH_INDEX_SINGLE,RUN_INSERT_DELETE_SINGLE,RUN_REPLACE_DELETE_SINGLE,snapshot"

declare -A load=(
# reads
POINT_SELECT         "oltp_point_select.lua"
SELECT_RANDOM_POINTS "oltp_inlist_select.lua"
SELECT_HOT_POINTS    "oltp_inlist_select.lua --hot-points"
POINTS_COVERED       "oltp_points_covered.lua"
POINTS_COVERED_SI    "oltp_points_covered.lua --on-id=false"
POINTS_NOTCOVERED    "oltp_points_covered.lua --covered=false"
POINTS_NOTCOVERED_SI "oltp_points_covered.lua --on-id=false --covered=false"
RANGE_COVERED        "oltp_range_covered.lua"
RANGE_COVERED_SI     "oltp_range_covered.lua --on-id=false"
RANGE_NOTCOVERED     "oltp_range_covered.lua --covered=false"
RANGE_NOTCOVERED_SI  "oltp_range_covered.lua --on-id=false --covered=false"
SIMPLE_RANGES        "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=1 --sum-ranges=0 --order-ranges=0 --distinct-ranges=0"
SUM_RANGES           "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=1 --order-ranges=0 --distinct-ranges=0"
ORDER_RANGES         "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=0 --order-ranges=1 --distinct-ranges=0"
DISTINCT_RANGES      "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=0 --order-ranges=0 --distinct-ranges=1"
COUNT_RANGES         "oltp_read_only_count.lua --skip-trx=on"

# writes (write workloads should start with the "WR_" prefix to reset the database to its original state after writes have been made)
WR_INLIST_UPDATE            "oltp_inlist_update.lua"
WR_INLIST_UPDATE_HOT_POINTS "oltp_inlist_update.lua --hot_points=true"
WR_UPDATE_INDEX             "oltp_update_index.lua"
WR_UPDATE_INDEX_LIMIT       "oltp_update_index_rl.lua"
WR_UPDATE_NON_INDEX         "oltp_update_non_index.lua"
WR_DELETE_INSERTS           "oltp_write_only.lua --index-updates=0 --non-index-updates=0"
WR_DELETE                   "oltp_delete.lua"
WR_INSERT                   "oltp_insert.lua"
WR_REPLACE                  "oltp_replace.lua --skip-trx=on"

#random-points 10 100 1000 (oltp_inlist_select) --random_points=10/100/1000 (default=10)
SELECT_RANDOM_POINTS_1   "oltp_inlist_select.lua --random_points=1"
SELECT_RANDOM_POINTS_8   "oltp_inlist_select.lua --random_points=8"
SELECT_RANDOM_POINTS_32  "oltp_inlist_select.lua --random_points=32"
SELECT_RANDOM_POINTS_128 "oltp_inlist_select.lua --random_points=128"
SELECT_HOT_POINTS_1      "oltp_inlist_select.lua --random_points=1 --hot-points=true"
SELECT_HOT_POINTS_8      "oltp_inlist_select.lua --random_points=8 --hot-points=true"
SELECT_HOT_POINTS_32     "oltp_inlist_select.lua --random_points=32 --hot-points=true"
SELECT_HOT_POINTS_128    "oltp_inlist_select.lua --random_points=128 --hot-points=true"

#read-only 10 100 1000 10000 (prepare_simple_ranges prepare_sum_ranges prepare_order_ranges prepare_order_ranges) --range_size=10/100/1000/10000 (default=100)
#read-only_count 1 10 100 1000 (oltp_read_only_count.lua) --count_ranges=1 --range_size=1/10/100/1000 (default=100)
SIMPLE_RANGES_10     "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=1 --sum-ranges=0 --order-ranges=0 --distinct-ranges=0 --range_size=10"
SIMPLE_RANGES_100    "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=1 --sum-ranges=0 --order-ranges=0 --distinct-ranges=0 --range_size=100"
SIMPLE_RANGES_1000   "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=1 --sum-ranges=0 --order-ranges=0 --distinct-ranges=0 --range_size=1000"
SUM_RANGES_10        "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=1 --order-ranges=0 --distinct-ranges=0 --range_size=10"
SUM_RANGES_100       "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=1 --order-ranges=0 --distinct-ranges=0 --range_size=100"
SUM_RANGES_1000      "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=1 --order-ranges=0 --distinct-ranges=0 --range_size=1000"
ORDER_RANGES_10      "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=0 --order-ranges=1 --distinct-ranges=0 --range_size=10"
ORDER_RANGES_100     "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=0 --order-ranges=1 --distinct-ranges=0 --range_size=100"
ORDER_RANGES_1000    "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=0 --order-ranges=1 --distinct-ranges=0 --range_size=1000"
DISTINCT_RANGES_10   "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=0 --order-ranges=0 --distinct-ranges=1 --range_size=10"
DISTINCT_RANGES_100  "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=0 --order-ranges=0 --distinct-ranges=1 --range_size=100"
DISTINCT_RANGES_1000 "oltp_read_only.lua --skip-trx=on --point-selects=0 --simple-ranges=0 --sum-ranges=0 --order-ranges=0 --distinct-ranges=1 --range_size=1000"
COUNT_RANGES_10      "oltp_read_only_count.lua --skip-trx=on --range_size=10"
COUNT_RANGES_100     "oltp_read_only_count.lua --skip-trx=on --range_size=100"
COUNT_RANGES_1000    "oltp_read_only_count.lua --skip-trx=on --range_size=1000"

# writes (write workloads should start with the "WR_" prefix to reset the database to its original state after writes have been made)
WR_INLIST_UPDATE_1              "oltp_inlist_update.lua --random_points=1"
WR_INLIST_UPDATE_8              "oltp_inlist_update.lua --random_points=8"
WR_INLIST_UPDATE_32             "oltp_inlist_update.lua --random_points=32 --reset_binlog=64000000"
WR_INLIST_UPDATE_128            "oltp_inlist_update.lua --random_points=128 --reset_binlog=64000000"
WR_INLIST_UPDATE_HOT_POINTS_1   "oltp_inlist_update.lua --random_points=1 --hot_points=true"
WR_INLIST_UPDATE_HOT_POINTS_8   "oltp_inlist_update.lua --random_points=8 --hot_points=true"
WR_INLIST_UPDATE_HOT_POINTS_32  "oltp_inlist_update.lua --random_points=32 --hot_points=true --reset_binlog=64000000"
WR_INLIST_UPDATE_HOT_POINTS_128 "oltp_inlist_update.lua --random_points=128 --hot_points=true --reset_binlog=64000000"

# additional commands
snapshot "options are ignored"
reset    "options are ignored"


#fill tables
CLEAN_PREPARE_SMALL       "windmills/oltp_write.lua --skip_trx=on"
CLEAN_PREPARE_LARGE       "windmills/oltp_write.lua --tables=5 --skip_trx=on"

#point select
SELECT_RUN_POINT_SELECT   "windmills/oltp_point_select.lua --skip_trx=on"

#Select Range
SELECT_RUN_RANGE_SIMPLE   "windmills/oltp_range_select.lua --skip_trx=on --type_of_range=simple"
SELECT_RUN_RANGE_SUM      "windmills/oltp_range_select.lua --skip_trx=on --type_of_range=sum"
SELECT_RUN_RANGE_ORDER    "windmills/oltp_range_select.lua --skip_trx=on --type_of_range=order"
SELECT_RUN_RANGE_DISTINCT "windmills/oltp_range_select.lua --skip_trx=on --type_of_range=distinct"
SELECT_RUN_RANGE_ALL      "windmills/oltp_range_select.lua --skip_trx=on --type_of_range=all"

#in list
SELECT_RUN_INLIST         "windmills/oltp_inlist_select.lua --skip_trx=on --random_points=100 --hot_points=false"
SELECT_RUN_INLIST_HOTSPOT "windmills/oltp_inlist_select.lua --skip_trx=on --random_points=100 --hot_points=true"

#select scan
SELECT_RUN_SELECT_SCAN    "windmills/oltp_scan.lua --skip_trx=on --events=2"

#Sysbench Write only tests
WRITE_RUN_INLIST_UPDATE         "windmills/oltp_inlist_update.lua --skip_trx=on --random_points=100 --hot_points=false"
WRITE_RUN_INLIST_UPDATE_HOTSPOT "windmills/oltp_inlist_update.lua --skip_trx=on --random_points=100 --hot_points=true"

#update no indexed
WRITE_RUN_UPDATE_NO_INDEX_SINGLE        "windmills/oltp_update_non_index.lua --skip_trx=on --non_index_updates=1"
WRITE_RUN_UPDATE_NO_INDEX_MULTI         "windmills/oltp_update_non_index.lua --skip_trx=on --non_index_updates=100"
WRITE_RUN_UPDATE_NO_INDEX_MULTI_SPECIAL "windmills/oltp_update_non_index.lua --skip_trx=on --rand-type=special --non_index_updates=10"

#update with indexed
WRITE_RUN_UPDATE_WITH_INDEX_SINGLE        "windmills/oltp_update_index.lua --skip_trx=on --index_updates=1"
WRITE_RUN_UPDATE_WITH_INDEX_MULTI         "windmills/oltp_update_index.lua --skip_trx=on --index_updates=100"
WRITE_RUN_UPDATE_WITH_INDEX_MULTI_SPECIAL "windmills/oltp_update_index.lua --skip_trx=on --rand-type=special --index_updates=10"

#Insert/deletes
WRITE_RUN_INSERT_DELETE_SINGLE  "windmills/oltp_delete_insert.lua --skip_trx=on --delete_inserts=1"
WRITE_RUN_INSERT_DELETE_MULTI   "windmills/oltp_delete_insert.lua --skip_trx=on --delete_inserts=100"
WRITE_RUN_REPLACE_DELETE_SINGLE "windmills/oltp_insert.lua --skip_trx=on --use_replace --delete_inserts=1"
WRITE_RUN_REPLACE_DELETE_MULTI  "windmills/oltp_insert.lua --skip_trx=on --use_replace --delete_inserts=100"

#All write operations at once
WRITE_RUN_WRITE_ALL_NO_TRX           "windmills/oltp_write.lua --skip_trx=on --delete_inserts=1"
WRITE_RUN_WRITE_ALL_WITH_TRX         "windmills/oltp_write.lua --skip_trx=off --delete_inserts=1"
WRITE_RUN_WRITE_ALL_WITH_TRX_SPECIAL "windmills/oltp_write.lua --skip_trx=off --rand-type=special --delete_inserts=1"

#Read and Write with range selects
WRITE_RUN_RW_WITH_RANGE_100  "windmills/oltp_read_write.lua --skip_trx=on --point_selects=1 --range_size=100"
WRITE_RUN_RW_WITH_RANGE_1000 "windmills/oltp_read_write.lua --skip_trx=on --point_selects=1 --range_size=1000"

#10% Writes 90% Select writes with Reads with and without transactions
WRITE_RUN_RW_25%_WRITES_NOTRX "windmills/oltp_read_write.lua --skip_trx=on --point_selects=180 --range_size=180 --index_updates=20 --non_index_updates=10 --delete_inserts=10"

WRITE_RUN_RW_25%_WRITES_TRX "windmills/oltp_read_write.lua --skip_trx=off --point_selects=180 --range_size=180 --index_updates=20 --non_index_updates=10 --delete_inserts=10"


#25% Writes 75% Select writes with Reads with and without transactions
WRITE_RUN_RW_25%_WRITES_NOTRX "windmills/oltp_read_write.lua --skip_trx=on --point_selects=150 --range_size=150 --index_updates=50 --non_index_updates=25 --delete_inserts=25"

WRITE_RUN_RW_25%_WRITES_TRX "windmills/oltp_read_write.lua --skip_trx=off --point_selects=150 --range_size=150 --index_updates=50 --non_index_updates=25 --delete_inserts=25"



#50% Writes 50% Select writes with Reads with and without transactions
WRITE_RUN_RW_50%_WRITES_NOTRX "windmills/oltp_read_write.lua --skip_trx=on --point_selects=100 --range_size=100 --index_updates=120 --non_index_updates=40 --delete_inserts=40"

WRITE_RUN_RW_50%_WRITES_TRX "windmills/oltp_read_write.lua --skip_trx=off --point_selects=100 --range_size=100 --index_updates=120 --non_index_updates=40 --delete_inserts=40"


#75% Writes 25% Select writes with Reads with and without transactions
WRITE_RUN_RW_75%_WRITES_NOTRX "windmills/oltp_read_write.lua --skip_trx=on --point_selects=50 --range_size=50 --index_updates=160 --non_index_updates=70 --delete_inserts=70"

WRITE_RUN_RW_75%_WRITES_TRX "windmills/oltp_read_write.lua --skip_trx=off --point_selects=50 --range_size=50 --index_updates=150 --non_index_updates=70 --delete_inserts=70"

#WARMUP action
WARMUP_RUN_SELECT_SCAN "windmills/oltp_scan.lua --skip_trx=on"


#Cleanup action
CLEANUP_RUN_SELECT_SCAN "windmills/oltp_scan.lua --skip_trx=on"
)
