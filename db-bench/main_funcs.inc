#!/bin/echo This script should be sourced in a shell, not executed directly

#**********************************************************************************************
# Percona Database Benchmark scripts
# main_funcs.inc - mysqld/postgres and sysbench related functions
#**********************************************************************************************

function usage(){
  echo "$1"
  echo -e "\nYou have to set the following env variables:"
  echo "<WORKSPACE> - path to store results (default: \$PWD)"
  echo "<BENCH_NAME> - name of benchmark (a directory with this name will be created in \$WORKSPACE)"
  echo "<BUILD_PATH> - path to PostgreSQL or MySQL or Percona Server binaries"
  echo "<CONFIG_FILES> - full path to Percona Server's configuration file"
  echo -e "\nUsage example:"
  echo "export WORKSPACE=/data/perf-results"
  echo "export CONFIG_FILES=/data/perf-tests/cnf/innodb-84.cnf"
  echo "export BUILD_PATH=/data/release-9.0.1-1-rel-gcc-13"
  echo "export BENCH_NAME=release-9.0.1-1@6feb03ff8ed"
  echo "bash $0"
  exit 1
}

# depends on $DATA_DIR, $LOGS, $SCALING_GOVERNOR, $BENCH_ID, $BENCH_NAME, $START_TIME, $THREADS_LIST, $CONFIG_LABELS, $WORKLOAD_NAMES, $SLACK_WEBHOOK_URL, $WORKSPACE, $BACKUP_DIR, $RESULTS_EMAIL
function on_exit(){
  pkill -f dstat
  pkill -f iostat
  killall -9 mysqld postgres

  echo "Remove DATA_DIR=$DATA_DIR"
  rm -rf ${DATA_DIR}
  # discard the freed blocks early, so the storage can recover while the
  # results below are being processed
  trim_filesystems

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

  local ALL_LOGS_BASE=${LOGS}/${BENCH_ID}_${BENCH_NAME}
  local LOG_BASE_QPS=${LOGS}/${BENCH_ID}_${BENCH_NAME}_qps
  local LOG_BASE_DIFF=${LOGS}/${BENCH_ID}_${BENCH_NAME}_diff
  local LOG_BASE_STDDEV=${LOGS}/${BENCH_ID}_${BENCH_NAME}_stddev
  local LOG_BASE_AVG=${LOGS}/${BENCH_ID}_${BENCH_NAME}_avg
  local END_TIME=$(date +%s)
  local DURATION=$((END_TIME - START_TIME))
  local TIME_HMS=$(printf "%02d:%02d:%02d" $((DURATION / 3600)) $(((DURATION % 3600) / 60)) $((DURATION % 60)))

  HEADER="WORKLOAD"
  for num_threads in ${THREADS_LIST}; do HEADER+=", ${num_threads} THDS"; done

  echo "Create .csv files"
  echo "${HEADER}" > ${LOG_BASE_QPS}.csv
  cat ${LOGS_QPS}/*${BENCH_NAME}_qps.csv >> ${LOG_BASE_QPS}.csv
  echo "${HEADER}" > ${LOG_BASE_DIFF}.csv
  cat ${LOGS_DIFF}/*${BENCH_NAME}_diff.csv >> ${LOG_BASE_DIFF}.csv
  echo "${HEADER}" > ${LOG_BASE_STDDEV}.csv
  cat ${LOGS_STDDEV}/*${BENCH_NAME}_stddev.csv >> ${LOG_BASE_STDDEV}.csv
  echo "${HEADER}" > ${LOG_BASE_AVG}.csv
  cat ${LOGS_AVG}/*${BENCH_NAME}_avg.csv >> ${LOG_BASE_AVG}.csv

  echo "Create .html files"
  echo -e "Script executed in $TIME_HMS ($DURATION seconds)<BR>\n<BR>\n" > ${ALL_LOGS_BASE}.html
  print_variables_and_workloads "<BR>" >> ${ALL_LOGS_BASE}.html
  echo -e "<BR>" >> ${ALL_LOGS_BASE}.html
  echo -e "QPS results:<BR>" >> ${LOG_BASE_QPS}.html
  csv_to_html_table ${LOG_BASE_QPS}.csv >> ${LOG_BASE_QPS}.html
  echo "Average QPS:<BR>" > ${LOG_BASE_AVG}.html
  csv_to_html_table ${LOG_BASE_AVG}.csv >> ${LOG_BASE_AVG}.html
  echo "Difference in percentages to the average QPS:<BR>" > ${LOG_BASE_DIFF}.html
  csv_to_html_table ${LOG_BASE_DIFF}.csv "color" >> ${LOG_BASE_DIFF}.html
  echo "Standard deviation as a percentage of the average QPS:<BR>" > ${LOG_BASE_STDDEV}.html
  csv_to_html_table ${LOG_BASE_STDDEV}.csv "color" >> ${LOG_BASE_STDDEV}.html
  create_html_page ${LOG_BASE_QPS}.html ${LOG_BASE_AVG}.html ${LOG_BASE_DIFF}.html ${LOG_BASE_STDDEV}.html >> ${ALL_LOGS_BASE}.html

  local tarFileName="${BENCH_ID}_${BENCH_NAME}.tar.gz"
  local NICE_DATE=$(date +"%Y-%m-%d %H:%M")
  local SUBJECT="Done ($RUN_NAME) [${CONFIG_LABELS}] $(basename "${WORKLOAD_NAMES^^}" .TXT) ${BENCH_ID}_${BENCH_NAME} at ${NICE_DATE}"

  if [[ ${SLACK_WEBHOOK_URL} != "" ]]; then
    echo "- Sending slack message"
    SLACK_MESSAGE="${SUBJECT}\nScript executed in $TIME_HMS ($DURATION seconds)\nWORKLOAD_NAMES=${WORKLOAD_NAMES}\nBUILD_PATH=${BUILD_PATH}\nDATA_DIR=${DATA_DIR}\n" ${SCRIPT_DIR}/db-bench/publish_to_slack.py ${LOG_BASE_QPS}.csv ${LOG_BASE_DIFF}.csv ${LOG_BASE_STDDEV}.csv
  fi

  echo "Script executed in $TIME_HMS ($DURATION seconds)" | tee -a ${LOG_BASE_QPS}.csv
  echo "-----" && cat ${LOG_BASE_QPS}.csv && echo "-----" && cat ${LOG_BASE_AVG}.csv && echo "-----" && cat ${LOG_BASE_DIFF}.csv && echo "-----" && cat ${LOG_BASE_STDDEV}.csv && echo "-----"

  cd $WORKSPACE
  tar czvf ${tarFileName} ${BENCH_NAME} --force-local --transform "s+^${BENCH_NAME}++"

  if [[ ${BACKUP_DIR} != "" ]]; then
    echo "- Copying ${tarFileName} to ${BACKUP_DIR}"
    cp ${tarFileName} ${BACKUP_DIR}/
    cp ${LOGS}/${BENCH_ID}_${BENCH_NAME}*.csv ${BACKUP_DIR}/
    cat ${ALL_LOGS_BASE}.html > ${BACKUP_DIR}/${BENCH_ID}_${BENCH_NAME}.html
  fi

  if [[ ${RESULTS_EMAIL} != "" ]]; then
    echo "- Sending e-mail to ${RESULTS_EMAIL} with ${tarFileName}"
    cat ${ALL_LOGS_BASE}.html | mutt -s "${SUBJECT}" -e "set content_type=text/html" -a ${tarFileName} -- ${RESULTS_EMAIL}
  fi

  # last thing to do: the results are already published, only the next
  # benchmark on this machine waits for it
  idle_after_bench
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
    ps aux --sort=-%cpu | head -n 20 >> ${LOG_NAME_PS} 2>/dev/null || true
    echo "${DATE} $SMART_DEVICE" >> ${LOG_NAME_SMART}
    sudo smartctl -A $SMART_DEVICE >> ${LOG_NAME_SMART} 2>&1
    sleep ${REPORT_INTERVAL}
  done
}



# print_database_size
function print_database_size() {
  local DATA_DIR=$1
  local DATA_SIZE=`du -s $DATA_DIR | awk '{sum+=$1;} END {printf "%d\n", sum/1024;}'`
  local FILE_COUNT=`ls -aR $DATA_DIR | wc -l`
  echo "- Size of database is $DATA_SIZE MB in $FILE_COUNT files"
}



function snapshot_datadir() {
  echo "Copying data directory from ${DATA_DIR} to ${SNAPSHOT_DIR}"
  rm -rf ${SNAPSHOT_DIR}
  (time cp -r ${DATA_DIR} ${SNAPSHOT_DIR}) 2>&1
  print_database_size ${DATA_DIR}
}

function copy_datadir() {
  local FROM_DIR=$1
  local TO_DIR=$2
  if [ $# -lt 2 ]; then echo "Usage: copy_datadir <FROM_DIR> <TO_DIR>"; return 1; fi

  echo "Copying data directory from ${FROM_DIR} to ${TO_DIR}"
  rm -rf ${TO_DIR}
  (time cp -r ${FROM_DIR} ${TO_DIR}) 2>&1
  print_database_size ${TO_DIR}
}

function start_server() {
  if [[ ${ENGINE} == "postgres" ]]; then
    start_postgres "-D ${DATA_DIR}"
  else
    start_mysqld "--datadir=${DATA_DIR}"
  fi
}

function shutdown_server() {
  if [[ ${ENGINE} == "postgres" ]]; then
    shutdown_postgres "${DATA_DIR}"
  else
    shutdown_mysqld
  fi
}

# true when the server has to be restarted before every concurrency level
function restart_per_run() {
  case ${RESTART_SERVER^^} in
    N|NO|OFF|0|FALSE) return 1 ;;
    *) return 0 ;;
  esac
}

function run_sysbench() {
  echo "Using ${WORKLOAD_NAME}=${WORKLOAD_PARAMETERS}"

  if [[ ${SYSBENCH_RUN_TIME} > 0 ]]; then

  # with RESTART_SERVER=N a single server instance serves all concurrency levels
  # of this workload, so it is started here and shut down after the last one
  if ! restart_per_run; then
    LOG_NAME=${LOGS_CONFIG}/${BENCH_ID}_${WORKLOAD_NAME}-all_threads.txt
    LOG_NAME_SERVERD=${LOG_NAME}.${SERVER_TYPE}d
    echo "*** RESTART_SERVER=${RESTART_SERVER}: not restarting between concurrency levels ***"
    start_server
  fi

  for num_threads in ${THREADS_LIST}; do
    echo "Testing $WORKLOAD_NAME with $num_threads threads for $SYSBENCH_RUN_TIME seconds"
    LOG_NAME_RESULTS=${LOGS_CONFIG}/${BENCH_ID}_${WORKLOAD_NAME}_results_qps.csv
    LOG_NAME=${LOGS_CONFIG}/${BENCH_ID}_${WORKLOAD_NAME}-$num_threads.txt
    LOG_NAME_SERVER=${LOG_NAME}.${SERVER_TYPE}
    LOG_NAME_SERVERD=${LOG_NAME}.${SERVER_TYPE}d
    LOG_NAME_MEMORY=${LOG_NAME}.memory
    LOG_NAME_VARIABLES=${LOG_NAME}.variables
    LOG_NAME_IOSTAT=${LOG_NAME}.iostat
    LOG_NAME_VMSTAT=${LOG_NAME}.vmstat
    LOG_NAME_DSTAT=${LOG_NAME}.dstat
    LOG_NAME_DSTAT_CSV=${LOG_NAME}.dstat.csv
    LOG_NAME_CPUINFO=${LOG_NAME}.cpuinfo
    LOG_NAME_SMART=${LOG_NAME}.smart
    LOG_NAME_PS=${LOG_NAME}.ps
    LOG_NAME_ZONEINFO=${LOG_NAME}.zoneinfo
    if restart_per_run; then start_server; fi

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
    sysbench_${SERVER_TYPE}_options
    echo "Starting sysbench with options $ALL_SYSBENCH_OPTIONS" | tee $LOG_NAME
    (time $SYSBENCH_BIN $ALL_SYSBENCH_OPTIONS run) 2>&1 | tee -a $LOG_NAME
    popd

    sleep 6
    pkill -f dstat
    pkill -f iostat
    kill -9 ${REPORT_THREAD_PID}
    result_set+=(`grep  "queries:" $LOG_NAME | cut -d'(' -f2 | awk '{print $1}'`)
    ${SERVER_TYPE}_before_shutdown
    if restart_per_run; then
      shutdown_server | tee -a $LOG_NAME
      kill -9 $(pgrep -f ${DATA_DIR}) 2>/dev/null
    else
      ${SERVER_TYPE}_between_runs | tee -a $LOG_NAME
    fi
    sync
  done

  if ! restart_per_run; then
    shutdown_server | tee -a $LOG_NAME
    kill -9 $(pgrep -f ${DATA_DIR}) 2>/dev/null
    sync
  fi

  local LOG_RESULTS_PATH="${CACHE_DIR}/${BENCH_ID}_${CONFIG_LABELS^^}_$(basename "${WORKLOAD_NAMES}" .txt)_${RUN_NAME}"
  local LOG_RESULTS_CACHE="${LOG_RESULTS_PATH}/${WORKLOAD_NAME}_${SCALING_GOVERNOR}_${THREADS_LIST// /_}.csv"
  local BENCH_WITH_CONFIG="${BENCH_ID}_${CONFIG_LABELS}_${WORKLOAD_NAME}_${BENCH_NAME}"
  local RESULTS_LINE="${BENCH_WITH_CONFIG}_qps"
  for number in "${result_set[@]}"; do RESULTS_LINE+=", ${number}"; done

  mkdir -p $LOG_RESULTS_PATH
  echo "${RESULTS_LINE}" > ${LOG_NAME_RESULTS}
  cat ${LOG_NAME_RESULTS} >> ${LOG_RESULTS_CACHE}
  cat ${LOG_NAME_RESULTS} >> ${LOGS_QPS}/${BENCH_ID}_${WORKLOAD_NAME}-${BENCH_NAME}_qps.csv
  echo "${BENCH_WITH_CONFIG}_diff$(diff_to_average "${LOG_RESULTS_CACHE}")" >> ${LOGS_DIFF}/${BENCH_ID}_${WORKLOAD_NAME}-${BENCH_NAME}_diff.csv
  echo "${BENCH_WITH_CONFIG}_stddev$(standard_deviation_percent "${LOG_RESULTS_CACHE}")" >> ${LOGS_STDDEV}/${BENCH_ID}_${WORKLOAD_NAME}-${BENCH_NAME}_stddev.csv
  echo "${BENCH_WITH_CONFIG}_avg$(average "${LOG_RESULTS_CACHE}")" >> ${LOGS_AVG}/${BENCH_ID}_${WORKLOAD_NAME}-${BENCH_NAME}_avg.csv
  unset result_set
  fi
}

function init_global_variables() {
  export START_TIME=$(date +%s)
  export LOGS=$BENCH_DIR
  export LOGS_AVG=${LOGS}/${BENCH_NAME}-avg
  export LOGS_STDDEV=${LOGS}/${BENCH_NAME}-stddev
  export LOGS_DIFF=${LOGS}/${BENCH_NAME}-diff
  export LOGS_QPS=${LOGS}/${BENCH_NAME}-qps

  ENGINE_CACHE=${ENGINE_CACHE:-32G}
  NUM_TABLES=${NUM_TABLES:-16}
  DATASIZE=${DATASIZE:-10M}
  SNAPSHOT_DIR=${TEMPLATE_PATH}/snapshot_${SERVER_VERSION%%-*}_${NUM_TABLES}x${DATASIZE}
  HOSTNAME=$(uname -n)
  export BENCH_ID=${HOSTNAME%%.*}_${SERVER_NAME}${SERVER_VERSION}_${WRITES_TIME_SECONDS}sec_${NUM_TABLES}x${DATASIZE}_${ENGINE}-${ENGINE_CACHE}

  set_${SERVER_TYPE}_sysbench_options
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

  if [[ ${RESULTS_EMAIL} != "" ]]; then
    command -v mutt >/dev/null 2>&1 || { echo >&2 "mutt is not installed. Aborting."; exit 1; }
  fi

  if [ "$ENGINE" != "innodb" ] && [ "$ENGINE" != "rocksdb" ] && [ "$ENGINE" != "postgres" ]; then usage "error: unknown $ENGINE storage engine"; fi

  # check parameters
  if [ -z "$BENCH_NAME" ]; then usage "Error: BENCH_NAME is not defined or is empty."; fi
  if [ -z "$BUILD_PATH" ]; then usage "Error: BUILD_PATH is not defined or is empty."; fi
  if [ -z "$CONFIG_FILES" ]; then usage "Error: CONFIG_FILES is not defined or is empty."; fi

  process_workload_names
  if [ ${#WORKLOAD_ARRAY[@]} -eq 0 ]; then usage "ERROR: Maching workloads names from $WORKLOAD_NAMES are empty."; fi

  if [[ ${ENGINE} == "postgres" ]]; then
    check_postgres_binaries
    get_postgres_build_info
  else
    check_mysql_binaries
    get_mysql_build_info
  fi
  init_global_variables

  rm -rf ${LOGS}
  mkdir -p ${LOGS} ${LOGS_AVG} ${LOGS_STDDEV} ${LOGS_DIFF} ${LOGS_QPS} ${CACHE_DIR}
  cd $WORKSPACE

  EXTRA_CONFIG_LABELS=""
  if [[ -n "${EXTRA_CONFIG_FILES}" ]]; then
    for extra_file in $EXTRA_CONFIG_FILES; do
      if [ ! -f $extra_file ]; then usage "ERROR: Extra config file $extra_file not found."; fi
      local name=$(basename "${extra_file%.*}")
      EXTRA_CONFIG_LABELS+="+${name#extra-}"
    done
  fi
  export CONFIG_LABELS=$(basename "${CONFIG_FILES%.*}")
  CONFIG_LABELS+=${EXTRA_CONFIG_LABELS}

  if [[ ${RESULTS_EMAIL} != "" ]]; then
    echo "- Sending e-mail to ${RESULTS_EMAIL}"
    local NICE_DATE=$(date +"%Y-%m-%d %H:%M")
    print_variables_and_workloads "" | mutt -s "Start ($RUN_NAME) [${CONFIG_LABELS}] $(basename "${WORKLOAD_NAMES^^}" .TXT) ${BENCH_ID}_${BENCH_NAME} at ${NICE_DATE}" -- ${RESULTS_EMAIL}
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
  local LOGS_VARIABLES_INC=${LOGS}/variables.inc.sh
  echo -e "Date: `date +'%d-%m-%Y %H:%M'`\nNode: $(uname -n)\n${SERVER_TYPE} version: ${SERVER_NAME}${SERVER_VERSION} = ${SERVER_VERSION_LONG}\n" | tee ${LOGS_BUILD_INFO}
  print_system_variables "" | tee -a ${LOGS_VARIABLES_INC}
  echo "=========="
  print_workloads "" | tee -a ${LOGS_BUILD_INFO}
  echo "=========="

  echo "Storing Sysbench results in ${WORKSPACE}"

  trap on_exit EXIT KILL
}

function db_bench_init_config() {
  if [ ! -f $SERVER_CONFIG_FILE ]; then usage "ERROR: Config file $SERVER_CONFIG_FILE not found."; fi
  CONFIG_LABELS=$(basename "${SERVER_CONFIG_FILE%.*}")
  CONFIG_LABELS+=${EXTRA_CONFIG_LABELS}

  LOGS_CONFIG=${LOGS}/${BENCH_NAME}-${CONFIG_LABELS}
  mkdir -p ${LOGS_CONFIG}
  CONFIG_FILE=${LOGS_CONFIG}/${CONFIG_LABELS}.cnf
  cp $SERVER_CONFIG_FILE $CONFIG_FILE
  echo "Using $CONFIG_FILE as ${SERVER_TYPE} config file"

  if [[ -n "${EXTRA_CONFIG_FILES}" ]]; then
    for extra_file in $EXTRA_CONFIG_FILES; do
      if [ ! -f $extra_file ]; then usage "ERROR: Extra config file $extra_file not found."; fi
      echo "" >> $CONFIG_FILE
      echo "# ===== Included from $(basename "$extra_file") =====" >> $CONFIG_FILE
      cat $extra_file >> $CONFIG_FILE
    done
    echo "Appended extra config files to $CONFIG_FILE (from: $EXTRA_CONFIG_FILES)"
  fi

  echo "" >> $CONFIG_FILE
  echo "# ===== Configured by user from \$ENGINE_CACHE=${ENGINE_CACHE} =====" >> $CONFIG_FILE
  ${SERVER_TYPE}_append_engine_cache

  ${SERVER_TYPE}_killall
}
