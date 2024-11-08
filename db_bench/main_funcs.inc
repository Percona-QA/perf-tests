#!/bin/echo This script should be sourced in a shell, not executed directly

#**********************************************************************************************
# Percona Database Benchmark scripts
# main_funcs.inc - mysqld and sysbench related functions
#**********************************************************************************************

function usage(){
  echo "$1"
  echo -e "\nYou have to set the following env variables:"
  echo "<WORKSPACE> - path to store results (default: \$PWD)"
  echo "<BENCH_NAME> - name of benchmark (a directory with this name will be created in \$WORKSPACE)"
  echo "<BUILD_PATH> - path to MySQL or Percona Server binaries"
  echo "<CONFIG_FILES> - full path to Percona Server's configuration file"
  echo -e "\nUsage example:"
  echo "export WORKSPACE=/data/perf-results"
  echo "export CONFIG_FILES=/data/perf-tests/cnf/stable-noACID.cnf"
  echo "export BUILD_PATH=/data/release-9.0.1-1-rel-gcc-13"
  echo "export BENCH_NAME=release-9.0.1-1@6feb03ff8ed"
  echo "bash $0"
  exit 1
}

# depends on $DATA_DIR, $LOGS, $SCALING_GOVERNOR, $BENCH_ID, $BENCH_NAME, $START_TIME, $THREADS_LIST, $CONFIG_FILES, $WORKLOAD_NAMES, $SLACK_WEBHOOK_URL, $WORKSPACE, $BACKUP_DIR, $RESULTS_EMAIL
function on_exit(){
  pkill -f dstat
  pkill -f iostat
  killall -9 mysqld

  echo "Remove DATA_DIR=$DATA_DIR"
  rm -rf ${DATA_DIR}

  local LOG_SYS_INFO=$LOGS/sys_info_end.txt
  print_system_info >> ${LOG_SYS_INFO}

  if [[ ${SCALING_GOVERNOR} != "" ]]; then
    echo "Restoring address randomization"
    restore_address_randomization >> ${LOG_SYS_INFO}
    echo "Restoring turbo boost"
    restore_turbo_boost >> ${LOG_SYS_INFO}
    echo "Restoring scaling governor"
    restore_scaling_governor >> ${LOG_SYS_INFO}
    echo "Enabling idle states"
    if [[ ${DISABLE_IDLE_STATES} == "yes" ]]; then
      enable_idle_states >> ${LOG_SYS_INFO}
    fi
  fi

  local LOG_BASE_FULL_RESULTS=${LOGS}/${BENCH_ID}_${BENCH_NAME}_qps
  local LOG_BASE_DIFF=${LOGS}/${BENCH_ID}_${BENCH_NAME}_diff
  local LOG_BASE_STDDEV=${LOGS}/${BENCH_ID}_${BENCH_NAME}_stddev
  local LOG_BASE_AVG=${LOGS}/${BENCH_ID}_${BENCH_NAME}_avg
  local END_TIME=$(date +%s)
  local DURATION=$((END_TIME - START_TIME))
  local TIME_HMS=$(printf "%02d:%02d:%02d" $((DURATION / 3600)) $(((DURATION % 3600) / 60)) $((DURATION % 60)))

  HEADER="WORKLOAD"
  for num_threads in ${THREADS_LIST}; do HEADER+=", ${num_threads} THDS"; done

  echo "Create .csv files"
  echo "${HEADER}" > ${LOG_BASE_FULL_RESULTS}.csv
  cat ${LOGS_QPS}/*${BENCH_NAME}_qps.csv >> ${LOG_BASE_FULL_RESULTS}.csv
  echo "${HEADER}" > ${LOG_BASE_DIFF}.csv
  cat ${LOGS_DIFF}/*${BENCH_NAME}_diff.csv >> ${LOG_BASE_DIFF}.csv
  echo "${HEADER}" > ${LOG_BASE_STDDEV}.csv
  cat ${LOGS_STDDEV}/*${BENCH_NAME}_stddev.csv >> ${LOG_BASE_STDDEV}.csv
  echo "${HEADER}" > ${LOG_BASE_AVG}.csv
  cat ${LOGS_AVG}/*${BENCH_NAME}_avg.csv >> ${LOG_BASE_AVG}.csv

  echo "Create .html files"
  echo -e "Script executed in $TIME_HMS ($DURATION seconds)<BR>\n<BR>\n" > ${LOG_BASE_FULL_RESULTS}.html
  print_parameters "<BR>" >> ${LOG_BASE_FULL_RESULTS}.html
  echo -e "<BR>QPS results:<BR>" >> ${LOG_BASE_FULL_RESULTS}.html
  csv_to_html_table ${LOG_BASE_FULL_RESULTS}.csv >> ${LOG_BASE_FULL_RESULTS}.html
  echo "Average QPS:<BR>" > ${LOG_BASE_AVG}.html
  csv_to_html_table ${LOG_BASE_AVG}.csv >> ${LOG_BASE_AVG}.html
  echo "Difference in percentages to the average QPS:<BR>" > ${LOG_BASE_DIFF}.html
  csv_to_html_table ${LOG_BASE_DIFF}.csv "color" >> ${LOG_BASE_DIFF}.html
  echo "Standard deviation as a percentage of the average QPS:<BR>" > ${LOG_BASE_STDDEV}.html
  csv_to_html_table ${LOG_BASE_STDDEV}.csv "color" >> ${LOG_BASE_STDDEV}.html

  local tarFileName="${BENCH_ID}_${BENCH_NAME}.tar.gz"
  local NICE_DATE=$(date +"%Y-%m-%d %H:%M")
  local SUBJECT="Done $(basename "${CONFIG_FILES}" .cnf) $(basename "${WORKLOAD_NAMES^^}" .TXT) ${BENCH_ID}_${BENCH_NAME} at ${NICE_DATE}"

  if [[ ${SLACK_WEBHOOK_URL} != "" ]]; then
    echo "- Sending slack message"
    SLACK_MESSAGE="${SUBJECT}\nScript executed in $TIME_HMS ($DURATION seconds)\nWORKLOAD_NAMES=${WORKLOAD_NAMES}\nBUILD_PATH=${BUILD_PATH}\nDATA_DIR=${DATA_DIR}\n" ${SCRIPT_DIR}/db_bench/publish_to_slack.py ${LOG_BASE_FULL_RESULTS}.csv ${LOG_BASE_DIFF}.csv ${LOG_BASE_STDDEV}.csv
  fi

  echo "Script executed in $TIME_HMS ($DURATION seconds)" | tee -a ${LOG_BASE_FULL_RESULTS}.csv
  echo "-----" && cat ${LOG_BASE_FULL_RESULTS}.csv && echo "-----" && cat ${LOG_BASE_AVG}.csv && echo "-----" && cat ${LOG_BASE_DIFF}.csv && echo "-----" && cat ${LOG_BASE_STDDEV}.csv && echo "-----"

  cd $WORKSPACE
  tar czvf ${tarFileName} ${BENCH_NAME} --force-local --transform "s+^${BENCH_NAME}++"

  if [[ ${BACKUP_DIR} != "" ]]; then
    echo "- Copying ${tarFileName} to ${BACKUP_DIR}"
    cp ${tarFileName} ${BACKUP_DIR}/
    cp ${LOGS}/${BENCH_ID}_${BENCH_NAME}*.csv ${BACKUP_DIR}/
    create_html_page ${LOG_BASE_FULL_RESULTS}.html ${LOG_BASE_AVG}.html ${LOG_BASE_DIFF}.html ${LOG_BASE_STDDEV}.html > ${BACKUP_DIR}/${BENCH_ID}_${BENCH_NAME}.html
  fi

  if [[ ${RESULTS_EMAIL} != "" ]]; then
    echo "- Sending e-mail to ${RESULTS_EMAIL} with ${tarFileName}"
    create_html_page ${LOG_BASE_FULL_RESULTS}.html ${LOG_BASE_AVG}.html ${LOG_BASE_DIFF}.html ${LOG_BASE_STDDEV}.html | mutt -s "${SUBJECT}" -e "set content_type=text/html" -a ${tarFileName} -- ${RESULTS_EMAIL}
  fi
}

function report_thread(){
  local CHECK_PID=`pgrep -f ${DATA_DIR}`
  rm -f ${LOG_NAME_CPUINFO} ${LOG_NAME_MEMORY} ${LOG_NAME_SMART} ${LOG_NAME_PS} ${LOG_NAME_ZONEINFO} ${LOG_NAME_VMSTAT}
  while [ true ]; do
    DATE=`date +"%Y%m%d%H%M%S"`
    CURRENT_INFO=`ps -o rss,vsz,pcpu ${CHECK_PID} | tail -n 1`
    echo "${DATE} ${CURRENT_INFO}" >> ${LOG_NAME_MEMORY}
    DATE=`date +"%Y-%m-%d %H:%M:%S"`
    echo "${DATE}" >> ${LOG_NAME_CPUINFO}
    cat /proc/cpuinfo | grep "cpu MHz" >> ${LOG_NAME_CPUINFO}
    echo "${DATE}" >> ${LOG_NAME_ZONEINFO}
    grep -A64 "zone   Normal" /proc/zoneinfo >> ${LOG_NAME_ZONEINFO}
    echo "${DATE}" >> ${LOG_NAME_VMSTAT}
    cat /proc/vmstat >> ${LOG_NAME_VMSTAT}
    echo "${DATE}" >> ${LOG_NAME_PS}
    ps aux | sort -rn -k +3 | head >> ${LOG_NAME_PS}
    echo "${DATE} $SMART_DEVICE" >> ${LOG_NAME_SMART}
    sudo smartctl -A $SMART_DEVICE >> ${LOG_NAME_SMART} 2>&1
    sleep ${REPORT_INTERVAL}
  done
}

# start_mysqld $MORE_PARAMS
function start_mysqld() {
  local EXTRA_PARAMS="--user=root --innodb-buffer-pool-size=$INNODB_CACHE $PERF_EXTRA $MYEXTRA $1"
  RBASE="$(( RBASE + 100 ))"
  local MYSQLD_OPTIONS="--defaults-file=${CONFIG_FILE} --basedir=${BUILD_PATH} $EXTRA_PARAMS --log-error=$LOG_NAME_MYSQLD --socket=$MYSQL_SOCKET --port=$RBASE"
  echo "Starting Percona Server with options $MYSQLD_OPTIONS" | tee -a $LOG_NAME_MYSQLD
  ${TASKSET_MYSQLD} ${BUILD_PATH}/bin/mysqld $MYSQLD_OPTIONS >> $LOG_NAME_MYSQLD 2>&1 &

  echo "- Waiting for start of mysqld"
  for X in $(seq 0 ${PS_START_TIMEOUT}); do
    sleep 1
    echo -n "."
    if ${BUILD_PATH}/bin/mysqladmin -uroot -S$MYSQL_SOCKET ping > /dev/null 2>&1; then
      echo "Percona Server started in $(( X+1 )) seconds (socket=$MYSQL_SOCKET port=$RBASE)"
      break
    fi
  done
  ${BUILD_PATH}/bin/mysqladmin -uroot -S$MYSQL_SOCKET ping > /dev/null 2>&1 || { cat $LOG_NAME_MYSQLD; echo "Couldn't connect $MYSQL_SOCKET" && exit 0; }
}

# print_database_size
function print_database_size() {
  local DATA_SIZE=`du -s $DATA_DIR | awk '{sum+=$1;} END {printf "%d\n", sum/1024;}'`
  local FILE_COUNT=`ls -aR $DATA_DIR | wc -l`
  echo "- Size of database is $DATA_SIZE MB in $FILE_COUNT files"
}

# shutdown_mysqld
function shutdown_mysqld() {
  if [[ "$MYSQL_VERSION" < "81" ]]; then
    ${BUILD_PATH}/bin/mysql -uroot -S$MYSQL_SOCKET -e "SHOW BINARY LOGS; RESET MASTER" 2>&1
  else
    ${BUILD_PATH}/bin/mysql -uroot -S$MYSQL_SOCKET -e "SHOW BINARY LOGS; RESET BINARY LOGS AND GTIDS" 2>&1
  fi
  ${BUILD_PATH}/bin/mysql -uroot -S$MYSQL_SOCKET -e "USE $MYSQL_DATABASE; SELECT COUNT(*) FROM sbtest1" 2>&1

  echo "Shutting mysqld down"
  (time ${BUILD_PATH}/bin/mysqladmin -uroot --socket=$MYSQL_SOCKET shutdown) 2>&1
  print_database_size
}

function init_perf_tests() {
  local NUM_ROWS=$(numfmt --from=si $DATASIZE)
  SYSBENCH_OPTIONS="--table-size=$NUM_ROWS --tables=$NUM_TABLES --mysql-db=$MYSQL_DATABASE --mysql-user=$SUSER --report-interval=$SYSBENCH_REPORT_INTERVAL --db-driver=mysql --mysql-ssl=DISABLED --db-ps-mode=disable --percentile=99 --rand-type=$RAND_TYPE $SYSBENCH_EXTRA"
}

function snapshot_datadir() {
  local SNAPSHOT_DIR=${TEMPLATE_PATH}/snapshot_${MYSQL_VERSION%-*}_${NUM_TABLES}x${DATASIZE}
  echo "Copying data directory from ${DATA_DIR} to ${SNAPSHOT_DIR}"
  rm -rf ${SNAPSHOT_DIR}
  (time cp -r ${DATA_DIR} ${SNAPSHOT_DIR}) 2>&1
  print_database_size
}

function copy_datadir() {
  local FROM_DIR=$1
  local TO_DIR=$2
  if [ $# -lt 2 ]; then echo "Usage: copy_datadir <FROM_DIR> <TO_DIR>"; return 1; fi

  echo "Copying data directory from ${FROM_DIR} to ${TO_DIR}"
  rm -rf ${TO_DIR}
  (time cp -r ${FROM_DIR} ${TO_DIR}) 2>&1
  print_database_size
}

function prepare_datadir() {
  local SNAPSHOT_DIR=${TEMPLATE_PATH}/snapshot_${MYSQL_VERSION%-*}_${NUM_TABLES}x${DATASIZE}
  local TEMPLATE_DIR=${TEMPLATE_PATH}/datadir_${MYSQL_VERSION%-*}_${NUM_TABLES}x${DATASIZE}

  if [[ ${WRITES_TIME_SECONDS} == 0 ]]; then return; fi

  if [ -d ${SNAPSHOT_DIR} ]; then copy_datadir "${SNAPSHOT_DIR}" "${DATA_DIR}"; return; fi

  if [ ! -d ${TEMPLATE_DIR} ]; then
    echo "Creating template data directory in ${TEMPLATE_DIR}"
    mkdir -p ${TEMPLATE_PATH} > /dev/null 2>&1
    ${TASKSET_MYSQLD} ${BUILD_PATH}/bin/mysqld --no-defaults --initialize-insecure --basedir=${BUILD_PATH} --datadir=${TEMPLATE_DIR} 2>&1

    LOG_NAME_MYSQLD=${LOGS_CONFIG}/prepare.mysqld
    start_mysqld "--datadir=${TEMPLATE_DIR} --disable-log-bin --innodb_flush_log_at_trx_commit=0 --innodb_fast_shutdown=0"
    ${BUILD_PATH}/bin/mysql -uroot -S$MYSQL_SOCKET -e "CREATE DATABASE IF NOT EXISTS $MYSQL_DATABASE" 2>&1
    pushd $SYSBENCH_LUA
    (time ${TASKSET_SYSBENCH} $SYSBENCH_BIN $SYSBENCH_WRITE --threads=$NUM_TABLES --rand-seed=$RAND_SEED $SYSBENCH_OPTIONS --mysql-socket=$MYSQL_SOCKET prepare) 2>&1
    popd
    echo "Data directory in ${TEMPLATE_DIR} created"
    shutdown_mysqld
  fi

  copy_datadir "${TEMPLATE_DIR}" "${DATA_DIR}"
}

function sysbench_warmup() {
  # *** REMEMBER *** warmmup is READ ONLY!
  # warmup the cache, 64 threads for $WORKLOAD_WARMUP_TIME seconds,
  num_threads=64
  echo "Warming up for $WORKLOAD_WARMUP_TIME seconds"
  LOG_NAME_MYSQLD=${LOGS_CONFIG}/sysbench_warmup_${WORKLOAD_NAME}.mysqld
  start_mysqld "--datadir=${DATA_DIR} --innodb_buffer_pool_load_at_startup=OFF"
  pushd $SYSBENCH_LUA
  ${TASKSET_SYSBENCH} $SYSBENCH_BIN $SYSBENCH_READ --threads=$num_threads --time=$WORKLOAD_WARMUP_TIME $SYSBENCH_OPTIONS --mysql-socket=$MYSQL_SOCKET run 2>&1
  popd
  shutdown_mysqld
  sleep $[WORKLOAD_WARMUP_TIME/10]
}

function run_sysbench() {
  echo "Using ${WORKLOAD_NAME}=${WORKLOAD_PARAMETERS}"

  if [[ ${WORKLOAD_WARMUP_TIME} > 0 ]]; then
    sysbench_warmup | tee ${LOGS_CONFIG}/sysbench_warmup_${WORKLOAD_NAME}.log
  fi

  if [[ ${SYSBENCH_RUN_TIME} > 0 ]]; then
  for num_threads in ${THREADS_LIST}; do
    echo "Testing $WORKLOAD_NAME with $num_threads threads for $SYSBENCH_RUN_TIME seconds"
    LOG_NAME_RESULTS=${LOGS_CONFIG}/${BENCH_ID}_${WORKLOAD_NAME}_results_qps.csv
    LOG_NAME=${LOGS_CONFIG}/${BENCH_ID}_${WORKLOAD_NAME}-$num_threads.txt
    LOG_NAME_MYSQL=${LOG_NAME}.mysql
    LOG_NAME_MYSQLD=${LOG_NAME}.mysqld
    LOG_NAME_MEMORY=${LOG_NAME}.memory
    LOG_NAME_IOSTAT=${LOG_NAME}.iostat
    LOG_NAME_VMSTAT=${LOG_NAME}.vmstat
    LOG_NAME_DSTAT=${LOG_NAME}.dstat
    LOG_NAME_DSTAT_CSV=${LOG_NAME}.dstat.csv
    LOG_NAME_CPUINFO=${LOG_NAME}.cpuinfo
    LOG_NAME_SMART=${LOG_NAME}.smart
    LOG_NAME_PS=${LOG_NAME}.ps
    LOG_NAME_ZONEINFO=${LOG_NAME}.zoneinfo
    start_mysqld "--datadir=${DATA_DIR}"

    if [[ ${BENCHMARK_LOGGING} == "Y" ]]; then
        # verbose logging
        echo "*** verbose benchmark logging enabled ***"
        report_thread &
        REPORT_THREAD_PID=$!
        (iostat -dxm $IOSTAT_INTERVAL 1000000 | grep -v loop > $LOG_NAME_IOSTAT) &
        if [[ ${DSTAT_OUTPUT_NOT_SUPPORTED} == "1" ]]; then
          dstat -t -v --nocolor $DSTAT_INTERVAL 1000000 > $LOG_NAME_DSTAT &
        else
          dstat -t -v --nocolor --output $LOG_NAME_DSTAT_CSV $DSTAT_INTERVAL 1000000 > $LOG_NAME_DSTAT &
        fi
    fi
    pushd $SYSBENCH_LUA
    local ALL_SYSBENCH_OPTIONS="$WORKLOAD_PARAMETERS --threads=$num_threads --time=$SYSBENCH_RUN_TIME --warmup-time=$WARMUP_TIME_SECONDS --rand-seed=$(( RAND_SEED + num_threads*num_threads )) $SYSBENCH_OPTIONS --mysql-socket=$MYSQL_SOCKET run"
    echo "Starting sysbench with options $ALL_SYSBENCH_OPTIONS" | tee $LOG_NAME
    (time ${TASKSET_SYSBENCH} $SYSBENCH_BIN $ALL_SYSBENCH_OPTIONS) 2>&1 | tee -a $LOG_NAME
    popd

    sleep 6
    pkill -f dstat
    pkill -f iostat
    kill -9 ${REPORT_THREAD_PID}
    result_set+=(`grep  "queries:" $LOG_NAME | cut -d'(' -f2 | awk '{print $1}'`)
    ${BUILD_PATH}/bin/mysql -uroot -S$MYSQL_SOCKET -e "SELECT @@innodb_flush_method; SHOW GLOBAL STATUS; SHOW ENGINE InnoDB STATUS\G; SHOW ENGINE INNODB MUTEX" >> ${LOG_NAME_MYSQL} 2>&1
    if [[ ${PERF_EXTRA} != "" ]]; then
      ${BUILD_PATH}/bin/mysql -uroot -S$MYSQL_SOCKET -e "SELECT EVENT_NAME, COUNT_STAR, SUM_TIMER_WAIT/1000000000 SUM_TIMER_WAIT_MS FROM performance_schema.events_waits_summary_global_by_event_name WHERE SUM_TIMER_WAIT > 0 AND EVENT_NAME LIKE 'wait/synch/mutex/innodb/%' ORDER BY COUNT_STAR DESC" >> ${LOG_NAME_MYSQL} 2>&1
    fi
    shutdown_mysqld | tee -a $LOG_NAME
    kill -9 $(pgrep -f ${DATA_DIR}) 2>/dev/null
    sync
  done

  local LOG_RESULTS_PATH="${CACHE_DIR}/${BENCH_ID}_${CONFIG_BASE^^}_$(basename "${WORKLOAD_NAMES}" .txt)"
  local LOG_RESULTS_CACHE="${LOG_RESULTS_PATH}/${WORKLOAD_NAME}_${SCALING_GOVERNOR}_${THREADS_LIST// /_}.csv"
  local BENCH_WITH_CONFIG="${BENCH_ID}_${CONFIG_BASE}_${WORKLOAD_NAME}_${BENCH_NAME}"
  local RESULTS_LINE="${BENCH_WITH_CONFIG}_qps"
  for number in "${result_set[@]}"; do RESULTS_LINE+=", ${number}"; done

  mkdir -p $LOG_RESULTS_PATH
  echo "${RESULTS_LINE}" > ${LOG_NAME_RESULTS}
  cat ${LOG_NAME_RESULTS} >> ${LOG_RESULTS_CACHE}
  cat ${LOG_NAME_RESULTS} >> ${LOGS_QPS}/${BENCH_ID}_${WORKLOAD_NAME}_${BENCH_NAME}_qps.csv
  echo "${BENCH_WITH_CONFIG}_diff$(diff_to_average "${LOG_RESULTS_CACHE}")" >> ${LOGS_DIFF}/${BENCH_ID}_${WORKLOAD_NAME}_${BENCH_NAME}_diff.csv
  echo "${BENCH_WITH_CONFIG}_stddev$(standard_deviation_percent "${LOG_RESULTS_CACHE}")" >> ${LOGS_STDDEV}/${BENCH_ID}_${WORKLOAD_NAME}_${BENCH_NAME}_stddev.csv
  echo "${BENCH_WITH_CONFIG}_avg$(average "${LOG_RESULTS_CACHE}")" >> ${LOGS_AVG}/${BENCH_ID}_${WORKLOAD_NAME}_${BENCH_NAME}_avg.csv
  unset result_set
  fi
}

function db_bench_init() {
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
  if [ -z "$BENCH_NAME" ]; then usage "Error: BENCH_NAME is not defined or is empty."; fi
  if [ -z "$BUILD_PATH" ]; then usage "Error: BUILD_PATH is not defined or is empty."; fi
  if [ -z "$CONFIG_FILES" ]; then usage "Error: CONFIG_FILES is not defined or is empty."; fi
  if [ ! -x $BUILD_PATH/bin/mysqld ]; then usage "ERROR: Executable $BUILD_PATH/bin/mysqld not found."; fi

  rm -rf ${LOGS}
  mkdir -p ${LOGS} ${LOGS_AVG} ${LOGS_STDDEV} ${LOGS_DIFF} ${LOGS_QPS} ${CACHE_DIR}
  cd $WORKSPACE

  get_build_info
  process_workload_names
  if [ ${#WORKLOAD_ARRAY[@]} -eq 0 ]; then usage "ERROR: Maching workloads names from $WORKLOAD_NAMES are empty."; fi

  export INNODB_CACHE=${INNODB_CACHE:-32G}
  export NUM_TABLES=${NUM_TABLES:-16}
  export DATASIZE=${DATASIZE:-10M}
  export BENCH_ID=$(uname -n)_${MYSQL_NAME}${MYSQL_VERSION}_${WRITES_TIME_SECONDS}sec_${NUM_TABLES}x${DATASIZE}-${INNODB_CACHE}

  if [[ ${RESULTS_EMAIL} != "" ]]; then
    echo "- Sending e-mail to ${RESULTS_EMAIL}"
    local NICE_DATE=$(date +"%Y-%m-%d %H:%M")
    print_parameters "" | mutt -s "Start $(basename "${CONFIG_FILES}" .cnf) $(basename "${WORKLOAD_NAMES^^}" .TXT) ${BENCH_ID}_${BENCH_NAME} at ${NICE_DATE}" -- ${RESULTS_EMAIL}
  fi

  local LOG_SYS_INFO=$LOGS/sys_info_start.txt
  print_system_info >> ${LOG_SYS_INFO}

  if [[ ${SCALING_GOVERNOR} != "" ]]; then
    disable_address_randomization > ${LOG_SYS_INFO}2
    disable_turbo_boost >> ${LOG_SYS_INFO}2
    change_scaling_governor ${SCALING_GOVERNOR} >> ${LOG_SYS_INFO}2
    cat ${LOG_SYS_INFO}2 | tee -a ${LOG_SYS_INFO}; rm ${LOG_SYS_INFO}2
    if [[ ${DISABLE_IDLE_STATES} == "yes" ]]; then
      disable_idle_states >> ${LOG_SYS_INFO}
    fi
  fi

  local LOGS_BUILD_INFO=${LOGS}/build_info.txt
  echo -e "Date: `date +'%d-%m-%Y %H:%M'`\nNode: $(uname -n)\nMySQL version: ${MYSQL_NAME}${MYSQL_VERSION} = ${MYSQL_VERSION_LONG}\n" | tee ${LOGS_BUILD_INFO}
  print_parameters "" | tee -a ${LOGS_BUILD_INFO}
  echo "=========="

  init_perf_tests
  echo "Storing Sysbench results in ${WORKSPACE}"

  trap on_exit EXIT KILL
}

function db_bench_init_config() {
  if [ ! -f $MYSQL_CONFIG_FILE ]; then usage "ERROR: Config file $MYSQL_CONFIG_FILE not found."; fi
  CONFIG_BASE=$(basename ${file%.*})
  LOGS_CONFIG=${LOGS}/${BENCH_NAME}-${CONFIG_BASE}
  mkdir -p ${LOGS_CONFIG}
  CONFIG_FILE=${LOGS_CONFIG}/$(basename $MYSQL_CONFIG_FILE)
  cp $MYSQL_CONFIG_FILE $CONFIG_FILE
  echo "Using $CONFIG_FILE as mysqld config file"

  MYSQL_SOCKET=${LOGS}/ps_socket.sock
  timeout --signal=9 30s ${BUILD_PATH}/bin/mysqladmin -uroot --socket=$MYSQL_SOCKET shutdown > /dev/null 2>&1
  kill -9 $(pgrep -f ${DATA_DIR}) 2>/dev/null
}
