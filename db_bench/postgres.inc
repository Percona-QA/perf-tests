#!/bin/echo This script should be sourced in a shell, not executed directly

#**********************************************************************************************
# Percona Database Benchmark scripts
# postgres.inc - postgres related functions
#**********************************************************************************************

function build_postgres() {
    if [ $# -lt 2 ]; then echo "Usage: build_postgres <REPO_DIR> <BIN_DIR>"; return 1; fi
    local REPO_DIR=$1
    local BIN_DIR=$2

    sudo apt -y install build-essential libreadline-dev zlib1g-dev flex bison

    pushd $REPO_DIR
    ./configure --prefix=$BIN_DIR
    echo "SELECTED_CC=$SELECTED_CC (`which $SELECTED_CC`) SELECTED_CXX=$SELECTED_CXX (`which $SELECTED_CXX`) BUILD_TYPE=$BUILD_TYPE"
    NPROC=`nproc --all`
    echo "Using $NPROC threads for compilation"
    make -j${NPROC}
    if [[ $? != 0 ]]; then echo "make -j${NPROC} failed"; exit -1; fi
    sudo make install
    ccache --show-stats
    df -Th
    popd
}

function get_postgres_build_info() {
  SERVER_VERSION_LONG=`${PSQL_BIN} --version`
  SERVER_VERSION=`echo ${SERVER_VERSION_LONG} | awk '{ print $3}'`
  if [[ $SERVER_VERSION_LONG == *"Percona"* ]]; then SERVER_NAME=Percona; else SERVER_NAME=Postgres; fi
  SERVER_VERSION="${SERVER_VERSION//./}"
  SERVER_TYPE="postgres"
}

# start_postgres <EXTRA_PARAMS>
function start_postgres() {
  PG_PARAMS="$PERF_EXTRA $MYEXTRA $1 -l $LOG_NAME_SERVERD"
  RBASE="$(( RBASE + 100 ))"
#  if [[ "$SSL_CERTS_PATH" != "" ]]; then
#    MYSQLD_OPTIONS+=" --ssl-ca=$SSL_CERTS_PATH/ca-cert.pem --ssl-cert=$SSL_CERTS_PATH/server-cert.pem --ssl-key=$SSL_CERTS_PATH/server-key.pem --require_secure_transport=ON"
#  fi
  echo "Starting Postgres with options $PG_PARAMS" | tee -a $LOG_NAME_SERVERD
  ${PG_CTL_BIN} $PG_PARAMS -o "-c config_file=${CONFIG_FILE} -c shared_buffers=${ENGINE_CACHE}B" start 2>&1

  echo "Waiting up to ${PS_START_TIMEOUT} seconds for PostgreSQL to start..."

  timeout=${PS_START_TIMEOUT}
  interval=2
  elapsed=0

  while ! ${PG_CTL_BIN} $PG_PARAMS status | grep -q "server is running"; do
      if [ "$elapsed" -ge "$timeout" ]; then
          echo "Error: PostgreSQL did not start within ${PS_START_TIMEOUT} seconds."
          exit 1
      fi
      sleep $interval
      ((elapsed+=interval))
  done

  echo "PostgreSQL is up and running!"
  ${PSQL_BIN} -U $DB_USER -d postgres -c "ALTER SYSTEM SET shared_buffers = '${ENGINE_CACHE}B';"
  ${PSQL_BIN} -U $DB_USER -d postgres -c "SELECT pg_reload_conf();"
  ${PSQL_BIN} -U $DB_USER -d postgres -c "
    SHOW ssl;
    SHOW ssl_min_protocol_version;
    SHOW ssl_max_protocol_version;
    SHOW shared_buffers;
    SHOW work_mem;
    SHOW maintenance_work_mem;
  "
}

# shutdown_postgres <DATA_DIR>
function shutdown_postgres() {
  local PG_DATA_DIR=$1
  ${PSQL_BIN} -U $DB_USER -d postgres -c "SELECT * FROM pg_ls_waldir();" 2>&1
  ${PSQL_BIN} -U $DB_USER -d postgres -c "SELECT pg_switch_wal();" 2>&1
  ${PSQL_BIN} -U $DB_USER -d $DB_NAME -c "SELECT COUNT(*) FROM sbtest1;" 2>&1

  timeout=${PS_START_TIMEOUT:-60}  # Default to 60 seconds if variable is unset
  interval=2  # Check every 2 seconds
  elapsed=0

  echo "Stopping PostgreSQL..."
  ${PG_CTL_BIN} -D ${PG_DATA_DIR} stop 2>&1
#  while ${PG_CTL_BIN} -D ${PG_DATA_DIR} status >/dev/null 2>&1; do
  while ${PG_CTL_BIN} -D ${PG_DATA_DIR} status; do
      if [[ $elapsed -ge $timeout ]]; then
          echo "Timeout reached: PostgreSQL did not stop within $timeout seconds."
          exit 1
      fi
      echo "Waiting for PostgreSQL to stop... $elapsed/$timeout"
      sleep $interval
      ((elapsed+=interval))
  done

  print_database_size
}

function postgres_prepare_datadir() {
  local TEMPLATE_DIR=${TEMPLATE_PATH}/${ENGINE}_${SERVER_VERSION%-*}_${NUM_TABLES}x${DATASIZE}

  if [[ ${WRITES_TIME_SECONDS} == 0 ]]; then return; fi

  if [ -d ${SNAPSHOT_DIR} ]; then copy_datadir "${SNAPSHOT_DIR}" "${DATA_DIR}"; return; fi

  if [ ! -d ${TEMPLATE_DIR} ]; then
    echo "Creating template data directory in ${TEMPLATE_DIR}"
    sudo mkdir -p ${TEMPLATE_PATH} > /dev/null 2>&1
    sudo chown $USER:$USER ${TEMPLATE_PATH}
    ${INITDB_BIN} -D ${TEMPLATE_DIR} # 2>&1

    LOG_NAME_SERVERD=${LOGS_CONFIG}/prepare.${SERVER_TYPE}d
    # EXTRA_PARAMS="--innodb_flush_log_at_trx_commit=0"
    # "--datadir=${TEMPLATE_DIR} --disable-log-bin $EXTRA_PARAMS --innodb_fast_shutdown=0"
    start_postgres "-D ${TEMPLATE_DIR}"
    ${PSQL_BIN} -U $DB_USER -d postgres -c "CREATE DATABASE $DB_NAME;" 2>&1

    pushd $SYSBENCH_LUA
    (time $SYSBENCH_BIN $SYSBENCH_WRITE --threads=$NUM_TABLES --rand-seed=$RAND_SEED $SYSBENCH_OPTIONS prepare) 2>&1
    popd
    echo "Data directory in ${TEMPLATE_DIR} created"
    shutdown_postgres "${TEMPLATE_DIR}"
  fi

  copy_datadir "${TEMPLATE_DIR}" "${DATA_DIR}"
}
