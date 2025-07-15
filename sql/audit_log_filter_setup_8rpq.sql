SELECT audit_log_filter_remove_filter('log_all');
SELECT audit_log_filter_remove_user('%');

-- 8 records/query
SELECT audit_log_filter_set_filter('log_all', '{"filter": { "log": true } }');

-- 5 records/query
-- SELECT audit_log_filter_set_filter('log_all', '{"filter": { "class": [ { "name": "general" }, { "name": "query" } ] } }');

-- 3 records/query
-- SELECT audit_log_filter_set_filter('log_all', '{"filter": { "class": [ { "name": "general" } ] } }');

-- 1 record/query
-- SELECT audit_log_filter_set_filter('log_all', '{"filter": { "class": [ { "name": "general", "event": {"name": "log"}, "general_data": { "sql_command": "select" } } ] } }');

SELECT audit_log_filter_set_user('%', 'log_all');

SELECT PLUGIN_NAME, PLUGIN_STATUS FROM INFORMATION_SCHEMA.PLUGINS WHERE PLUGIN_NAME LIKE 'audit%';
SHOW GLOBAL VARIABLES LIKE 'audit%';
