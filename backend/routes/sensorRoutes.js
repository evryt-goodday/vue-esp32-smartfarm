import express from 'express';
import { saveSensorData, getControlStatus, setControlCommand, getDashboardData, updateDeviceStatus } from '../controllers/sensorController.js';

const router = express.Router();

// 아두이노 연동 API
router.post('/data', saveSensorData);           // 센서 데이터 수신 & 소켓 발송
router.get('/control/:id', getControlStatus);   // 제어 명령 조회 (Polling)
router.post('/status', updateDeviceStatus);     // 장치 실행 결과 보고

// 프론트엔드 연동 API
router.get('/dashboard', getDashboardData);     // 초기 데이터 조회
router.post('/command', setControlCommand);     // 사용자 제어 명령

export default router;
