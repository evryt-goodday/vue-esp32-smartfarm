import { io } from "socket.io-client";

const socket = io("http://localhost:3002", {
    transports: ["websocket"],
    autoConnect: true
});

export default socket;
