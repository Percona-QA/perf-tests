#!/bin/echo This script should be sourced in a shell, not executed directly

#**********************************************************************************************
# Percona Database Benchmark scripts
# mysql.inc - mysql related functions
#**********************************************************************************************

function mysql_call_cmake() {
    if [ $# -lt 2 ]; then echo "Usage: call_cmake <REPO_DIR> <BUILD_DIR>"; return 1; fi
    local REPO_DIR=$1
    local BUILD_DIR=$2
    local BUILD_TYPE="RelWithDebInfo"
    local BOOST_DIR="../../_deps"
    echo "SELECTED_CC=$SELECTED_CC (`which $SELECTED_CC`) SELECTED_CXX=$SELECTED_CXX (`which $SELECTED_CXX`) BUILD_TYPE=$BUILD_TYPE"

    local PACKAGES_LIBS="libtirpc-dev libgflags-dev libxml-simple-perl libeatmydata1 libfido2-dev libicu-dev libevent-dev libudev-dev libaio-dev libmecab-dev libnuma-dev liblz4-dev libzstd-dev libedit-dev libpam-dev libssl-dev libcurl4-openssl-dev libldap2-dev libkrb5-dev libsasl2-dev libsasl2-modules-gssapi-mit"
    local PACKAGES_PROTOBUF="protobuf-compiler libprotobuf-dev libprotoc-dev"
    sudo apt -yq --no-install-suggests --no-install-recommends --allow-unauthenticated install $PACKAGES_LIBS $PACKAGES_PROTOBUF

    rm -rf $BUILD_DIR
    mkdir -p $BUILD_DIR
    pushd $BUILD_DIR
    OPTIONS_DEBUG="-DCMAKE_C_FLAGS_DEBUG=-g1 -DCMAKE_CXX_FLAGS_DEBUG=-g1"
    OPTIONS_BUILD="-DMYSQL_MAINTAINER_MODE=OFF -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_CONFIG=mysql_release -DWITH_PACKAGE_FLAGS=OFF -DDOWNLOAD_BOOST=1 -DWITH_BOOST=$BOOST_DIR"
    OPTIONS_COMPILER="-DCMAKE_C_COMPILER=$SELECTED_CC -DCMAKE_CXX_COMPILER=$SELECTED_CXX -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache"
    OPTIONS_COMPONENTS="-DWITH_ROCKSDB=ON -DWITH_COREDUMPER=ON -DWITH_COMPONENT_KEYRING_VAULT=ON -DWITH_PAM=ON -DWITH_NUMA=ON"
    OPTIONS_LIBS="-DWITH_MECAB=system -DWITH_SYSTEM_LIBS=ON -DWITH_EDITLINE=system -DWITH_ZLIB=bundled -DWITH_LZ4=bundled"
    OPTIONS_OPTIMIZED="-DWITH_ROUTER=OFF -DWITH_UNIT_TESTS=OFF"
    SELECTED_OPTIONS="$OPTIONS_DEBUG $OPTIONS_BUILD $OPTIONS_COMPILER $OPTIONS_COMPONENTS $OPTIONS_LIBS $OPTIONS_OPTIMIZED"
    echo "SELECTED_OPTIONS=$SELECTED_OPTIONS"
    cmake $REPO_DIR $SELECTED_OPTIONS
    cmake -L .
    popd
}

function build_mysql() {
    if [ $# -lt 1 ]; then echo "Usage: build_ps <BUILD_DIR>"; return 1; fi
    local BUILD_DIR=$1

    echo "SELECTED_CC=$SELECTED_CC (`which $SELECTED_CC`) SELECTED_CXX=$SELECTED_CXX (`which $SELECTED_CXX`) BUILD_TYPE=$BUILD_TYPE"
    pushd $BUILD_DIR
    NPROC=`nproc --all`
    echo "Using $NPROC threads for compilation"
    make -j${NPROC}
    if [[ $? != 0 ]]; then echo "make -j${NPROC} failed"; exit -1; fi
    ccache --show-stats
    df -Th
    popd
}

function get_mysql_build_info() {
  SERVER_VERSION_LONG=`${MYSQLD_BIN} --version`
  SERVER_VERSION=`echo ${SERVER_VERSION_LONG} | awk '{ print $3}'`
  SERVER_NAME=`${MYSQLD_BIN} --help | grep Percona`
  if [[ $SERVER_NAME == *"Percona"* ]]; then SERVER_NAME=PS; else SERVER_NAME=MS; fi
  SERVER_VERSION="${SERVER_VERSION//./}"
  SERVER_TYPE="mysql"
}

# start_mysqld $MORE_PARAMS
function start_mysqld() {
  if [ "$ENGINE" == "rocksdb" ]; then
      EXTRA_PARAMS="--rocksdb_block_cache_size=${ENGINE_CACHE}"
  else
      EXTRA_PARAMS="--innodb_buffer_pool_size=${ENGINE_CACHE}"
  fi
  EXTRA_PARAMS+=" --user=root $PERF_EXTRA $MYEXTRA $1"
  RBASE="$(( RBASE + 100 ))"
  local MYSQLD_OPTIONS="--defaults-file=${CONFIG_FILE} --basedir=${BUILD_PATH} $EXTRA_PARAMS --log-error=$LOG_NAME_SERVERD --socket=$MYSQL_SOCKET --port=$RBASE"
  if [[ "$SSL_CERTS_PATH" != "" ]]; then
    MYSQLD_OPTIONS+=" --ssl-ca=$SSL_CERTS_PATH/ca-cert.pem --ssl-cert=$SSL_CERTS_PATH/server-cert.pem --ssl-key=$SSL_CERTS_PATH/server-key.pem --require_secure_transport=ON"
  fi
  echo "Starting Percona Server with options $MYSQLD_OPTIONS" | tee -a $LOG_NAME_SERVERD
  ${TASKSET_MYSQLD} ${MYSQLD_BIN} $MYSQLD_OPTIONS >> $LOG_NAME_SERVERD 2>&1 &

  echo "- Waiting for start of mysqld"
  for X in $(seq 0 ${PS_START_TIMEOUT}); do
    sleep 1
    echo -n "."
    if ${MYSQLADMIN_BIN} -uroot -S$MYSQL_SOCKET ping > /dev/null 2>&1; then
      echo "Percona Server started in $(( X+1 )) seconds (socket=$MYSQL_SOCKET port=$RBASE)"
      break
    fi
  done
  ${MYSQLADMIN_BIN} -uroot -S$MYSQL_SOCKET ping > /dev/null 2>&1 || { cat $LOG_NAME_SERVERD; echo "Couldn't connect $MYSQL_SOCKET" && exit 0; }
  ${MYSQLCLIENT_BIN} -uroot -S$MYSQL_SOCKET -e "SHOW GLOBAL VARIABLES LIKE 'tls_version'; SHOW VARIABLES LIKE 'require_secure_transport'" 2>&1
}

# shutdown_mysqld
function shutdown_mysqld() {
  if [[ "$SERVER_VERSION" < "81" ]]; then
    ${MYSQLCLIENT_BIN} -uroot -S$MYSQL_SOCKET -e "SHOW BINARY LOGS; RESET MASTER" 2>&1
  else
    ${MYSQLCLIENT_BIN} -uroot -S$MYSQL_SOCKET -e "SHOW BINARY LOGS; RESET BINARY LOGS AND GTIDS" 2>&1
  fi

  if [ "$ENGINE" == "rocksdb" ]; then
    ${MYSQLCLIENT_BIN} -uroot -S$MYSQL_SOCKET -e "set global rocksdb_force_flush_memtable_and_lzero_now=0" 2>&1
  fi

  ${MYSQLCLIENT_BIN} -uroot -S$MYSQL_SOCKET -e "USE $MYSQL_DATABASE; SELECT COUNT(*) FROM sbtest1" 2>&1

  echo "Shutting mysqld down"
  (time ${MYSQLADMIN_BIN} -uroot --socket=$MYSQL_SOCKET shutdown) 2>&1
  print_database_size
}

function mysql_prepare_datadir() {
  local TEMPLATE_DIR=${TEMPLATE_PATH}/${ENGINE}_${SERVER_VERSION%-*}_${NUM_TABLES}x${DATASIZE}

  if [[ ${WRITES_TIME_SECONDS} == 0 ]]; then return; fi

  if [ -d ${SNAPSHOT_DIR} ]; then copy_datadir "${SNAPSHOT_DIR}" "${DATA_DIR}"; return; fi

  if [ ! -d ${TEMPLATE_DIR} ]; then
    echo "Creating template data directory in ${TEMPLATE_DIR}"
    mkdir -p ${TEMPLATE_PATH} > /dev/null 2>&1
    ${TASKSET_MYSQLD} ${MYSQLD_BIN} --no-defaults --initialize-insecure --basedir=${BUILD_PATH} --datadir=${TEMPLATE_DIR} 2>&1

    LOG_NAME_SERVERD=${LOGS_CONFIG}/prepare.mysqld
    if [ "$ENGINE" == "rocksdb" ]; then
        EXTRA_PARAMS="--rocksdb_bulk_load=1"
    else
        EXTRA_PARAMS="--innodb_flush_log_at_trx_commit=0"
    fi
    start_mysqld "--datadir=${TEMPLATE_DIR} --disable-log-bin $EXTRA_PARAMS --innodb_fast_shutdown=0"
    ${MYSQLCLIENT_BIN} -uroot -S$MYSQL_SOCKET -e "CREATE DATABASE IF NOT EXISTS $MYSQL_DATABASE" 2>&1
    pushd $SYSBENCH_LUA
    # --rocksdb_bulk_load_sync_size  # Number of rows to sync bulk load
    (time ${TASKSET_SYSBENCH} $SYSBENCH_BIN $SYSBENCH_WRITE --threads=$NUM_TABLES --rand-seed=$RAND_SEED $SYSBENCH_OPTIONS --mysql-socket=$MYSQL_SOCKET prepare) 2>&1
    popd
    echo "Data directory in ${TEMPLATE_DIR} created"
    shutdown_mysqld
  fi

  copy_datadir "${TEMPLATE_DIR}" "${DATA_DIR}"
}
