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
    cd contrib/pg_tde
    make -j${NPROC}
    if [[ $? != 0 ]]; then echo "pg_tde: make -j${NPROC} failed"; exit -1; fi
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

# start_postgres <EXTRA_PARAMS> <EXTRA_CONFIG_PARAMS>
function start_postgres() {
  PG_PARAMS="$PERF_EXTRA $MYEXTRA $1 -l $LOG_NAME_SERVERD"
  PG_CONF_PARAMS="-c config_file=${CONFIG_FILE} -c shared_buffers=${ENGINE_CACHE}B $2"
  RBASE="$(( RBASE + 100 ))"
#  if [[ "$SSL_CERTS_PATH" != "" ]]; then
#    MYSQLD_OPTIONS+=" --ssl-ca=$SSL_CERTS_PATH/ca-cert.pem --ssl-cert=$SSL_CERTS_PATH/server-cert.pem --ssl-key=$SSL_CERTS_PATH/server-key.pem --require_secure_transport=ON"
#  fi
  echo "Starting Postgres with options $PG_PARAMS -o \"$PG_CONF_PARAMS\"" | tee -a $LOG_NAME_SERVERD
  ${PG_CTL_BIN} start $PG_PARAMS -o "$PG_CONF_PARAMS" 2>&1
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
    SHOW synchronous_commit;
  "
}

# shutdown_postgres <DATA_DIR>
function shutdown_postgres() {
  local PG_DATA_DIR=$1
  #${PSQL_BIN} -U $DB_USER -d postgres -c "SELECT * FROM pg_ls_waldir();" 2>&1
  ${PSQL_BIN} -U $DB_USER -d postgres -c "SELECT pg_switch_wal();" 2>&1
  ${PSQL_BIN} -U $DB_USER -d $DB_NAME -c "SELECT COUNT(*) FROM sbtest1;" 2>&1
  if [[ "$PG_TDE" == "ON" ]]; then
    ${PSQL_BIN} -U $DB_USER -d $DB_NAME -c "SELECT tablename, pg_tde_is_encrypted(tablename::regclass)
                                            AS is_encrypted FROM pg_tables WHERE schemaname = 'public';" 2>&1
    ${PSQL_BIN} -U $DB_USER -d $DB_NAME -c "SELECT pg_tde_is_encrypted('sbtest1');" 2>&1
  fi

  timeout=${PS_START_TIMEOUT:-60}  # Default to 60 seconds if variable is unset
  interval=2  # Check every 2 seconds
  elapsed=0

  echo "Stopping PostgreSQL..."
  ${PG_CTL_BIN} stop -D ${PG_DATA_DIR} 2>&1
  while ${PG_CTL_BIN} status -D ${PG_DATA_DIR}; do
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
  PG_TDE=${PG_TDE:-OFF}
  local TEMPLATE_DIR=${TEMPLATE_PATH}/${ENGINE}_${SERVER_VERSION%-*}_${NUM_TABLES}x${DATASIZE}_TDE=${PG_TDE}

  if [[ ${WRITES_TIME_SECONDS} == 0 ]]; then return; fi

  if [ -d ${SNAPSHOT_DIR} ]; then copy_datadir "${SNAPSHOT_DIR}" "${DATA_DIR}"; return; fi

  if [ "${REBUILD_TEMPLATE}" == "ON" ] || [ ! -d ${TEMPLATE_DIR} ]; then
    echo "Creating template data directory in ${TEMPLATE_DIR}"
    sudo rm -rf ${TEMPLATE_PATH}
    sudo mkdir -p ${TEMPLATE_PATH} > /dev/null 2>&1
    sudo chown $USER:$USER ${TEMPLATE_PATH}

    LOG_NAME_SERVERD=${LOGS_CONFIG}/prepare.${SERVER_TYPE}d
    ${INITDB_BIN} -D ${TEMPLATE_DIR} 2>&1

    if [[ "$PG_TDE" == "ON" ]]; then
      #export TDE_MODE=1
      ${PG_CTL_BIN} start -D ${TEMPLATE_DIR} 2>&1
      ${PSQL_BIN} -U $DB_USER -d postgres -c "ALTER SYSTEM SET shared_preload_libraries = 'pg_tde';" 2>&1
      ${PG_CTL_BIN} restart -D ${TEMPLATE_DIR} 2>&1

      ${PSQL_BIN} -U $DB_USER -d template1 -c 'CREATE EXTENSION pg_tde;'
      ${PSQL_BIN} -U $DB_USER -d postgres -c "
            CREATE EXTENSION IF NOT EXISTS pg_tde;
            SELECT pg_tde_add_global_key_provider_file('reg_file-global', '$BENCH_DIR/pg_tde_test_keyring.per');
            SELECT pg_tde_set_default_principal_key_using_global_key_provider('server-principal-key', 'reg_file-global');
       "
      ${PSQL_BIN} -U $DB_USER -d postgres -c "ALTER SYSTEM SET pg_tde.wal_encrypt = on;"
      ${PSQL_BIN} -U $DB_USER -d postgres -c "ALTER SYSTEM SET default_table_access_method = 'tde_heap';"
      #${PSQL_BIN} -U $DB_USER -d postgres -c "ALTER SYSTEM SET search_path = $DB_USER,public,tde;"
      ${PG_CTL_BIN} stop -m smart -D ${TEMPLATE_DIR} 2>&1
    fi

    start_postgres "-D ${TEMPLATE_DIR}" "-c synchronous_commit=off"
    ${PSQL_BIN} -U $DB_USER -d postgres -c "CREATE DATABASE $DB_NAME;" 2>&1

    pushd $SYSBENCH_LUA
    (time $SYSBENCH_BIN $SYSBENCH_WRITE --threads=$NUM_TABLES --rand-seed=$RAND_SEED $SYSBENCH_OPTIONS prepare) 2>&1
    popd
    echo "Data directory in ${TEMPLATE_DIR} created"
    shutdown_postgres "${TEMPLATE_DIR}"
  fi

  copy_datadir "${TEMPLATE_DIR}" "${DATA_DIR}"
}

