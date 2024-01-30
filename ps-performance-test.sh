#!/bin/bash
# set -x

#**********************************************************************************************
# PS performance benchmark scripts
# Sysbench suite will run performance tests
#**********************************************************************************************

# script parameters
export BENCH_NAME=$1
export BUILD_DIR=$2
export CONFIG_FILES="$3"

# generic variables
export RPORT=$(( RANDOM%21 + 10 ))
export RBASE="$(( RPORT*1000 ))"
export WORKSPACE=${WORKSPACE:-${PWD}}
export BENCHMARK_LOGGING=${BENCHMARK_LOGGING:-Y}
SCRIPT_DIR=$(cd $(dirname $0) && pwd)
WORKLOAD_SCRIPT=${WORKLOAD_SCRIPT:-$SCRIPT_DIR/workloads/read_write.txt}

# sysbench variables
export MYSQL_DATABASE=test
export SUSER=root
export RAND_TYPE=${RAND_TYPE:-uniform}
export RAND_SEED=${RAND_SEED:-1111}
export THREADS_LIST=${THREADS_LIST:="0001 0004 0016 0064 0128 0256 0512 1024"}
SYSBENCH_DIR=${SYSBENCH_DIR:-/usr/local/share}
export EVENTS_MULT=${EVENTS_MULT:-1}

# time variables
export PS_START_TIMEOUT=100
WARMUP_TIME_AT_START=${WARMUP_TIME_AT_START:-600}
export WARMUP_TIME_SECONDS=${WARMUP_TIME_SECONDS:-30}
export RUN_TIME_SECONDS=${RUN_TIME_SECONDS:-600}
export REPORT_INTERVAL=10
export IOSTAT_INTERVAL=10
export DSTAT_INTERVAL=10

#MYEXTRA=${MYEXTRA:=--disable-log-bin}
#TASKSET_MYSQLD=${TASKSET_MYSQLD:=taskset -c 0}
#TASKSET_SYSBENCH=${TASKSET_SYSBENCH:=taskset -c 1}


function usage(){
  echo $1
  echo "Usage: $0 <BENCH_NAME> <BUILD_DIR> <MYSQL_CONFIG_FILE>"
  echo "where:"
  echo "<BENCH_NAME> - name of benchmark (a directory with this name will be created in \$WORKSPACE)"
  echo "<BUILD_DIR> - relative path to MySQL or Percona Server binaries (inside \$WORKSPACE)"
  echo "<MYSQL_CONFIG_FILE> - full path to Percona Server's configuration file"
  echo "Usage example:"
  echo "$0 100 Percona-Server-8.0.34-26-Linux.x86_64.glibc2.35 cnf/percona-innodb.cnf"
  echo "This would lead to $WORKSPACE/100 being created, in which testing takes place and"
  echo "$WORKSPACE/Percona-Server-8.0.34-26-Linux.x86_64.glibc2.35 would be used to test."
  exit 1
}

function disable_address_randomization(){
    PREVIOUS_ASLR=`cat /proc/sys/kernel/randomize_va_space`
    sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"
    echo "Changing /proc/sys/kernel/randomize_va_space from $PREVIOUS_ASLR to `cat /proc/sys/kernel/randomize_va_space`"
}

function restore_address_randomization(){
    local CURRENT_ASLR=`cat /proc/sys/kernel/randomize_va_space`
    sudo sh -c "echo $PREVIOUS_ASLR > /proc/sys/kernel/randomize_va_space"
    echo "Resoring /proc/sys/kernel/randomize_va_space from $CURRENT_ASLR to `cat /proc/sys/kernel/randomize_va_space`"
}

function disable_turbo_boost(){
  SCALING_DRIVER=`cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_driver`
  echo "Using $SCALING_DRIVER scaling driver"

  if [[ ${SCALING_DRIVER} == "intel_pstate" || ${SCALING_DRIVER} == "intel_cpufreq" ]]; then
    PREVIOUS_TURBO=`cat /sys/devices/system/cpu/intel_pstate/no_turbo`
    sudo sh -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"
    echo "Changing /sys/devices/system/cpu/intel_pstate/no_turbo from $PREVIOUS_TURBO to `cat /sys/devices/system/cpu/intel_pstate/no_turbo`"
  else
    PREVIOUS_TURBO=`cat /sys/devices/system/cpu/cpufreq/boost`
    sudo sh -c "echo 0 > /sys/devices/system/cpu/cpufreq/boost"
    echo "Changing /sys/devices/system/cpu/cpufreq/boost from $PREVIOUS_TURBO to `cat /sys/devices/system/cpu/cpufreq/boost`"
  fi
}

