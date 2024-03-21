#!/bin/bash

# mysqld and sysbench parameters
export INNODB_CACHE=96G
export NUM_TABLES=16
export DATASIZE=10M
export RUN_TIME_SECONDS=300
export THREADS_LIST="8 16 32 64"
# additional mysqld parameters
export MYEXTRA="--sync_binlog=1024 --innodb_flush_log_at_trx_commit=0"

MAIN_DIR=/mnt/fast/username
# path to template databases
export TEMPLATE_PATH=$MAIN_DIR
# path to work directory and results
export WORKSPACE=$MAIN_DIR/perf-results

# path to binaries of MySQL or Percona Server
BUILD_PATH=$MAIN_DIR/release-8.0.36-28@47601f19675-rel-gcc10

# path to files from https://github.com/Percona-QA/perf-tests
PERFTEST_PATH=$MAIN_DIR/perf-tests
CNFFILE_PATH=${PERFTEST_PATH}/cnf/stable-innodb.cnf
export WORKLOAD_SCRIPT=${PERFTEST_PATH}/workloads/read_write.txt

BENCH_PREFIX=nodesm-8036
${PERFTEST_PATH}/ps-performance-test.sh ${BENCH_PREFIX}1_time${RUN_TIME_SECONDS} ${BUILD_PATH} "${CNFFILE_PATH}"
${PERFTEST_PATH}/ps-performance-test.sh ${BENCH_PREFIX}2_time${RUN_TIME_SECONDS} ${BUILD_PATH} "${CNFFILE_PATH}"
${PERFTEST_PATH}/ps-performance-test.sh ${BENCH_PREFIX}3_time${RUN_TIME_SECONDS} ${BUILD_PATH} "${CNFFILE_PATH}"
${PERFTEST_PATH}/ps-performance-test.sh ${BENCH_PREFIX}4_time${RUN_TIME_SECONDS} ${BUILD_PATH} "${CNFFILE_PATH}"
${PERFTEST_PATH}/ps-performance-test.sh ${BENCH_PREFIX}5_time${RUN_TIME_SECONDS} ${BUILD_PATH} "${CNFFILE_PATH}"
