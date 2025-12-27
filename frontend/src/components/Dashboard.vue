<script setup>
import { ref, onMounted, onUnmounted } from 'vue';
import api from '../services/api';
import socket from '../services/socket';

// 상태 변수
const sensors = ref({
  temp: 0,
  humi: 0,
  soil: 0
});

const actuators = ref({
  fan: { state: false, auto: false },
  pump: { state: false, auto: false },
  led: { state: false, auto: false }
});

const connectionStatus = ref('Disconnected');
const lastUpdated = ref('-');

// 사용자 조작 중 소켓 업데이트 무시를 위한 플래그
const userInteracting = ref(false);
let interactionTimeout = null;

// 초기 데이터 로드
const fetchInitialData = async () => {
  try {
    const response = await api.get('/dashboard');
    const data = response.data;
    
    sensors.value.temp = data.temp;
    sensors.value.humi = data.humi;
    sensors.value.soil = data.soil;

    // DB에서 가져온 액추에이터 상태 매핑 (actuator_id: 1=Fan, 2=Pump, 3=LED)
    data.actuators.forEach(act => {
      if (act.actuator_id === 1) {
        actuators.value.fan.state = !!act.current_state;
        actuators.value.fan.auto = !!act.is_auto_mode;
      } else if (act.actuator_id === 2) {
        actuators.value.pump.state = !!act.current_state;
        actuators.value.pump.auto = !!act.is_auto_mode;
      } else if (act.actuator_id === 3) {
        actuators.value.led.state = !!act.current_state;
        actuators.value.led.auto = !!act.is_auto_mode;
      }
    });
  } catch (error) {
    console.error('Failed to fetch initial data:', error);
  }
};

// 제어 명령 전송
const toggleActuator = async (device, id) => {
  // Auto 모드일 때는 수동 제어 불가
  if (actuators.value[device].auto) {
    alert('자동 모드에서는 수동으로 제어할 수 없습니다.');
    return;
  }

  const newState = !actuators.value[device].state;
  // 낙관적 업데이트 (UI 먼저 변경)
  actuators.value[device].state = newState;

  // 사용자 조작 플래그 설정 (2초간 소켓 업데이트 무시)
  userInteracting.value = true;
  clearTimeout(interactionTimeout);
  interactionTimeout = setTimeout(() => {
    userInteracting.value = false;
  }, 2000);

  try {
    await api.post('/command', {
      actuator_id: id,
      command: newState ? 'ON' : 'OFF'
    });
  } catch (error) {
    console.error('Control failed:', error);
    // 실패 시 롤백
    actuators.value[device].state = !newState;
    userInteracting.value = false;
  }
};

const toggleAutoMode = async (device, id) => {
  const newAutoState = !actuators.value[device].auto;
  actuators.value[device].auto = newAutoState;

  // 모드 변경 시에도 잠깐 소켓 업데이트 무시
  userInteracting.value = true;
  clearTimeout(interactionTimeout);
  interactionTimeout = setTimeout(() => {
    userInteracting.value = false;
  }, 2000);

  try {
    await api.post('/command', {
      actuator_id: id,
      is_auto: newAutoState
    });
  } catch (error) {
    console.error('Mode change failed:', error);
    actuators.value[device].auto = !newAutoState;
    userInteracting.value = false;
  }
};

onMounted(() => {
  fetchInitialData();

  // 소켓 연결 이벤트
  socket.on('connect', () => {
    connectionStatus.value = 'Connected';
  });

  socket.on('disconnect', () => {
    connectionStatus.value = 'Disconnected';
  });

  // 센서 데이터 업데이트 이벤트
  socket.on('sensor_update', (data) => {
    sensors.value.temp = data.temp;
    sensors.value.humi = data.humi;
    sensors.value.soil = data.soil;
    lastUpdated.value = new Date(data.timestamp).toLocaleTimeString();

    // 사용자가 조작 중일 때는 액추에이터 상태 업데이트 무시 (깜빡임 방지)
    if (!userInteracting.value && data.actuators) {
      actuators.value.fan.state = !!data.actuators.fan.state;
      actuators.value.fan.auto = !!data.actuators.fan.auto;
      
      actuators.value.pump.state = !!data.actuators.pump.state;
      actuators.value.pump.auto = !!data.actuators.pump.auto;

      actuators.value.led.state = !!data.actuators.led.state;
      actuators.value.led.auto = !!data.actuators.led.auto;
    }
  });
});

onUnmounted(() => {
  socket.off('connect');
  socket.off('disconnect');
  socket.off('sensor_update');
});
</script>

