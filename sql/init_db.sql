-- MySQL dump 10.13  Distrib 8.0.46, for Linux (x86_64)
--
-- Host: localhost    Database: medical_system
-- ------------------------------------------------------
-- Server version	8.0.46-0ubuntu0.24.04.4

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!50503 SET NAMES utf8mb4 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Current Database: `medical_system`
--

CREATE DATABASE /*!32312 IF NOT EXISTS*/ `medical_system` /*!40100 DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci */ /*!80016 DEFAULT ENCRYPTION='N' */;

USE `medical_system`;

--
-- Table structure for table `t_admin`
--

DROP TABLE IF EXISTS `t_admin`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_admin` (
  `admin_id` varchar(10) NOT NULL COMMENT '管理员编号',
  `name` varchar(50) NOT NULL COMMENT '姓名',
  `username` varchar(50) NOT NULL COMMENT '登录账号',
  `password` varchar(100) NOT NULL COMMENT '登录密码',
  `phone` varchar(20) DEFAULT NULL COMMENT '联系电话',
  PRIMARY KEY (`admin_id`),
  UNIQUE KEY `username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='管理员表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_admin`
--

LOCK TABLES `t_admin` WRITE;
/*!40000 ALTER TABLE `t_admin` DISABLE KEYS */;
INSERT INTO `t_admin` VALUES ('A001','系统管理员','admin','123456','13800000000');
/*!40000 ALTER TABLE `t_admin` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `t_appointment`
--

DROP TABLE IF EXISTS `t_appointment`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_appointment` (
  `appoint_id` int NOT NULL AUTO_INCREMENT COMMENT '预约ID',
  `patient_id` varchar(20) NOT NULL COMMENT '患者编号',
  `doctor_id` varchar(10) NOT NULL COMMENT '医生编号',
  `appoint_date` date DEFAULT NULL COMMENT '预约日期',
  `time_slot` varchar(10) DEFAULT NULL COMMENT '时段',
  `symptom` varchar(200) DEFAULT NULL COMMENT '症状描述',
  `status` tinyint DEFAULT '0' COMMENT '0待就诊 1已就诊 2已取消',
  `create_time` datetime DEFAULT CURRENT_TIMESTAMP COMMENT '挂号时间',
  PRIMARY KEY (`appoint_id`),
  KEY `idx_appoint_doctor` (`doctor_id`,`appoint_date`,`status`),
  KEY `idx_appoint_patient` (`patient_id`,`status`),
  CONSTRAINT `fk_appoint_doctor` FOREIGN KEY (`doctor_id`) REFERENCES `t_doctor` (`doctor_id`),
  CONSTRAINT `fk_appoint_patient` FOREIGN KEY (`patient_id`) REFERENCES `t_patient` (`patient_id`)
) ENGINE=InnoDB AUTO_INCREMENT=2 DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='预约表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_appointment`
--

LOCK TABLES `t_appointment` WRITE;
/*!40000 ALTER TABLE `t_appointment` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_appointment` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `t_doctor`
--

DROP TABLE IF EXISTS `t_doctor`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_doctor` (
  `doctor_id` varchar(10) NOT NULL COMMENT '医生编号 D001',
  `name` varchar(50) NOT NULL COMMENT '姓名',
  `gender` varchar(4) DEFAULT NULL COMMENT '性别',
  `title` varchar(20) DEFAULT NULL COMMENT '职称',
  `dept` varchar(50) DEFAULT NULL COMMENT '科室',
  `phone` varchar(20) DEFAULT NULL COMMENT '联系电话',
  `username` varchar(50) NOT NULL COMMENT '登录账号',
  `password` varchar(100) NOT NULL COMMENT '登录密码',
  `status` tinyint DEFAULT '1' COMMENT '1在职 0离职',
  PRIMARY KEY (`doctor_id`),
  UNIQUE KEY `username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='医生表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_doctor`
--

LOCK TABLES `t_doctor` WRITE;
/*!40000 ALTER TABLE `t_doctor` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_doctor` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `t_medical_record`
--

DROP TABLE IF EXISTS `t_medical_record`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_medical_record` (
  `record_id` int NOT NULL AUTO_INCREMENT COMMENT '病历ID',
  `patient_id` varchar(20) DEFAULT NULL COMMENT '患者编号',
  `doctor_id` varchar(10) DEFAULT NULL COMMENT '医生编号',
  `visit_id` int DEFAULT NULL COMMENT '就诊ID',
  `allergy_history` varchar(200) DEFAULT NULL COMMENT '过敏史',
  `past_history` varchar(200) DEFAULT NULL COMMENT '既往史',
  `family_history` varchar(200) DEFAULT NULL COMMENT '家族史',
  `update_time` datetime DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间',
  PRIMARY KEY (`record_id`),
  KEY `fk_record_doctor` (`doctor_id`),
  KEY `fk_record_visit` (`visit_id`),
  KEY `idx_record_patient` (`patient_id`),
  CONSTRAINT `fk_record_doctor` FOREIGN KEY (`doctor_id`) REFERENCES `t_doctor` (`doctor_id`),
  CONSTRAINT `fk_record_patient` FOREIGN KEY (`patient_id`) REFERENCES `t_patient` (`patient_id`),
  CONSTRAINT `fk_record_visit` FOREIGN KEY (`visit_id`) REFERENCES `t_visit_record` (`visit_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='病历表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_medical_record`
--

LOCK TABLES `t_medical_record` WRITE;
/*!40000 ALTER TABLE `t_medical_record` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_medical_record` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `t_medicine`
--

DROP TABLE IF EXISTS `t_medicine`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_medicine` (
  `med_id` varchar(20) NOT NULL COMMENT '药品编号',
  `med_name` varchar(50) NOT NULL COMMENT '药品名称',
  `spec` varchar(50) DEFAULT NULL COMMENT '规格',
  `unit` varchar(10) DEFAULT NULL COMMENT '单位',
  `price` decimal(10,2) NOT NULL DEFAULT '0.00' COMMENT '单价',
  `stock` int NOT NULL DEFAULT '0' COMMENT '库存',
  `manufacturer` varchar(50) DEFAULT NULL COMMENT '生产厂家',
  `status` tinyint DEFAULT '1' COMMENT '1有效 0停用',
  PRIMARY KEY (`med_id`),
  KEY `idx_medicine_name` (`med_name`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='药品表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_medicine`
--

LOCK TABLES `t_medicine` WRITE;
/*!40000 ALTER TABLE `t_medicine` DISABLE KEYS */;
INSERT INTO `t_medicine` VALUES ('M001','阿莫西林胶囊','0.25g*24粒','盒',12.50,500,'华北制药',1),('M002','布洛芬缓释胶囊','0.3g*20粒','盒',18.80,300,'中美史克',1),('M003','头孢克肟分散片','0.1g*12片','盒',26.00,45,'扬子江药业',1),('M004','维生素C片','100mg*100片','瓶',6.50,3,'华中药业',1),('M005','复方甘草片','100片','瓶',9.90,200,'太极集团',1),('M006','过期测试药品','10片','盒',1.00,100,'测试厂家',0);
/*!40000 ALTER TABLE `t_medicine` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `t_patient`
--

DROP TABLE IF EXISTS `t_patient`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_patient` (
  `patient_id` varchar(20) NOT NULL COMMENT '患者编号 P20260001',
  `name` varchar(50) NOT NULL COMMENT '姓名',
  `gender` varchar(4) DEFAULT NULL COMMENT '性别',
  `age` int DEFAULT NULL COMMENT '年龄',
  `id_card` varchar(18) NOT NULL COMMENT '身份证号',
  `phone` varchar(20) DEFAULT NULL COMMENT '联系电话',
  `address` varchar(200) DEFAULT NULL COMMENT '家庭住址',
  `username` varchar(50) NOT NULL COMMENT '登录账号',
  `password` varchar(100) NOT NULL COMMENT '登录密码',
  `create_time` datetime DEFAULT CURRENT_TIMESTAMP COMMENT '注册时间',
  `status` tinyint DEFAULT '1' COMMENT '1正常 0注销',
  PRIMARY KEY (`patient_id`),
  UNIQUE KEY `id_card` (`id_card`),
  UNIQUE KEY `username` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='患者表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_patient`
--

LOCK TABLES `t_patient` WRITE;
/*!40000 ALTER TABLE `t_patient` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_patient` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `t_presc_detail`
--

DROP TABLE IF EXISTS `t_presc_detail`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_presc_detail` (
  `detail_id` int NOT NULL AUTO_INCREMENT COMMENT '明细ID',
  `presc_id` int NOT NULL COMMENT '处方ID',
  `med_id` varchar(20) NOT NULL COMMENT '药品编号',
  `quantity` int NOT NULL COMMENT '数量',
  `dosage` varchar(100) DEFAULT NULL COMMENT '用法用量',
  `unit_price` decimal(10,2) NOT NULL COMMENT '开方时单价快照',
  `subtotal` decimal(10,2) NOT NULL COMMENT '小计',
  PRIMARY KEY (`detail_id`),
  KEY `fk_detail_med` (`med_id`),
  KEY `idx_detail_presc` (`presc_id`),
  CONSTRAINT `fk_detail_med` FOREIGN KEY (`med_id`) REFERENCES `t_medicine` (`med_id`),
  CONSTRAINT `fk_detail_presc` FOREIGN KEY (`presc_id`) REFERENCES `t_prescription` (`presc_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='处方明细表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_presc_detail`
--

LOCK TABLES `t_presc_detail` WRITE;
/*!40000 ALTER TABLE `t_presc_detail` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_presc_detail` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `t_prescription`
--

DROP TABLE IF EXISTS `t_prescription`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_prescription` (
  `presc_id` int NOT NULL AUTO_INCREMENT COMMENT '处方ID',
  `visit_id` int DEFAULT NULL COMMENT '就诊ID',
  `patient_id` varchar(20) DEFAULT NULL COMMENT '患者编号',
  `doctor_id` varchar(10) DEFAULT NULL COMMENT '医生编号',
  `diagnosis` varchar(200) DEFAULT NULL COMMENT '诊断结果',
  `status` tinyint DEFAULT '1' COMMENT '1有效 0作废',
  `create_time` datetime DEFAULT CURRENT_TIMESTAMP COMMENT '开方时间',
  PRIMARY KEY (`presc_id`),
  KEY `fk_presc_visit` (`visit_id`),
  KEY `fk_presc_doctor` (`doctor_id`),
  KEY `idx_presc_patient` (`patient_id`,`status`),
  CONSTRAINT `fk_presc_doctor` FOREIGN KEY (`doctor_id`) REFERENCES `t_doctor` (`doctor_id`),
  CONSTRAINT `fk_presc_patient` FOREIGN KEY (`patient_id`) REFERENCES `t_patient` (`patient_id`),
  CONSTRAINT `fk_presc_visit` FOREIGN KEY (`visit_id`) REFERENCES `t_visit_record` (`visit_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='处方表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_prescription`
--

LOCK TABLES `t_prescription` WRITE;
/*!40000 ALTER TABLE `t_prescription` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_prescription` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `t_visit_record`
--

DROP TABLE IF EXISTS `t_visit_record`;
/*!40101 SET @saved_cs_client     = @@character_set_client */;
/*!50503 SET character_set_client = utf8mb4 */;
CREATE TABLE `t_visit_record` (
  `visit_id` int NOT NULL AUTO_INCREMENT COMMENT '就诊ID',
  `patient_id` varchar(20) DEFAULT NULL COMMENT '患者编号',
  `doctor_id` varchar(10) DEFAULT NULL COMMENT '医生编号',
  `appoint_id` int DEFAULT NULL COMMENT '预约ID',
  `visit_time` datetime DEFAULT CURRENT_TIMESTAMP COMMENT '就诊时间',
  `chief_complaint` text COMMENT '主诉',
  `present_illness` text COMMENT '现病史',
  `temp` decimal(3,1) DEFAULT NULL COMMENT '体温',
  `blood_pressure` varchar(10) DEFAULT NULL COMMENT '血压',
  `heart_rate` int DEFAULT NULL COMMENT '心率',
  PRIMARY KEY (`visit_id`),
  KEY `fk_visit_appoint` (`appoint_id`),
  KEY `idx_visit_patient` (`patient_id`),
  KEY `idx_visit_doctor` (`doctor_id`),
  CONSTRAINT `fk_visit_appoint` FOREIGN KEY (`appoint_id`) REFERENCES `t_appointment` (`appoint_id`),
  CONSTRAINT `fk_visit_doctor` FOREIGN KEY (`doctor_id`) REFERENCES `t_doctor` (`doctor_id`),
  CONSTRAINT `fk_visit_patient` FOREIGN KEY (`patient_id`) REFERENCES `t_patient` (`patient_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_0900_ai_ci COMMENT='就诊记录表';
/*!40101 SET character_set_client = @saved_cs_client */;

--
-- Dumping data for table `t_visit_record`
--

LOCK TABLES `t_visit_record` WRITE;
/*!40000 ALTER TABLE `t_visit_record` DISABLE KEYS */;
/*!40000 ALTER TABLE `t_visit_record` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2026-10-14 11:19:52
