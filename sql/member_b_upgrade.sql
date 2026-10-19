-- 在已导入团队 init_db.sql 的数据库中执行：
-- sudo mysql medical_system < sql/member_b_upgrade.sql
-- 不删除业务表，不清空数据；可重复执行。执行前自行备份正式数据。
-- MySQL DDL 会隐式提交；先检查重复数据，再逐项补充结构。
CREATE TABLE IF NOT EXISTS t_member_b_schema(version INT PRIMARY KEY) ENGINE=InnoDB;
DELETE FROM t_member_b_schema WHERE version=1;
DELIMITER $$
DROP PROCEDURE IF EXISTS member_b_upgrade$$
CREATE PROCEDURE member_b_upgrade()
BEGIN
    IF DATABASE() IS NULL THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Select a database before migration';
    END IF;
    IF EXISTS (SELECT 1 FROM t_visit_record WHERE appoint_id IS NOT NULL
               GROUP BY appoint_id HAVING COUNT(*)>1) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Duplicate visits per appointment; resolve manually';
    END IF;
    IF EXISTS (SELECT 1 FROM t_medical_record WHERE visit_id IS NOT NULL
               GROUP BY visit_id HAVING COUNT(*)>1) THEN
        SIGNAL SQLSTATE '45000' SET MESSAGE_TEXT='Duplicate medical records per visit; resolve manually';
    END IF;
    IF NOT EXISTS (SELECT 1 FROM information_schema.columns
                   WHERE table_schema=DATABASE() AND table_name='t_medical_record' AND column_name='create_time') THEN
        ALTER TABLE t_medical_record ADD COLUMN create_time DATETIME NULL;
    END IF;
    -- 旧记录没有可靠的创建时间，保守置为已过期，避免迁移后重新获得修改窗口。
    UPDATE t_medical_record SET create_time=DATE_SUB(NOW(),INTERVAL 25 HOUR),update_time=update_time
        WHERE create_time IS NULL;
    ALTER TABLE t_medical_record MODIFY create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP;
    IF NOT EXISTS (SELECT 1 FROM information_schema.statistics
                   WHERE table_schema=DATABASE() AND table_name='t_visit_record' AND index_name='uq_b_visit_appointment') THEN
        ALTER TABLE t_visit_record ADD UNIQUE KEY uq_b_visit_appointment(appoint_id);
    END IF;
    IF NOT EXISTS (SELECT 1 FROM information_schema.statistics
                   WHERE table_schema=DATABASE() AND table_name='t_medical_record' AND index_name='uq_b_record_visit') THEN
        ALTER TABLE t_medical_record ADD UNIQUE KEY uq_b_record_visit(visit_id);
    END IF;
    CREATE TABLE IF NOT EXISTS t_b_sequence (
        name VARCHAR(32) NOT NULL PRIMARY KEY,
        next_value BIGINT NOT NULL
    ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
    INSERT IGNORE INTO t_b_sequence(name,next_value) VALUES('doctor',1);
END$$
CALL member_b_upgrade()$$
DROP PROCEDURE member_b_upgrade$$
DELIMITER ;

INSERT INTO t_member_b_schema(version) VALUES(1);
