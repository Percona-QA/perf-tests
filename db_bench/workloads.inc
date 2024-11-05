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
)
