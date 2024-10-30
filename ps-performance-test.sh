#!/bin/bash
# set -x

# script parameters
export BENCH_NAME=$1
export BUILD_PATH=$2
export CONFIG_FILES="$3"

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
export WORKLOAD_SCRIPT=${WORKLOAD_SCRIPT:-$SCRIPT_DIR/workloads/read_write.txt}

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

source ${SCRIPT_DIR}/data_funcs.inc
source ${SCRIPT_DIR}/main_funcs.inc
source ${SCRIPT_DIR}/system_funcs.inc


#**********************************************************************************************
# main
#**********************************************************************************************
if [[ ${BENCHMARK_LOGGING} == "Y" ]]; then
  command -v cpupower >/dev/null 2>&1 || { echo >&2 "cpupower is not installed. Aborting."; exit 1; }
  [[ $(cpupower 2>&1) == *"WARNING: cpupower not found for kernel"* ]] && { echo >&2 "Error: cpupower is installed but not available for the current kernel."; exit 1; }
  command -v dstat >/dev/null 2>&1 || { echo >&2 "dstat is not installed. Aborting."; exit 1; }
  command -v iostat >/dev/null 2>&1 || { echo >&2 "iostat is not installed. Aborting."; exit 1; }
  dstat -t -v --nocolor --output dstat.csv 1 1 >/dev/null 2>&1 || DSTAT_OUTPUT_NOT_SUPPORTED=1
  rm dstat.csv
fi

export START_TIME=$(date +%s)
export LOGS=$BENCH_DIR
export LOGS_AVG=${LOGS}/${BENCH_NAME}-avg
export LOGS_STDDEV=${LOGS}/${BENCH_NAME}-stddev
export LOGS_DIFF=${LOGS}/${BENCH_NAME}-diff
export LOGS_QPS=${LOGS}/${BENCH_NAME}-qps

# check parameters
if [ $# -lt 3 ]; then usage "ERROR: Too little parameters passed"; fi
if [ ! -f $WORKLOAD_SCRIPT ]; then usage "ERROR: Workloads config file $WORKLOAD_SCRIPT not found."; fi
if [ ! -x $BUILD_PATH/bin/mysqld ]; then usage "ERROR: Executable $BUILD_PATH/bin/mysqld not found."; fi

rm -rf ${LOGS}
mkdir -p ${LOGS} ${LOGS_AVG} ${LOGS_STDDEV} ${LOGS_DIFF} ${LOGS_QPS} ${CACHE_DIR}
cd $WORKSPACE

process_workload_config_file "$WORKLOAD_SCRIPT"
get_build_info

export INNODB_CACHE=${INNODB_CACHE:-32G}
export NUM_TABLES=${NUM_TABLES:-16}
export DATASIZE=${DATASIZE:-10M}
export BENCH_ID=$(uname -n)_${MYSQL_NAME}${MYSQL_VERSION}_${WRITES_TIME_SECONDS}sec_${NUM_TABLES}x${DATASIZE}-${INNODB_CACHE}

on_start

for file in $CONFIG_FILES; do
  if [ ! -f $file ]; then usage "ERROR: Config file $file not found."; fi
  CONFIG_BASE=$(basename ${file%.*})
  LOGS_CONFIG=${LOGS}/${BENCH_NAME}-${CONFIG_BASE}
  mkdir -p ${LOGS_CONFIG}
  CONFIG_FILE=${LOGS_CONFIG}/$(basename $file)
  cp $file $CONFIG_FILE
  echo "Using $CONFIG_FILE as mysqld config file"


  MYSQL_SOCKET=${LOGS}/ps_socket.sock
  timeout --signal=9 30s ${BUILD_PATH}/bin/mysqladmin -uroot --socket=$MYSQL_SOCKET shutdown > /dev/null 2>&1
  kill -9 $(pgrep -f ${DATA_DIR}) 2>/dev/null

  run_sysbench
done
