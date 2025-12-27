import axios from 'axios';

const api = axios.create({
    baseURL: 'http://localhost:3002/api/sensor', // 백엔드 주소
    timeout: 5000
});

export default api;
