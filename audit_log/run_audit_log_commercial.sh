SCRIPT_DIR=/data/pstress/pstress/scripts
AUDIT_DIR=/data/db-bench/audit_log
DATA_DIR=/mnt/black/pstress-run/audit_log_filter/data_alf_84

BASEDIR=/data/mysql-server/mysql-8.4.7-commercial
#COMMON_SQL=common_single_table.sql
#COMMON_SQL=udf_audit_log_format.sql
COMMON_SQL=check_table_format.sql

$SCRIPT_DIR/mysql_option_tester.py \
   --basedir $BASEDIR \
   --datadir $DATA_DIR \
   --sql $AUDIT_DIR/audit_log_commercial_install.sql  \
   --sql $AUDIT_DIR/common_filters.sql \
   --sql $AUDIT_DIR/$COMMON_SQL \
   --opt-file $AUDIT_DIR/audit_log.opt
