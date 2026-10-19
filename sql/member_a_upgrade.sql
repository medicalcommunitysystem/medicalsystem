-- Apply to the selected project database during a maintenance window.
-- Additive migration: existing business records are never deleted.
-- Existing duplicate usernames/effective bookings deliberately cause an error.
SET NAMES utf8mb4;
CREATE TABLE IF NOT EXISTS t_member_a_schema (
    version INT NOT NULL PRIMARY KEY
) ENGINE=InnoDB;
-- A failed/partial migration must not leave a successful readiness marker.
DELETE FROM t_member_a_schema WHERE version=1;
CREATE TABLE IF NOT EXISTS t_auth_account (
    username VARCHAR(50) NOT NULL,
    role TINYINT NOT NULL,
    user_id VARCHAR(20) NOT NULL,
    status TINYINT NOT NULL DEFAULT 1,
    failed_attempts INT NOT NULL DEFAULT 0,
    locked_until DATETIME DEFAULT NULL,
    PRIMARY KEY (username),
    UNIQUE KEY uq_auth_identity (role,user_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci;

INSERT INTO t_auth_account(username,role,user_id,status)
SELECT username,1,patient_id,COALESCE(status,0) FROM t_patient p
WHERE NOT EXISTS(SELECT 1 FROM t_auth_account a
    WHERE a.role=1 AND a.user_id=p.patient_id AND a.username=p.username)
UNION ALL
SELECT username,2,doctor_id,COALESCE(status,0) FROM t_doctor d
WHERE NOT EXISTS(SELECT 1 FROM t_auth_account a
    WHERE a.role=2 AND a.user_id=d.doctor_id AND a.username=d.username)
UNION ALL
SELECT username,3,admin_id,1 FROM t_admin m
WHERE NOT EXISTS(SELECT 1 FROM t_auth_account a
    WHERE a.role=3 AND a.user_id=m.admin_id AND a.username=m.username);

CREATE TABLE IF NOT EXISTS t_member_a_sequence (
    sequence_name VARCHAR(32) NOT NULL PRIMARY KEY,
    next_value INT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB;
INSERT IGNORE INTO t_member_a_sequence VALUES('patient',0);
UPDATE t_member_a_sequence SET next_value=GREATEST(next_value,
    COALESCE((SELECT MAX(CAST(RIGHT(patient_id,8) AS UNSIGNED))
      FROM t_patient WHERE patient_id REGEXP '^P[0-9]{12}$'),0))
WHERE sequence_name='patient';

-- Cancelled rows have NULL keys, so booking again is allowed.
SET @member_a_ddl=IF(EXISTS(SELECT 1 FROM information_schema.columns
    WHERE table_schema=DATABASE() AND table_name='t_appointment'
      AND column_name='effective_booking'),'DO 0',
    'ALTER TABLE t_appointment ADD COLUMN effective_booking TINYINT GENERATED ALWAYS AS (CASE WHEN status IN (0,1) THEN 1 ELSE NULL END) STORED');
PREPARE member_a_statement FROM @member_a_ddl;
EXECUTE member_a_statement;
DEALLOCATE PREPARE member_a_statement;
SET @member_a_ddl=IF(EXISTS(SELECT 1 FROM information_schema.statistics
    WHERE table_schema=DATABASE() AND table_name='t_appointment'
      AND index_name='uq_appointment_effective'),'DO 0',
    'ALTER TABLE t_appointment ADD UNIQUE KEY uq_appointment_effective(patient_id,doctor_id,appoint_date,effective_booking)');
PREPARE member_a_statement FROM @member_a_ddl;
EXECUTE member_a_statement;
DEALLOCATE PREPARE member_a_statement;

-- Only migration-owned triggers are replaced; all writes use the caller's transaction.
DROP TRIGGER IF EXISTS member_a_patient_insert;
DROP TRIGGER IF EXISTS member_a_patient_update;
DROP TRIGGER IF EXISTS member_a_patient_delete;
DROP TRIGGER IF EXISTS member_a_doctor_insert;
DROP TRIGGER IF EXISTS member_a_doctor_update;
DROP TRIGGER IF EXISTS member_a_doctor_delete;
DROP TRIGGER IF EXISTS member_a_admin_insert;
DROP TRIGGER IF EXISTS member_a_admin_update;
DROP TRIGGER IF EXISTS member_a_admin_delete;
DELIMITER $$
CREATE TRIGGER member_a_patient_insert AFTER INSERT ON t_patient FOR EACH ROW
BEGIN
    INSERT INTO t_auth_account(username,role,user_id,status)
    VALUES(NEW.username,1,NEW.patient_id,COALESCE(NEW.status,0));
END$$
CREATE TRIGGER member_a_patient_update AFTER UPDATE ON t_patient FOR EACH ROW
BEGIN
    UPDATE t_auth_account SET username=NEW.username,user_id=NEW.patient_id,
      status=COALESCE(NEW.status,0) WHERE role=1 AND user_id=OLD.patient_id;
END$$
CREATE TRIGGER member_a_patient_delete AFTER DELETE ON t_patient FOR EACH ROW
BEGIN
    DELETE FROM t_auth_account WHERE role=1 AND user_id=OLD.patient_id;
END$$
CREATE TRIGGER member_a_doctor_insert AFTER INSERT ON t_doctor FOR EACH ROW
BEGIN
    INSERT INTO t_auth_account(username,role,user_id,status)
    VALUES(NEW.username,2,NEW.doctor_id,COALESCE(NEW.status,0));
END$$
CREATE TRIGGER member_a_doctor_update AFTER UPDATE ON t_doctor FOR EACH ROW
BEGIN
    UPDATE t_auth_account SET username=NEW.username,user_id=NEW.doctor_id,
      status=COALESCE(NEW.status,0) WHERE role=2 AND user_id=OLD.doctor_id;
END$$
CREATE TRIGGER member_a_doctor_delete AFTER DELETE ON t_doctor FOR EACH ROW
BEGIN
    DELETE FROM t_auth_account WHERE role=2 AND user_id=OLD.doctor_id;
END$$
CREATE TRIGGER member_a_admin_insert AFTER INSERT ON t_admin FOR EACH ROW
BEGIN
    INSERT INTO t_auth_account(username,role,user_id,status)
    VALUES(NEW.username,3,NEW.admin_id,1);
END$$
CREATE TRIGGER member_a_admin_update AFTER UPDATE ON t_admin FOR EACH ROW
BEGIN
    UPDATE t_auth_account SET username=NEW.username,user_id=NEW.admin_id
    WHERE role=3 AND user_id=OLD.admin_id;
END$$
CREATE TRIGGER member_a_admin_delete AFTER DELETE ON t_admin FOR EACH ROW
BEGIN
    DELETE FROM t_auth_account WHERE role=3 AND user_id=OLD.admin_id;
END$$
DELIMITER ;
-- Validate actual postconditions as well: GUI clients may continue after an error.
INSERT INTO t_member_a_schema(version)
SELECT IF(
    (SELECT COUNT(*) FROM information_schema.triggers
     WHERE trigger_schema=DATABASE() AND trigger_name IN
       ('member_a_patient_insert','member_a_patient_update','member_a_patient_delete',
        'member_a_doctor_insert','member_a_doctor_update','member_a_doctor_delete',
        'member_a_admin_insert','member_a_admin_update','member_a_admin_delete'))=9
    AND EXISTS(SELECT 1 FROM information_schema.columns
        WHERE table_schema=DATABASE() AND table_name='t_appointment'
          AND column_name='effective_booking' AND extra LIKE '%STORED GENERATED%')
    AND (SELECT COUNT(*) FROM information_schema.statistics
        WHERE table_schema=DATABASE() AND table_name='t_appointment'
          AND index_name='uq_appointment_effective' AND non_unique=0
          AND ((seq_in_index=1 AND column_name='patient_id')
            OR (seq_in_index=2 AND column_name='doctor_id')
            OR (seq_in_index=3 AND column_name='appoint_date')
            OR (seq_in_index=4 AND column_name='effective_booking')))=4
    AND EXISTS(SELECT 1 FROM t_member_a_sequence WHERE sequence_name='patient')
    AND NOT EXISTS(
        SELECT 1 FROM (
            SELECT patient_id AS user_id,username,1 AS role FROM t_patient
            UNION ALL SELECT doctor_id,username,2 FROM t_doctor
            UNION ALL SELECT admin_id,username,3 FROM t_admin
        ) source_accounts LEFT JOIN t_auth_account a
          ON a.role=source_accounts.role AND a.user_id=source_accounts.user_id
             AND a.username=source_accounts.username
        WHERE a.user_id IS NULL
    ),1,NULL);
