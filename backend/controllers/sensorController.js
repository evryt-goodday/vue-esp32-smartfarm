import pool from '../config/db.js';

// 1. 센서 데이터 저장 (ESP32 -> Server -> Socket -> Vue)
export const saveSensorData = async (req, res) => {
    // 아두이노에서 보내는 JSON 구조:
    // { temp, humi, soil, mode, fan_state, pump_state, led_state, fan_auto, pump_auto, led_auto }
    const { temp, humi, soil, fan_state, pump_state, led_state, fan_auto, pump_auto, led_auto } = req.body;
    
    let conn;
    try {
        conn = await pool.getConnection();
        
        // 1-1. 센서 데이터 저장 (Vertical)
        // sensor_id: 1=Temp, 2=Humi, 3=Soil
        if(temp !== undefined) await conn.query('INSERT INTO sensor_log (sensor_id, value) VALUES (?, ?)', [1, temp]);
        if(humi !== undefined) await conn.query('INSERT INTO sensor_log (sensor_id, value) VALUES (?, ?)', [2, humi]);
        if(soil !== undefined) await conn.query('INSERT INTO sensor_log (sensor_id, value) VALUES (?, ?)', [3, soil]);

        // 1-2. 액추에이터 실제 상태 업데이트 (모니터링용)
        // 아두이노가 현재 상태를 보고하면 DB에 반영
        // 주의: is_auto_mode는 서버->아두이노 명령이므로 여기서 업데이트하지 않음
        if(fan_state !== undefined) await conn.query('UPDATE actuator_state SET current_state = ? WHERE actuator_id = ?', [fan_state, 1]);
        if(pump_state !== undefined) await conn.query('UPDATE actuator_state SET current_state = ? WHERE actuator_id = ?', [pump_state, 2]);
        if(led_state !== undefined) await conn.query('UPDATE actuator_state SET current_state = ? WHERE actuator_id = ?', [led_state, 3]);

        // 1-3. 소켓 브로드캐스팅 (Vue 화면 갱신용)
        req.io.emit('sensor_update', { 
            temp, humi, soil, 
            actuators: {
                fan: { state: fan_state, auto: fan_auto },
                pump: { state: pump_state, auto: pump_auto },
                led: { state: led_state, auto: led_auto }
            },
            timestamp: new Date() 
        });

        res.status(200).send('Data Saved & Broadcasted');
    } catch (error) {
        console.error(error);
        res.status(500).send('Error');
    } finally {
        if (conn) conn.release();
    }
};

// 2. 제어 명령 조회 (ESP32 Polling -> Server)
export const getControlStatus = async (req, res) => {
    const actuatorId = req.params.id;
    let conn;
    try {
        conn = await pool.getConnection();
        const rows = await conn.query('SELECT target_state, is_auto_mode FROM actuator_state WHERE actuator_id = ?', [actuatorId]);
        
        if (rows.length > 0) {
            const responseData = {
                target_state: rows[0].target_state, // 1 or 0
                is_auto_mode: rows[0].is_auto_mode  // 1 or 0
            };
            // 디버깅용 로그 (너무 자주 찍히면 주석 처리)
            console.log(`[ESP32 Polling] Device ${actuatorId} -> Sending:`, responseData);
            
            res.json(responseData);
        } else {
            res.status(404).send("Device not found");
        }
    } catch (error) {
        console.error(error);
        res.status(500).send('Error');
    } finally {
        if (conn) conn.release();
    }
};

// 3. 사용자 제어 명령 (Vue -> Server)
export const setControlCommand = async (req, res) => {
    const { actuator_id, command, is_auto } = req.body; 
    console.log(`[Frontend Command] Received -> Device: ${actuator_id}, Command: ${command}, Auto: ${is_auto}`);

    // command: 'ON' or 'OFF' (수동 제어 시)
    // is_auto: true or false (모드 변경 시)

    let conn;
    try {
        conn = await pool.getConnection();
        
        // 3-1. 모드 변경 요청인 경우
        if (is_auto !== undefined) {
            console.log(`[DB Update] Device ${actuator_id} -> Auto Mode: ${is_auto}`);
            await conn.query('UPDATE actuator_state SET is_auto_mode = ? WHERE actuator_id = ?', [is_auto, actuator_id]);
            
            // 로그 기록
            await conn.query('INSERT INTO actuator_log (actuator_id, command, executor) VALUES (?, ?, ?)', 
                [actuator_id, is_auto ? 'AUTO_MODE' : 'MANUAL_MODE', 'USER']);
        }

        // 3-2. 수동 제어 명령인 경우 (ON/OFF)
        if (command !== undefined) {
            const targetState = (command === 'ON');
            console.log(`[DB Update] Device ${actuator_id} -> Target State: ${targetState} (${command})`);
            await conn.query('UPDATE actuator_state SET target_state = ? WHERE actuator_id = ?', [targetState, actuator_id]);
            
            // 로그 기록
            await conn.query('INSERT INTO actuator_log (actuator_id, command, executor) VALUES (?, ?, ?)', 
                [actuator_id, command, 'USER']);
        }

        res.status(200).json({ message: 'Command Updated' });
    } catch (error) {

        console.error(error);
        res.status(500).json({ error: 'Database Error' });
    } finally {
        if (conn) conn.release();
    }
};

// 4. 대시보드 초기 데이터 조회 (Vue -> Server)
export const getDashboardData = async (req, res) => {
    let conn;
    try {
        conn = await pool.getConnection();
        
        // 최신 센서 값 조회 (각 센서별 마지막 값)
        // 실제로는 더 복잡한 쿼리가 필요할 수 있으나, 여기선 간단히 구현
        // (실무에서는 View를 생성하거나 윈도우 함수 사용 권장)
        const tempRow = await conn.query('SELECT value FROM sensor_log WHERE sensor_id = 1 ORDER BY id DESC LIMIT 1');
        const humiRow = await conn.query('SELECT value FROM sensor_log WHERE sensor_id = 2 ORDER BY id DESC LIMIT 1');
        const soilRow = await conn.query('SELECT value FROM sensor_log WHERE sensor_id = 3 ORDER BY id DESC LIMIT 1');

        // 액추에이터 상태 조회
        const actuators = await conn.query('SELECT actuator_id, current_state, is_auto_mode FROM actuator_state');

        res.json({
            temp: tempRow[0]?.value || 0,
            humi: humiRow[0]?.value || 0,
            soil: soilRow[0]?.value || 0,
            actuators: actuators
        });
    } catch (error) {
        console.error(error);
        res.status(500).send('Error');
    } finally {
        if (conn) conn.release();
    }
};

// 5. 장치 실행 결과 보고 (Arduino -> Server) - 로그용
export const updateDeviceStatus = async (req, res) => {
    // 아두이노 코드의 sendCommandResult 함수 대응
    // 현재 아두이노 코드는 /status로 결과를 보냄
    const { commandId, status, actuators } = req.body;
    // 필요하다면 actuator_log에 실행 완료 표시를 업데이트할 수 있음
    // 여기서는 간단히 성공 응답만 반환
    res.status(200).send('OK');
};
