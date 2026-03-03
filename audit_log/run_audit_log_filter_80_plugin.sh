#!/bin/bash
set -o pipefail

SCRIPT_DIR=/data/pstress/pstress/scripts
AUDIT_DIR=/data/db-bench/audit_log
DATA_DIR=/mnt/black/pstress-run/audit_log_filter/data_alf_80

BASEDIR=/data/mysql-server/percona-8.0-deb-gcc14-rocks

#COMMON_SQL=common_single_table.sql
COMMON_SQL=udf_audit_log_format.sql 

$SCRIPT_DIR/mysql_option_tester.py \
   --basedir $BASEDIR \
   --datadir $DATA_DIR \
   --sql $AUDIT_DIR/audit_log_filter_80_plugin_install.sql  \
   --sql $AUDIT_DIR/common_filters.sql \
   --sql $AUDIT_DIR/$COMMON_SQL \
   --opt-file $AUDIT_DIR/audit_log.opt
#   --socket

#   --sh $SCRIPT_DIR/run_mysqlslap.sh \

#   --sql $AUDIT_DIR/test_common.sql \
#   --sql $AUDIT_DIR/udf_audit_log_format.sql \
