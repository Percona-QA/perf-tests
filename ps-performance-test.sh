#!/bin/bash
# set -x

# directories
export WORKSPACE=${WORKSPACE:-${PWD}}
export TEMPLATE_PATH=${TEMPLATE_PATH:-${WORKSPACE}/template_datadir}
export CACHE_DIR=${CACHE_DIR:-${WORKSPACE}/results_cache}
export BENCH_DIR=${BENCH_DIR:-${WORKSPACE}}
export DATA_DIR=${DATA_DIR:-${WORKSPACE}}
BENCH_DIR=${BENCH_DIR}/${BENCH_NAME}
DATA_DIR=${DATA_DIR}/${BENCH_NAME}-datadir
SCRIPT_DIR=$(cd $(dirname $0) && pwd)

# generic variables
export RPORT=$(( RANDOM%21 + 10 ))
export RBASE="$(( RPORT*1000 ))"
export BENCHMARK_LOGGING=${BENCHMARK_LOGGING:-Y}
export SMART_DEVICE=${SMART_DEVICE:-/dev/nvme0n1}
export WORKLOAD_NAMES=${WORKLOAD_NAMES:-reads,writes}

# sysbench variables
export MYSQL_DATABASE=test
export SUSER=root
RAND_TYPE=${RAND_TYPE:-uniform}
RAND_SEED=${RAND_SEED:-1111}
export THREADS_LIST=${THREADS_LIST:-"1 4 16 64 128 256 512 1024"}
SYSBENCH_REPORT_INTERVAL=${SYSBENCH_REPORT_INTERVAL:-10}
SYSBENCH_BIN=${SYSBENCH_BIN:-sysbench}
SYSBENCH_LUA=${SYSBENCH_LUA:-/usr/local/share/sysbench}
SYSBENCH_WRITE=${SYSBENCH_WRITE:-oltp_write_only.lua}
SYSBENCH_READ=${SYSBENCH_READ:-oltp_read_only.lua}
export EVENTS_MULT=${EVENTS_MULT:-1}

# time variables
export PS_START_TIMEOUT=${PS_START_TIMEOUT:-180}
WORKLOAD_WARMUP_TIME=${WORKLOAD_WARMUP_TIME:-0}
export WARMUP_TIME_SECONDS=${WARMUP_TIME_SECONDS:-0}
export WRITES_TIME_SECONDS=${WRITES_TIME_SECONDS:-${RUN_TIME_SECONDS:-600}}
export READS_TIME_SECONDS=${READS_TIME_SECONDS:-$((WRITES_TIME_SECONDS / 2))} # optimization: spend only half of given time for reads
export REPORT_INTERVAL=10
export IOSTAT_INTERVAL=10
export DSTAT_INTERVAL=10

#MYEXTRA=${MYEXTRA:=--disable-log-bin}
#PERF_EXTRA=${PERF_EXTRA:=--performance-schema-instrument='wait/synch/mutex/innodb/%=ON'}

#TASKSET_MYSQLD=${TASKSET_MYSQLD:=taskset -c 0}
#TASKSET_SYSBENCH=${TASKSET_SYSBENCH:=taskset -c 1}

source ${SCRIPT_DIR}/db_bench/data_funcs.inc
source ${SCRIPT_DIR}/db_bench/main_funcs.inc
source ${SCRIPT_DIR}/db_bench/system_funcs.inc

db_bench_init

for file in $CONFIG_FILES; do
    MYSQL_CONFIG_FILE=$file
    db_bench_init_config

    for ((num=0; num<${#WORKLOAD_ARRAY[@]}; num++)); do
        WORKLOAD_NAME=${WORKLOAD_ARRAY[num]}
        WORKLOAD_PARAMETERS=$(eval echo ${WORKLOAD_PARAMS[num]})

        if [[ $num -eq 0 || ${PREV_WORKLOAD_NAME:0:3} == "WR_" ]]; then
            drop_caches
            prepare_datadir | tee ${LOGS_CONFIG}/prepare_datadir_${WORKLOAD_NAME}.log
        fi
        PREV_WORKLOAD_NAME=${WORKLOAD_NAME}

        if [[ ${WORKLOAD_NAME:0:3} != "WR_" ]]; then
            SYSBENCH_RUN_TIME=$READS_TIME_SECONDS
        else
            SYSBENCH_RUN_TIME=$WRITES_TIME_SECONDS
        fi

        run_sysbench
    done
done
