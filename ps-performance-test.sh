#!/bin/bash
# set -x

#**********************************************************************************************
# PS performance benchmark scripts
# Sysbench suite will run performance tests
#**********************************************************************************************

# script parameters
export BENCH_NAME=$1
export BUILD_PATH=$2
export CONFIG_FILES="$3"

# generic variables
export RPORT=$(( RANDOM%21 + 10 ))
export RBASE="$(( RPORT*1000 ))"
export WORKSPACE=${WORKSPACE:-${PWD}}
export TEMPLATE_PATH=${TEMPLATE_PATH:-${WORKSPACE}/template_datadir}
export RESULTS_DIR=${RESULTS_DIR:-$WORKSPACE/results_to_compare}
export BENCHMARK_LOGGING=${BENCHMARK_LOGGING:-Y}
export SMART_DEVICE=${SMART_DEVICE:-/dev/nvme0n1}
SCRIPT_DIR=$(cd $(dirname $0) && pwd)
WORKLOAD_SCRIPT=${WORKLOAD_SCRIPT:-$SCRIPT_DIR/workloads/read_write.txt}

# sysbench variables
export MYSQL_DATABASE=test
export SUSER=root
export RAND_TYPE=${RAND_TYPE:-uniform}
export RAND_SEED=${RAND_SEED:-1111}
export THREADS_LIST=${THREADS_LIST:-"1 4 16 64 128 256 512 1024"}
SYSBENCH_BIN=${SYSBENCH_BIN:-sysbench}
SYSBENCH_LUA=${SYSBENCH_LUA:-/usr/local/share/sysbench}
SYSBENCH_WRITE=${SYSBENCH_WRITE:-oltp_write_only.lua}
SYSBENCH_READ=${SYSBENCH_READ:-oltp_read_only.lua}
export EVENTS_MULT=${EVENTS_MULT:-1}

# time variables
export PS_START_TIMEOUT=${PS_START_TIMEOUT:-180}
WORKLOAD_WARMUP_TIME=${WORKLOAD_WARMUP_TIME:-0}
export WARMUP_TIME_SECONDS=${WARMUP_TIME_SECONDS:-0}
export RUN_TIME_SECONDS=${RUN_TIME_SECONDS:-600}
export REPORT_INTERVAL=10
export IOSTAT_INTERVAL=10
export DSTAT_INTERVAL=10

#MYEXTRA=${MYEXTRA:=--disable-log-bin}
#PERF_EXTRA=${PERF_EXTRA:=--performance-schema-instrument='wait/synch/mutex/innodb/%=ON'}

#TASKSET_MYSQLD=${TASKSET_MYSQLD:=taskset -c 0}
#TASKSET_SYSBENCH=${TASKSET_SYSBENCH:=taskset -c 1}


