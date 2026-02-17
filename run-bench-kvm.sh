#!/bin/bash
# Usage: ./run-bench-kvm.sh <large|medium|small>
#   large  - 64 vCPUs + 128 GB RAM (InnoDB 96G, RocksDB 16G)
#   medium - 32 vCPUs +  64 GB RAM (InnoDB 48G, RocksDB  8G)
#   small  - 16 vCPUs +  32 GB RAM (InnoDB 24G, RocksDB  4G)

case "${1}" in
    large)
        INNODB_CACHE=96G
        ROCKSDB_CACHE=16G
        ;;
    medium)
        INNODB_CACHE=48G
        ROCKSDB_CACHE=8G
        ;;
    small)
        INNODB_CACHE=24G
        ROCKSDB_CACHE=4G
        ;;
    *)
        echo "Usage: $0 <large|medium|small>"
        echo "  large  - 64 vCPUs + 128 GB RAM (InnoDB 96G, RocksDB 16G)"
        echo "  medium - 32 vCPUs +  64 GB RAM (InnoDB 48G, RocksDB  8G)"
        echo "  small  - 16 vCPUs +  32 GB RAM (InnoDB 24G, RocksDB  4G)"
        exit 1
        ;;
esac

# Common parameters
export SERVER_REPO_URL=https://github.com/percona/percona-server
export SERVER_BRANCH=release-8.4.7-7
export WORKLOAD_NAMES=prod_trx
export RESULTS_EMAIL=przemyslaw.skibinski@percona.com
export THREADS_LIST="8 16 32 64"
export WRITES_TIME_SECONDS=1800
export NUM_TABLES=32
export DATASIZE=20M
export SELECTED_CC=gcc-14
export SELECTED_CXX=g++-14
export ROOT_DIR=/work/bench-mysql
export TEMPLATE_PATH=/data/template_datadir

# InnoDB benchmark
export ENGINE=innodb
export ENGINE_CACHE=$INNODB_CACHE
export CNFFILE_NAME=cnf/innodb-ACID-slowIO.cnf
echo "=== KVM ${1}: InnoDB ENGINE_CACHE=$ENGINE_CACHE ==="
nice --adjustment=-10 bash -xe ./build-and-bench.sh

# RocksDB benchmark
export ENGINE=rocksdb
export ENGINE_CACHE=$ROCKSDB_CACHE
export CNFFILE_NAME=cnf/rocksdb-ACID.cnf
echo "=== KVM ${1}: RocksDB ENGINE_CACHE=$ENGINE_CACHE ==="
nice --adjustment=-10 bash -xe ./build-and-bench.sh