function set_postgres_sysbench_options() {
  local NUM_ROWS=$(numfmt --from=si $DATASIZE)
  SYSBENCH_OPTIONS="--table-size=$NUM_ROWS --tables=$NUM_TABLES --pgsql-db=$DB_NAME --pgsql-user=$DB_USER --report-interval=$SYSBENCH_REPORT_INTERVAL --db-driver=pgsql --db-ps-mode=disable --percentile=99 --rand-type=$RAND_TYPE $SYSBENCH_EXTRA"
  if [[ "$SSL_CERTS_PATH" != "" ]]; then
    SYSBENCH_OPTIONS+=" --pgsql-ssl=VERIFY_CA --pgsql-ssl-ca=$SSL_CERTS_PATH/ca-cert.pem --pgsql-ssl-cert=$SSL_CERTS_PATH/client-cert.pem --pgsql-ssl-key=$SSL_CERTS_PATH/client-key.pem"
  else
    export PGSSLMODE=disable
  fi
}

function postgres_before_shutdown() {
    ${PSQL_BIN} -U $DB_USER -d $DB_NAME -c "SHOW wal_sync_method;" >> ${LOG_NAME_SERVER} 2>&1
    ${PSQL_BIN} -U $DB_USER -d $DB_NAME -c "SELECT * FROM pg_stat_bgwriter;" >> ${LOG_NAME_SERVER} 2>&1
    ${PSQL_BIN} -U $DB_USER -d $DB_NAME -c "SELECT * FROM pg_stat_database WHERE datname = current_database();" >> ${LOG_NAME_SERVER} 2>&1
    ${PSQL_BIN} -U $DB_USER -d $DB_NAME -c "SELECT * FROM pg_stat_activity;" >> ${LOG_NAME_SERVER} 2>&1
}

function check_postgres_binaries() {
  export PSQL_BIN=$BUILD_PATH/psql
  if [ ! -x ${PSQL_BIN} ]; then usage "ERROR: Executable ${PSQL_BIN} not found."; fi

  export INITDB_BIN=${BUILD_PATH}/initdb
  if [ ! -x ${INITDB_BIN} ]; then usage "ERROR: Executable ${INITDB_BIN} not found."; fi

  export PG_CTL_BIN=${BUILD_PATH}/pg_ctl
  if [ ! -x ${PG_CTL_BIN} ]; then usage "ERROR: Executable ${PG_CTL_BIN} not found."; fi

  echo PSQL_BIN=${PSQL_BIN} INITDB_BIN=${INITDB_BIN} PG_CTL_BIN=${PG_CTL_BIN}
}

function postgres_killall() {
  timeout --signal=9 30s ${PG_CTL_BIN} stop -D "${DATA_DIR}" -m fast 2>&1
  kill -9 $(pgrep -f ${DATA_DIR}) 2>/dev/null
  killall -9 postgres
}

function sysbench_postgres_options() {
    ALL_SYSBENCH_OPTIONS="$WORKLOAD_PARAMETERS --threads=$num_threads --time=$SYSBENCH_RUN_TIME --warmup-time=$WARMUP_TIME_SECONDS --rand-seed=$(( RAND_SEED + num_threads*num_threads )) $SYSBENCH_OPTIONS "
    if [[ ${SYSBENCH_HOST} != "" ]]; then
      ALL_SYSBENCH_OPTIONS+="--pgsql-host=$SYSBENCH_HOST --pgsql-port=$RBASE"
    fi
}