function usage(){
  echo $1
  echo "Usage: $0 <BENCH_NAME> <BUILD_PATH> <MYSQL_CONFIG_FILE>"
  echo "where:"
  echo "<BENCH_NAME> - name of benchmark (a directory with this name will be created in \$WORKSPACE)"
  echo "<BUILD_PATH> - path to MySQL or Percona Server binaries"
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
  local LOG_SYS_INFO=$LOGS/sys_info.log
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

  uname -a >> $LOG_SYS_INFO
  ulimit -a >> $LOG_SYS_INFO
  sysctl -a 2>/dev/null | grep "\bvm." >> $LOG_SYS_INFO
  free -m >> $LOG_SYS_INFO
  df -Th >> $LOG_SYS_INFO
  echo "===== nproc=$(nproc --all)" >> $LOG_SYS_INFO
  cat /proc/cpuinfo >> $LOG_SYS_INFO
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

function print_parameters() {
  local ENDLINE=$1
  variables=("INNODB_CACHE" "NUM_TABLES" "DATASIZE" "THREADS_LIST" "RUN_TIME_SECONDS" "WARMUP_TIME_SECONDS" "WORKLOAD_WARMUP_TIME"
             "RESULTS_EMAIL" "RESULTS_DIR" "WORKSPACE" "BENCH_DIR" "BUILD_PATH" "MYEXTRA" "SYSBENCH_EXTRA" "CONFIG_FILES" "WORKLOAD_SCRIPT")
  for variable in "${variables[@]}"; do echo "$variable=${!variable}${ENDLINE}"; done
  echo "=====${ENDLINE}"
  for ((i=0; i<${#WORKLOAD_NAMES[@]}; i++)); do
    WORKLOAD_PARAMETERS=$(eval echo ${WORKLOAD_PARAMS[i]})
    echo "${WORKLOAD_NAMES[i]}=${WORKLOAD_PARAMETERS}${ENDLINE}"
  done
  echo "=====${ENDLINE}"
}

function diff_to_average() {
    local csv_file="$1"
    diff_output=$(awk -F ',' 'BEGIN {
        for (i=2; i<=NF; i++) {
            sum[i] = 0
            count[i] = 0
        }
    }
    {
        if (FNR != total_rows) { # Process all rows except the last one
            for (i=2; i<=NF; i++) {
                if ($i != "") {
                    count[i]++
                    sum[i] += $i
                }
            }
        } else { # Process the last row
            for (i=2; i<=NF; i++) {
                last_row_data[i] = $i
            }
        }
    }
    END {
        for (i=2; i<=NF; i++) {
          avg[i] = (count[i] > 0) ? sum[i] / count[i] : 0
          printf ", %.2f%%", ((last_row_data[i] - avg[i]) / avg[i]) * 100
        }
        printf "\n"

    }' total_rows=$(awk 'END{print NR}' "$csv_file") "$csv_file")
    echo $diff_output
}

function average() {
    local csv_file="$1"
    awk -F ',' 'BEGIN {
        for (i=2; i<=NF; i++) {
            sum[i] = 0
            count[i] = 0
        }
    }
    {
        for (i=2; i<=NF; i++) {
            if ($i != "") {
                count[i]++
                sum[i] += $i
            }
        }
    }
    END {
        for (i=2; i<=NF; i++) {
           avg[i] = (count[i] > 0) ? sum[i] / count[i] : 0
           printf ", %.2f", avg[i]
        }
        printf "\n"

    }' "$csv_file"
}

function standard_deviation_percent() {
    local csv_file="$1"
    awk -F ',' 'BEGIN {
        for (i=2; i<=NF; i++) {
            sum[i] = 0
            count[i] = 0
            sumsq[i] = 0
        }
    }
    {
        for (i=2; i<=NF; i++) {
            if ($i != "") {
                count[i]++
                sum[i] += $i
                sumsq[i] += $i^2
            }
        }
    }
    END {
        for (i=2; i<=NF; i++) {
           avg[i] = (count[i] > 0) ? sum[i] / count[i] : 0
           printf ", %.2f%%", (sqrt((sumsq[i]/count[i]) - (avg[i])**2) / avg[i]) * 100
        }
        printf "\n"

    }' "$csv_file"
}

function csv_to_html_table() {
    local INPUT_NAME=$1
    local USE_COLOR=$2

    echo "<table>"

    while IFS=',' read -r -a fields; do
        echo "  <tr>"
        for ((i=0; i<${#fields[@]}; i++)); do
            if [ -n "${fields[i]}" ]; then
              if [ $i -eq 0 ]; then
                  echo "    <td>${fields[i]}</td>"
              else
                  local FIELD="${fields[i]//[% ]/}"
                  if [ "$USE_COLOR" = "color" ] && [[ "${FIELD}" =~ ^-?[0-9]+(\.[0-9]+)?$ ]] && (( $(echo "${FIELD} > 1.0 || ${FIELD} < -1.0" | bc -l) )); then
                      echo "    <td style=\"text-align: right; color: red;\">${fields[i]}</td>"
                  else
                      echo "    <td style=\"text-align: right;\">${fields[i]}</td>"
                  fi
              fi
            fi
        done
        echo "  </tr>"
    done < "$INPUT_NAME"

    echo "</table>"
}

# depends on $LOGS, $LOGS_CPU, $BENCH_NAME, $DATA_DIR, $MYSQL_NAME, $MYSQL_VERSION, $NUM_TABLES, $DATASIZE, $INNODB_CACHE, $WORKSPACE, $THREADS_LIST, $RESULTS_EMAIL
function on_start(){
  if [[ ${RESULTS_EMAIL} != "" ]]; then
    echo "- Sending e-mail to ${RESULTS_EMAIL}"
    local NICE_DATE=$(date +"%Y-%m-%d %H:%M:%S")
    echo "" | mutt -s "$(uname -n): Perf benchmarking started for ${BENCH_ID}_${BENCH_NAME} at ${NICE_DATE}" -- ${RESULTS_EMAIL}
  fi

  disable_address_randomization >> ${LOGS_CPU}
  disable_turbo_boost > ${LOGS_CPU}
  change_scaling_governor powersave >> ${LOGS_CPU}
  disable_idle_states >> ${LOGS_CPU}

  trap on_exit EXIT KILL
}

function create_html_page() {
  echo "<!DOCTYPE html>"
  echo "<html>"
  echo "<head>"
  echo "<style>"
  echo "table, th, td {"
  echo "  border: 1px solid;"
  echo "  border-collapse: collapse;"
  echo "  border-color: #DDDDDD;"
  echo "}"
  echo "</style>"
  echo "</head>"
  echo "<body>"
  cat $1
  echo "<BR>"
  cat $2
  echo "<BR>"
  cat $3
  echo "</body>"
  echo "</html>"
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

  local LOG_BASE_FULL_RESULTS=${LOGS}/${BENCH_ID}_${BENCH_NAME}_qps
  local LOG_BASE_DIFF=${LOGS}/${BENCH_ID}_${BENCH_NAME}_diff
  local LOG_BASE_STDDEV=${LOGS}/${BENCH_ID}_${BENCH_NAME}_stddev
  local LOG_BASE_AVG=${LOGS}/${BENCH_ID}_${BENCH_NAME}_avg
  local END_TIME=$(date +%s)
  local DURATION=$((END_TIME - START_TIME))
  local TIME_HMS=$(printf "%02d:%02d:%02d" $((DURATION / 3600)) $(((DURATION % 3600) / 60)) $((DURATION % 60)))

  HEADER="WORKLOAD"
  for num_threads in ${THREADS_LIST}; do HEADER+=", ${num_threads} THREADS"; done

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
  echo -e "QPS results:<BR>" >> ${LOG_BASE_FULL_RESULTS}.html
  csv_to_html_table ${LOG_BASE_FULL_RESULTS}.csv >> ${LOG_BASE_FULL_RESULTS}.html
  echo "Difference in percentages to the average QPS:<BR>" > ${LOG_BASE_DIFF}.html
  csv_to_html_table ${LOG_BASE_DIFF}.csv "color" >> ${LOG_BASE_DIFF}.html
  echo "Standard deviation as a percentage of the average QPS:<BR>" > ${LOG_BASE_STDDEV}.html
  csv_to_html_table ${LOG_BASE_STDDEV}.csv "color" >> ${LOG_BASE_STDDEV}.html

  local tarFileName="${BENCH_ID}_${BENCH_NAME}.tar.gz"
  local NICE_DATE=$(date +"%Y-%m-%d %H:%M:%S")
  local SUBJECT="$(uname -n): Perf benchmarking finished for ${BENCH_ID}_${BENCH_NAME} at ${NICE_DATE}"

  if [[ ${SLACK_WEBHOOK_URL} != "" ]]; then
    echo "- Sending slack message"
    SLACK_MESSAGE="${SUBJECT}\nScript executed in $TIME_HMS ($DURATION seconds)" ${SCRIPT_DIR}/publish_to_slack.py ${LOG_BASE_FULL_RESULTS}.csv ${LOG_BASE_DIFF}.csv ${LOG_BASE_STDDEV}.csv
  fi

  echo "Script executed in $TIME_HMS ($DURATION seconds)" | tee -a ${LOG_BASE_FULL_RESULTS}.csv
  echo "-----" && cat ${LOG_BASE_FULL_RESULTS}.csv && echo "-----" && cat ${LOG_BASE_DIFF}.csv && echo "-----" && cat ${LOG_BASE_STDDEV}.csv && echo "-----"

  cd $WORKSPACE
  tar czvf ${tarFileName} ${BENCH_NAME} --force-local --transform "s+^${BENCH_NAME}++"

  if [[ ${RESULTS_EMAIL} != "" ]]; then
    echo "- Sending e-mail to ${RESULTS_EMAIL} with ${tarFileName}"
    create_html_page ${LOG_BASE_FULL_RESULTS}.html ${LOG_BASE_DIFF}.html ${LOG_BASE_STDDEV}.html | mutt -s "${SUBJECT}" -e "set content_type=text/html" -a ${tarFileName} -- ${RESULTS_EMAIL}
  fi

  rm -rf ${DATA_DIR}
}

function drop_caches(){
  echo "Dropping caches"
  sync
  sudo sh -c 'sysctl -q -w vm.drop_caches=3'
  sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'
  ulimit -n 1000000  # open files
  ulimit -l 524288   # max locked memory (kbytes)
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
  echo "Shutting mysqld down"
  (time ${BUILD_PATH}/bin/mysqladmin -uroot --socket=$MYSQL_SOCKET shutdown) 2>&1
  print_database_size
}

function init_perf_tests() {
  local NUM_ROWS=$(numfmt --from=si $DATASIZE)
  SYSBENCH_OPTIONS="--table-size=$NUM_ROWS --tables=$NUM_TABLES --mysql-db=$MYSQL_DATABASE --mysql-user=$SUSER --report-interval=10 --db-driver=mysql --mysql-ssl=DISABLED --db-ps-mode=disable --percentile=99 --rand-type=$RAND_TYPE $SYSBENCH_EXTRA"
}

function prepare_datadir() {
  local TEMPLATE_DIR=${TEMPLATE_PATH}/datadir_${MYSQL_VERSION%-*}_${NUM_TABLES}x${DATASIZE}
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
  echo "Copying data directory from ${TEMPLATE_DIR} to ${DATA_DIR}"
  rm -rf ${DATA_DIR}
  (time cp -r ${TEMPLATE_DIR} ${DATA_DIR}) 2>&1
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
  init_perf_tests

  echo "Storing Sysbench results in ${WORKSPACE}"

  for ((num=0; num<${#WORKLOAD_NAMES[@]}; num++)); do
    local WORKLOAD_NAME=${WORKLOAD_NAMES[num]}
    local WORKLOAD_PARAMETERS=$(eval echo ${WORKLOAD_PARAMS[num]})

    echo "Using ${WORKLOAD_NAME}=${WORKLOAD_PARAMETERS}"
    if [[ $num -eq 0 || ${PREV_WORKLOAD_NAME:0:3} == "WR_" ]]; then
      drop_caches
      prepare_datadir | tee ${LOGS_CONFIG}/prepare_datadir_${WORKLOAD_NAME}.log
    fi
    PREV_WORKLOAD_NAME=${WORKLOAD_NAME}

    if [[ ${WORKLOAD_WARMUP_TIME} > 0 ]]; then
      sysbench_warmup | tee ${LOGS_CONFIG}/sysbench_warmup_${WORKLOAD_NAME}.log
    fi

    for num_threads in ${THREADS_LIST}; do
      echo "Testing $WORKLOAD_NAME with $num_threads threads"
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
      local ALL_SYSBENCH_OPTIONS="$WORKLOAD_PARAMETERS --threads=$num_threads --time=$RUN_TIME_SECONDS --warmup-time=$WARMUP_TIME_SECONDS --rand-seed=$(( RAND_SEED + num_threads*num_threads )) $SYSBENCH_OPTIONS --mysql-socket=$MYSQL_SOCKET run"
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
    done

    local LOG_RESULTS_CACHE="${RESULTS_DIR}/${BENCH_ID}_${WORKLOAD_NAME}_${THREADS_LIST// /_}.csv"
    local BENCH_WITH_CONFIG="${BENCH_ID}_${CONFIG_BASE}_${WORKLOAD_NAME}_${BENCH_NAME}"
    local RESULTS_LINE="${BENCH_WITH_CONFIG}_qps"
    for number in "${result_set[@]}"; do RESULTS_LINE+=", ${number}"; done

    echo "${RESULTS_LINE}" >> ${LOG_NAME_RESULTS}
    cat ${LOG_NAME_RESULTS} >> ${LOG_RESULTS_CACHE}
    cat ${LOG_NAME_RESULTS} >> ${LOGS_QPS}/${BENCH_ID}_${WORKLOAD_NAME}_${BENCH_NAME}_qps.csv
    echo "${BENCH_WITH_CONFIG}_diff$(diff_to_average "${LOG_RESULTS_CACHE}")" >> ${LOGS_DIFF}/${BENCH_ID}_${WORKLOAD_NAME}_${BENCH_NAME}_diff.csv
    echo "${BENCH_WITH_CONFIG}_stddev$(standard_deviation_percent "${LOG_RESULTS_CACHE}")" >> ${LOGS_STDDEV}/${BENCH_ID}_${WORKLOAD_NAME}_${BENCH_NAME}_stddev.csv
    echo "${BENCH_WITH_CONFIG}_avg$(average "${LOG_RESULTS_CACHE}")" >> ${LOGS_AVG}/${BENCH_ID}_${WORKLOAD_NAME}_${BENCH_NAME}_avg.csv
    unset result_set
  done
}


#**********************************************************************************************
# main
#**********************************************************************************************
if [[ ${BENCHMARK_LOGGING} == "Y" ]]; then
  command -v cpupower >/dev/null 2>&1 || { echo >&2 "cpupower is not installed. Aborting."; exit 1; }
  command -v dstat >/dev/null 2>&1 || { echo >&2 "dstat is not installed. Aborting."; exit 1; }
  command -v iostat >/dev/null 2>&1 || { echo >&2 "iostat is not installed. Aborting."; exit 1; }
  dstat -t -v --nocolor --output dstat.csv 1 1 >/dev/null 2>&1 || DSTAT_OUTPUT_NOT_SUPPORTED=1
  rm dstat.csv
fi

export START_TIME=$(date +%s)
export BENCH_DIR=$WORKSPACE/$BENCH_NAME
export DATA_DIR=$BENCH_DIR-datadir
export LOGS=$BENCH_DIR
export LOGS_AVG=${LOGS}/${BENCH_NAME}-avg
export LOGS_STDDEV=${LOGS}/${BENCH_NAME}-stddev
export LOGS_DIFF=${LOGS}/${BENCH_NAME}-diff
export LOGS_QPS=${LOGS}/${BENCH_NAME}-qps
LOGS_CPU=$LOGS/cpu-states.txt

# check parameters
if [ $# -lt 3 ]; then usage "ERROR: Too little parameters passed"; fi
if [ ! -f $WORKLOAD_SCRIPT ]; then usage "ERROR: Workloads config file $WORKLOAD_SCRIPT not found."; fi

process_workload_config_file "$WORKLOAD_SCRIPT"
print_parameters ""

rm -rf ${LOGS}
mkdir -p ${LOGS} ${LOGS_AVG} ${LOGS_STDDEV} ${LOGS_DIFF} ${LOGS_QPS} ${RESULTS_DIR}
cd $WORKSPACE

if [ ! -x $BUILD_PATH/bin/mysqld ]; then usage "ERROR: Executable $BUILD_PATH/bin/mysqld not found."; fi
MYSQL_VERSION=`$BUILD_PATH/bin/mysqld --version | awk '{ print $3}'`
MYSQL_NAME=`$BUILD_PATH/bin/mysqld --help | grep Percona`
if [[ $MYSQL_NAME == *"Percona"* ]]; then MYSQL_NAME=PS; else MYSQL_NAME=MS; fi
export MYSQL_NAME="${MYSQL_NAME}"
export MYSQL_VERSION="${MYSQL_VERSION//./}"

export INNODB_CACHE=${INNODB_CACHE:-32G}
export NUM_TABLES=${NUM_TABLES:-16}
export DATASIZE=${DATASIZE:-10M}
export BENCH_ID=$(uname -n)_${MYSQL_NAME}${MYSQL_VERSION}_${RUN_TIME_SECONDS}sec_${NUM_TABLES}x${DATASIZE}-${INNODB_CACHE}

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

# "exit" calls on_exit()
exit 0