function restore_turbo_boost(){
  echo "Restore turbo boost with $SCALING_DRIVER scaling driver"

  if [[ ${SCALING_DRIVER} == "intel_pstate" || ${SCALING_DRIVER} == "intel_cpufreq" ]]; then
    CURRENT_TURBO=`cat /sys/devices/system/cpu/intel_pstate/no_turbo`
    sudo sh -c "echo $PREVIOUS_TURBO > /sys/devices/system/cpu/intel_pstate/no_turbo"
    echo "Resoring /sys/devices/system/cpu/intel_pstate/no_turbo from $CURRENT_TURBO to $PREVIOUS_TURBO"
  else
    CURRENT_TURBO=`cat /sys/devices/system/cpu/cpufreq/boost`
    sudo sh -c "echo $PREVIOUS_TURBO > /sys/devices/system/cpu/cpufreq/boost"
    echo "Resoring /sys/devices/system/cpu/cpufreq/boost from $CURRENT_TURBO to $PREVIOUS_TURBO"
  fi
}

function change_scaling_governor(){
  PREVIOUS_GOVERNOR=`cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`
  sudo cpupower frequency-set -g $1
  echo "Changing scaling governor from $PREVIOUS_GOVERNOR to `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`"
  sudo cpupower frequency-info
}

function restore_scaling_governor(){
  local CURRENT_GOVERNOR=`cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`
  sudo cpupower frequency-set -g $PREVIOUS_GOVERNOR
  echo "Restoring scaling governor from $CURRENT_GOVERNOR to `cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor`"
  sudo cpupower frequency-info
}

function disable_idle_states(){
  sudo cpupower idle-set --disable-by-latency 0
  sudo cpupower idle-info
}

function enable_idle_states(){
  sudo cpupower idle-set --enable-all
  sudo cpupower idle-info
}

function save_system_info(){
  local VERSION_INFO=`$BUILD_PATH/bin/mysqld --version | cut -d' ' -f2-`
  local UPTIME_HOUR=`uptime -p`
  local SYSTEM_LOAD=`uptime | sed 's|  | |g' | sed -e 's|.*user*.,|System|'`
  local MEM=`free -g | grep "Mem:" | awk '{print "Total:"$2"GB  Used:"$3"GB  Free:"$4"GB" }'`
  if [ ! -f $LOGS/hw.info ];then
    if [ -f /etc/redhat-release ]; then
      RELEASE=`cat /etc/redhat-release`
    else
      RELEASE=`cat /etc/issue`
    fi
    local KERNEL=`uname -r`
    echo "HW info | $RELEASE $KERNEL"  > $LOGS/hw.info
  fi
  echo "Build #$BENCH_NAME | `date +'%d-%m-%Y | %H:%M'` | $VERSION_INFO | $UPTIME_HOUR | $SYSTEM_LOAD | Memory: $MEM " >> $LOGS/build_info.log
}

function archive_logs(){
  local BENCH_ID=${MYSQL_VERSION}-${NUM_TABLES}x${DATASIZE}-${INNODB_CACHE}
  local DATE=`date +"%Y%m%d%H%M%S"`
  local tarFileName="sysbench_${BENCH_ID}_perf_result_set_${BENCH_NAME}_${DATE}.tar.gz"
  tar czvf ${tarFileName} ${BENCH_NAME}/logs --transform "s+^${BENCH_NAME}/logs++"
}

# depends on $LOGS, $LOGS_CPU, $BENCH_NAME, $DATA_DIR, $MYSQL_VERSION, $NUM_TABLES, $DATASIZE, $INNODB_CACHE
function on_start(){
  disable_address_randomization >> ${LOGS_CPU}
  disable_turbo_boost > ${LOGS_CPU}
  change_scaling_governor powersave >> ${LOGS_CPU}
  disable_idle_states >> ${LOGS_CPU}

  trap on_exit EXIT KILL
}

function on_exit(){
  pkill -f dstat
  pkill -f iostat
  killall -9 mysqld

  echo "Restoring address randomization"
  restore_address_randomization >> ${LOGS_CPU}
  echo "Restoring turbo boost"
  restore_turbo_boost >> ${LOGS_CPU}
  echo "Restoring scaling governor"
  restore_scaling_governor >> ${LOGS_CPU}
  echo "Enabling idle states"
  enable_idle_states >> ${LOGS_CPU}

  save_system_info

  cat ${LOGS}/sysbench_*_perf_result_set.txt > ${LOGS}/sysbench_${BENCH_NAME}_full_result_set.txt
  cat ${LOGS}/sysbench_*_perf_result_set.txt

  archive_logs

  rm -rf ${DATA_DIR}
}

