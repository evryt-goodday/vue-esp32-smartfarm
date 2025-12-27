-- 데이터베이스 생성
CREATE DATABASE IF NOT EXISTS smartfarm_db;
USE smartfarm_db;

-- 1. 하우스 정보
CREATE TABLE IF NOT EXISTS house (
    id INT PRIMARY KEY AUTO_INCREMENT COMMENT '하우스 고유 ID',
    name VARCHAR(50) NOT NULL COMMENT '하우스 이름',
    description VARCHAR(100) COMMENT '하우스 설명'
) COMMENT='스마트팜 하우스 기본 정보';

-- 2. 센서 장치 목록
CREATE TABLE IF NOT EXISTS sensor_device (
    id INT PRIMARY KEY COMMENT '센서 장치 ID (1:온도, 2:습도, 3:토양수분)',
    house_id INT COMMENT '소속 하우스 ID',
    name VARCHAR(50) NOT NULL COMMENT '센서 이름',
    location VARCHAR(100) COMMENT '센서 설치 위치',
    description VARCHAR(100) COMMENT '센서 설명'
) COMMENT='센서 장치 마스터 테이블 (온도, 습도, 토양수분 등)';

-- 3. 액추에이터 장치 목록
CREATE TABLE IF NOT EXISTS actuator_device (
    id INT PRIMARY KEY COMMENT '액추에이터 ID (1:팬, 2:펌프, 3:LED)',
    house_id INT COMMENT '소속 하우스 ID',
    name VARCHAR(50) NOT NULL COMMENT '장치 이름',
    location VARCHAR(100) COMMENT '장치 설치 위치',
    description VARCHAR(100) COMMENT '장치 설명'
) COMMENT='제어 장치 마스터 테이블 (팬, 펌프, LED 등)';

-- 4. 센서 데이터 로그 (확장성 고려: Vertical)
CREATE TABLE IF NOT EXISTS sensor_log (
    id BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '로그 고유 ID',
    sensor_id INT COMMENT '센서 장치 ID (sensor_device 참조)',
    value FLOAT COMMENT '측정값 (온도:°C, 습도:%, 토양수분:%)',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '측정 시각'
) COMMENT='센서 측정값 이력 (시계열 데이터, Vertical 구조)';

-- 5. 액추에이터 현재 상태 (실시간 제어용)
-- 장치별로 독립적인 모드 설정 가능
CREATE TABLE IF NOT EXISTS actuator_state (
    actuator_id INT PRIMARY KEY COMMENT '액추에이터 ID (actuator_device 참조)',
    target_state BOOLEAN DEFAULT FALSE COMMENT '목표 상태 (0:OFF, 1:ON) - 사용자/서버가 설정',
    current_state BOOLEAN DEFAULT FALSE COMMENT '실제 상태 (0:OFF, 1:ON) - 아두이노가 보고',
    is_auto_mode BOOLEAN DEFAULT TRUE COMMENT '제어 모드 (0:수동, 1:자동) - 장치별 독립',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '최종 변경 시각'
) COMMENT='액추에이터 실시간 상태 및 제어 명령 테이블';

-- 6. 액추에이터 제어 이력 (히스토리용)
CREATE TABLE IF NOT EXISTS actuator_log (
    id BIGINT AUTO_INCREMENT PRIMARY KEY COMMENT '로그 고유 ID',
    actuator_id INT COMMENT '액추에이터 ID',
    command VARCHAR(20) COMMENT '실행 명령 (ON, OFF, AUTO_MODE, MANUAL_MODE)',
    executor VARCHAR(20) COMMENT '실행 주체 (AUTO:자동로직, USER:사용자, ADMIN:관리자)',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '명령 실행 시각'
) COMMENT='액추에이터 제어 이력 로그 (감사/분석용)';

-- 초기 데이터 삽입 (중복 방지)
INSERT IGNORE INTO house (id, name, description) VALUES (1, 'House A', 'Main Greenhouse');

INSERT IGNORE INTO sensor_device (id, house_id, name, location) VALUES 
(1, 1, 'Temperature', 'Center Wall'), 
(2, 1, 'Humidity', 'Center Wall'), 
(3, 1, 'Soil Moisture', 'Bed 1');

INSERT IGNORE INTO actuator_device (id, house_id, name, location) VALUES 
(1, 1, 'Fan', 'Roof Vent'), 
(2, 1, 'Pump', 'Water Tank'), 
(3, 1, 'LED', 'Ceiling');

-- 초기 상태 설정 (기본값: 자동 모드)
INSERT IGNORE INTO actuator_state (actuator_id, is_auto_mode) VALUES (1, TRUE), (2, TRUE), (3, TRUE);
