# 🌿 Vue-ESP32 Smart Farm

ESP32와 Vue.js를 활용한 **웹 기반 스마트팜 원격 제어 및 모니터링 시스템**입니다.

## 🛠 주요 기능

*   **실시간 모니터링**: 온도, 습도, 토양 수분, 조도 데이터를 웹 대시보드에서 실시간 확인 (Socket.io)
*   **원격 제어**: 웹 인터페이스를 통해 팬, 펌프, LED 조명 제어
*   **듀얼 제어 모드**:
    *   **Auto Mode**: 센서 값 기반 자동 제어 (습도→팬, 수분→펌프, 조도→LED)
    *   **Manual Mode**: 사용자 강제 제어 (비상 시 또는 유지보수용)
*   **데이터 시각화**: Chart.js를 이용한 센서 데이터 추이 그래프 제공

## 📂 시스템 아키텍처

| 영역 | 기술 스택 | 역할 |
| :--- | :--- | :--- |
| **Firmware** | ESP32 (Arduino IDE) | 센서 데이터 수집, 액추에이터 구동, HTTP/WiFi 통신 |
| **Backend** | Node.js, Express, Socket.io | REST API 제공, 실시간 데이터 중계, DB 연동 |
| **Database** | MariaDB | 센서 로그 저장, 장치 상태 및 설정 관리 |
| **Frontend** | Vue.js 3, Chart.js | 사용자 대시보드, 실시간 데이터 표시, 제어 명령 전송 |

## 🔄 데이터 흐름

1.  **수집**: ESP32 → Server (HTTP POST)
2.  **저장 & 전파**: Server → DB (Save) & Server → Frontend (Socket Emit)
3.  **제어**: Frontend → Server (Command) → DB (Update) → ESP32 (Polling) 