# Function to process a configuration file and return WORKLOAD_NAMES[] and WORKLOAD_PARAMS[] arrays
function process_workload_config_file() {
  local filename="$1"
  WORKLOAD_NAMES=()
  WORKLOAD_PARAMS=()

  while IFS= read -r line; do
    # Ignore lines starting with '#' (comments) and empty lines
    if [[ "$line" =~ ^\# ]] || [[ "$line" == "" ]]; then
      continue
    fi

    # Concatenate lines ending with "\"
    out_line="${line%\\}"
    out_line=${out_line% } # Trim suffix (space)
    while [[ $line =~ \\$ ]]; do
      read -r line
      out_line+=" ${line%\\}"
      out_line=${out_line% } # Trim suffix (space)
    done
    line=$out_line

    # Extract variable name and value
    variable_name=$(echo "$line" | cut -d= -f1)
    variable_value=$(echo "$line" | cut -d= -f2-)

    # Trim leading and trailing whitespaces from variable value
    variable_name=$(echo "$variable_name" | sed -e 's/^[[:space:]]*//' -e 's/[[:space:]]*$//')
    variable_value=$(echo "$variable_value" | sed -e 's/^[[:space:]]*[" ]//' -e 's/[" ][[:space:]]*$//')

    # Add the variable name and value to their respective arrays
    WORKLOAD_NAMES+=("$variable_name")
    WORKLOAD_PARAMS+=("$variable_value")
  done < "$filename"
}

function drop_caches(){
  echo "Dropping caches"
  sync
  sudo sh -c 'sysctl -q -w vm.drop_caches=3'
  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
  ulimit -n 1000000
}

function report_thread(){
  local CHECK_PID=`ps -ef | grep ps_socket | grep -v grep | awk '{ print $2}'`
  rm -f ${LOG_NAME_INXI} ${LOG_NAME_MEMORY}
  while [ true ]; do
    DATE=`date +"%Y%m%d%H%M%S"`
    CURRENT_INFO=`ps -o rss,vsz,pcpu ${CHECK_PID} | tail -n 1`
    echo "${DATE} ${CURRENT_INFO}" >> ${LOG_NAME_MEMORY}
    inxi -C -c 0 >> ${LOG_NAME_INXI}
    sleep ${REPORT_INTERVAL}
  done
}

# start_mysqld $MORE_PARAMS
function start_mysqld() {
  local EXTRA_PARAMS="$MYEXTRA --innodb-buffer-pool-size=$INNODB_CACHE $1"
  RBASE="$(( RBASE + 100 ))"
  local MYSQLD_OPTIONS="--defaults-file=${CONFIG_FILE} --basedir=${BUILD_PATH} $EXTRA_PARAMS --log-error=${LOGS_CONFIG}/master.err --socket=$MYSQL_SOCKET --port=$RBASE"
  echo "Starting Percona Server with options $MYSQLD_OPTIONS" | tee -a ${LOGS_CONFIG}/master.err
  ${TASKSET_MYSQLD} ${BUILD_PATH}/bin/mysqld $MYSQLD_OPTIONS >> ${LOGS_CONFIG}/master.err 2>&1 &

  for X in $(seq 0 ${PS_START_TIMEOUT}); do
    sleep 1
    if ${BUILD_PATH}/bin/mysqladmin -uroot -S$MYSQL_SOCKET ping > /dev/null 2>&1; then
      echo "Started Percona Server. Socket=$MYSQL_SOCKET Port=$RBASE"
      break
    fi
  done
  ${BUILD_PATH}/bin/mysqladmin -uroot -S$MYSQL_SOCKET ping > /dev/null 2>&1 || { echo “Couldn\'t connect $MYSQL_SOCKET” && exit 0; }
}

# shutdown_mysqld $TIMEOUT
function shutdown_mysqld() {
  local TIMEOUT=${1:-3600}
  echo "Shutting mysqld down"
  timeout --signal=9 ${TIMEOUT}s ${BUILD_PATH}/bin/mysqladmin -uroot --socket=$MYSQL_SOCKET shutdown > /dev/null 2>&1
}

function create_datadir() {
  local NUM_ROWS=$(numfmt --from=si $DATASIZE)
  SYSBENCH_OPTIONS="$SYSBENCH_EXTRA --table-size=$NUM_ROWS --tables=$NUM_TABLES --mysql-db=$MYSQL_DATABASE --mysql-user=$SUSER --report-interval=10 --db-driver=mysql --db-ps-mode=disable --percentile=99 --rand-seed=$RAND_SEED --rand-type=$RAND_TYPE"
  local WS_DATADIR="${WORKSPACE}/80_sysbench_data_template"
  local TEMPLATE_DIR=${WS_DATADIR}/datadir_${NUM_TABLES}x${DATASIZE}
  if [ ! -d ${TEMPLATE_DIR} ]; then
    mkdir ${WS_DATADIR} > /dev/null 2>&1
    ${TASKSET_MYSQLD} ${BUILD_PATH}/bin/mysqld --no-defaults --initialize-insecure --basedir=${BUILD_PATH} --datadir=${TEMPLATE_DIR} > $LOGS/startup.err 2>&1

    start_mysqld "--datadir=${TEMPLATE_DIR} --disable-log-bin"
    echo "Creating template data directory in ${TEMPLATE_DIR}"
    ${BUILD_PATH}/bin/mysql -uroot -S$MYSQL_SOCKET -e "CREATE DATABASE IF NOT EXISTS $MYSQL_DATABASE" 2>&1
    time ${TASKSET_SYSBENCH} sysbench $SYSBENCH_DIR/sysbench/oltp_insert.lua --threads=$NUM_TABLES $SYSBENCH_OPTIONS --mysql-socket=$MYSQL_SOCKET prepare 2>&1 | tee $LOGS/sysbench_prepare.log
    echo "Data directory in ${TEMPLATE_DIR} created"
    shutdown_mysqld
  fi
  echo "Copying data directory from ${TEMPLATE_DIR} to ${DATA_DIR}"
  rm -rf ${DATA_DIR}
  cp -r ${TEMPLATE_DIR} ${DATA_DIR}
}

function run_sysbench() {
  if [[ ${WARMUP_TIME_AT_START} > 0 ]]; then
    # *** REMEMBER *** warmmup is READ ONLY!
    # warmup the cache, 64 threads for $WARMUP_TIME_AT_START seconds,
    num_threads=64
    echo "Warming up for $WARMUP_TIME_AT_START seconds"
    start_mysqld "--datadir=${DATA_DIR}"
    ${TASKSET_SYSBENCH} sysbench $SYSBENCH_DIR/sysbench/oltp_read_only.lua --threads=$num_threads --time=$WARMUP_TIME_AT_START $SYSBENCH_OPTIONS --mysql-socket=$MYSQL_SOCKET run > ${LOGS_CONFIG}/sysbench_warmup.log 2>&1
    shutdown_mysqld
    sleep $[WARMUP_TIME_AT_START/10]
  fi
  echo "Storing Sysbench results in ${WORKSPACE}"

  for ((num=0; num<${#WORKLOAD_NAMES[@]}; num++)); do
    start_mysqld "--datadir=${DATA_DIR}"
    local WORKLOAD_NAME=${WORKLOAD_NAMES[num]}
    local WORKLOAD_PARAMETERS=$(eval echo ${WORKLOAD_PARAMS[num]})
    local BENCH_ID=${MYSQL_VERSION}-${WORKLOAD_NAME%.*}-${NUM_TABLES}x${DATASIZE}-${INNODB_CACHE}
    echo "Using ${WORKLOAD_NAME}=${WORKLOAD_PARAMETERS}"

    for num_threads in ${THREADS_LIST}; do
      echo "Testing $WORKLOAD_NAME with $num_threads threads"
      LOG_NAME_RESULTS=${LOGS_CONFIG}/results-QPS-${BENCH_ID}.txt
      LOG_NAME=${LOGS_CONFIG}/${BENCH_ID}-$num_threads.txt
      LOG_NAME_MEMORY=${LOG_NAME}.memory
      LOG_NAME_IOSTAT=${LOG_NAME}.iostat
      LOG_NAME_DSTAT=${LOG_NAME}.dstat
      LOG_NAME_DSTAT_CSV=${LOG_NAME}.dstat.csv
      LOG_NAME_INXI=${LOG_NAME}.inxi

      if [[ ${BENCHMARK_LOGGING} == "Y" ]]; then
          # verbose logging
          echo "*** verbose benchmark logging enabled ***"
          report_thread &
          REPORT_THREAD_PID=$!
          (iostat -dxm $IOSTAT_INTERVAL 1000000 | grep -v loop > $LOG_NAME_IOSTAT) &
          dstat -t -v --nocolor --output $LOG_NAME_DSTAT_CSV $DSTAT_INTERVAL 1000000 > $LOG_NAME_DSTAT &
      fi
      local ALL_SYSBENCH_OPTIONS="$SYSBENCH_DIR/sysbench/$WORKLOAD_PARAMETERS --threads=$num_threads --time=$RUN_TIME_SECONDS --warmup-time=$WARMUP_TIME_SECONDS $SYSBENCH_OPTIONS --mysql-socket=$MYSQL_SOCKET run"
      echo "Starting sysbench with options $ALL_SYSBENCH_OPTIONS" | tee $LOG_NAME
      ${TASKSET_SYSBENCH} sysbench $ALL_SYSBENCH_OPTIONS | tee -a $LOG_NAME
      sleep 6
      pkill -f dstat
      pkill -f iostat
      kill -9 ${REPORT_THREAD_PID}
      result_set+=(`grep  "queries:" $LOG_NAME | cut -d'(' -f2 | awk '{print $1 ","}'`)
    done

    for ((i=0; i<${#result_set[@]}; i++)); do if [ -z ${result_set[i]} ]; then result_set[i]='0,'; fi; done
    echo "[ '${BENCH_NAME}_${CONFIG_BASE}_${BENCH_ID}', ${result_set[*]} ]," >> ${LOG_NAME_RESULTS}
    cat ${LOG_NAME_RESULTS} >> ${LOGS}/sysbench_${BENCH_ID}_${BENCH_NAME}_perf_result_set.txt
    unset result_set
    shutdown_mysqld
    ps -ef | grep 'ps_socket' | grep ${BENCH_NAME} | grep -v grep | awk '{print $2}' | xargs kill -9 >/dev/null 2>&1 || true
  done
}


#**********************************************************************************************
# main
#**********************************************************************************************
command -v cpupower >/dev/null 2>&1 || { echo >&2 "cpupower is not installed. Aborting."; exit 1; }

export BENCH_DIR=$WORKSPACE/$BENCH_NAME
export BUILD_PATH=$BENCH_DIR/$BUILD_DIR
export DATA_DIR=$BENCH_DIR/datadir
export LOGS=$BENCH_DIR/logs
LOGS_CPU=$LOGS/cpu-states.txt

# check parameters
echo "Using WORKSPACE=$WORKSPACE WORKLOAD_SCRIPT=$WORKLOAD_SCRIPT"
if [ $# -lt 3 ]; then usage "ERROR: Too little parameters passed"; fi
if [[ ! -d $WORKSPACE/$BUILD_DIR ]]; then usage "ERROR: Couldn't find binaries in $WORKSPACE/$BUILD_DIR"; fi
if [ ! -f $WORKLOAD_SCRIPT ]; then usage "ERROR: Workloads config file $WORKLOAD_SCRIPT not found."; fi

process_workload_config_file "$WORKLOAD_SCRIPT"
echo "====="
for ((i=0; i<${#WORKLOAD_NAMES[@]}; i++)); do
  WORKLOAD_PARAMETERS=$(eval echo ${WORKLOAD_PARAMS[i]})
  echo "${WORKLOAD_NAMES[i]}=${WORKLOAD_PARAMETERS}"
done
echo "====="

rm -rf ${LOGS}
mkdir -p ${LOGS}
cd $WORKSPACE
echo "Copying server binaries from $WORKSPACE/$BUILD_DIR to $BUILD_PATH"
cp -r $WORKSPACE/$BUILD_DIR $BENCH_DIR || usage "ERROR: Failed to copy binaries from $WORKSPACE/$BUILD_DIR to $BUILD_PATH"

if [ ! -x $BUILD_PATH/bin/mysqld ]; then usage "ERROR: Executable $BUILD_PATH/bin/mysqld not found."; fi
MYSQL_VERSION=`$BUILD_PATH/bin/mysqld --version | awk '{ print $3}'`
MYSQL_NAME=`$BUILD_PATH/bin/mysqld --version | awk '{for (i=2; i<NF; i++) printf $i " "; print $NF}'`
if [[ $MYSQL_NAME == *"Percona Server"* ]]; then MYSQL_NAME=PS; else MYSQL_NAME=MS; fi
export MYSQL_VERSION="$MYSQL_NAME${MYSQL_VERSION//./}"

export INNODB_CACHE=${INNODB_CACHE:-32G}
export NUM_TABLES=${NUM_TABLES:-16}
export DATASIZE=${DATASIZE:-10M}

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
  ps -ef | grep 'ps_socket' | grep ${BENCH_NAME} | grep -v grep | awk '{print $2}' | xargs kill -9 >/dev/null 2>&1 || true

  drop_caches
  create_datadir
  run_sysbench
done

# "exit" calls on_exit()
exit 0