<template>
  <div class="dashboard">
    <header class="header">
      <h1>Smart Farm Dashboard</h1>
      <div class="status-bar">
        <span :class="['status-indicator', connectionStatus.toLowerCase()]">● {{ connectionStatus }}</span>
        <span class="last-updated">Last Updated: {{ lastUpdated }}</span>
      </div>
    </header>

    <div class="sensor-grid">
      <div class="card sensor-card">
        <h3>Temperature</h3>
        <div class="value">{{ sensors.temp }}°C</div>
      </div>
      <div class="card sensor-card">
        <h3>Humidity</h3>
        <div class="value">{{ sensors.humi }}%</div>
      </div>
      <div class="card sensor-card">
        <h3>Soil Moisture</h3>
        <div class="value">{{ sensors.soil }}%</div>
      </div>
    </div>

    <div class="control-grid">
      <!-- FAN Control -->
      <div class="card control-card">
        <div class="card-header">
          <h3>Fan Control</h3>
          <button 
            @click="toggleAutoMode('fan', 1)" 
            :class="['mode-btn', actuators.fan.auto ? 'auto' : 'manual']"
          >
            {{ actuators.fan.auto ? 'AUTO' : 'MANUAL' }}
          </button>
        </div>
        <div class="control-body">
          <div class="status-display">
            Status: <span :class="actuators.fan.state ? 'on' : 'off'">{{ actuators.fan.state ? 'ON' : 'OFF' }}</span>
          </div>
          <button 
            @click="toggleActuator('fan', 1)" 
            :disabled="actuators.fan.auto"
            :class="['power-btn', actuators.fan.state ? 'active' : '']"
          >
            {{ actuators.fan.state ? 'TURN OFF' : 'TURN ON' }}
          </button>
        </div>
      </div>

      <!-- PUMP Control -->
      <div class="card control-card">
        <div class="card-header">
          <h3>Water Pump</h3>
          <button 
            @click="toggleAutoMode('pump', 2)" 
            :class="['mode-btn', actuators.pump.auto ? 'auto' : 'manual']"
          >
            {{ actuators.pump.auto ? 'AUTO' : 'MANUAL' }}
          </button>
        </div>
        <div class="control-body">
          <div class="status-display">
            Status: <span :class="actuators.pump.state ? 'on' : 'off'">{{ actuators.pump.state ? 'ON' : 'OFF' }}</span>
          </div>
          <button 
            @click="toggleActuator('pump', 2)" 
            :disabled="actuators.pump.auto"
            :class="['power-btn', actuators.pump.state ? 'active' : '']"
          >
            {{ actuators.pump.state ? 'TURN OFF' : 'TURN ON' }}
          </button>
        </div>
      </div>

      <!-- LED Control -->
      <div class="card control-card">
        <div class="card-header">
          <h3>Grow LED</h3>
          <button 
            @click="toggleAutoMode('led', 3)" 
            :class="['mode-btn', actuators.led.auto ? 'auto' : 'manual']"
          >
            {{ actuators.led.auto ? 'AUTO' : 'MANUAL' }}
          </button>
        </div>
        <div class="control-body">
          <div class="status-display">
            Status: <span :class="actuators.led.state ? 'on' : 'off'">{{ actuators.led.state ? 'ON' : 'OFF' }}</span>
          </div>
          <button 
            @click="toggleActuator('led', 3)" 
            :disabled="actuators.led.auto"
            :class="['power-btn', actuators.led.state ? 'active' : '']"
          >
            {{ actuators.led.state ? 'TURN OFF' : 'TURN ON' }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>

<style scoped>
.dashboard {
  padding: 20px;
}

.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 30px;
}

.status-bar {
  display: flex;
  gap: 15px;
  font-size: 0.9rem;
  color: #666;
}

.status-indicator.connected { color: #2ecc71; }
.status-indicator.disconnected { color: #e74c3c; }

.sensor-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 20px;
  margin-bottom: 30px;
}

.control-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 20px;
}

.card {
  background: white;
  border-radius: 12px;
  padding: 20px;
  box-shadow: 0 4px 6px rgba(0,0,0,0.05);
}

.sensor-card {
  text-align: center;
}

.sensor-card .value {
  font-size: 2.5rem;
  font-weight: bold;
  color: #2c3e50;
  margin-top: 10px;
}

.control-card .card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  border-bottom: 1px solid #eee;
  padding-bottom: 10px;
}

.mode-btn {
  padding: 5px 10px;
  border-radius: 20px;
  border: none;
  font-size: 0.8rem;
  font-weight: bold;
  cursor: pointer;
  transition: all 0.3s;
}

.mode-btn.auto { background-color: #3498db; color: white; }
.mode-btn.manual { background-color: #95a5a6; color: white; }

.control-body {
  display: flex;
  flex-direction: column;
  gap: 15px;
}

.status-display {
  font-size: 1.1rem;
}

.status-display .on { color: #2ecc71; font-weight: bold; }
.status-display .off { color: #e74c3c; font-weight: bold; }

.power-btn {
  width: 100%;
  padding: 12px;
  border: none;
  border-radius: 8px;
  background-color: #ecf0f1;
  color: #7f8c8d;
  font-weight: bold;
  cursor: pointer;
  transition: all 0.2s;
}

.power-btn:not(:disabled):hover {
  background-color: #bdc3c7;
}

.power-btn.active {
  background-color: #2ecc71;
  color: white;
}

.power-btn:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}
</style>
