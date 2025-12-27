import express from 'express'
import http from 'http'
import { Server } from 'socket.io'
import cors from 'cors'
import bodyParser from 'body-parser'
import sensorRoutes from './routes/sensorRoutes.js'

const app = express()
const server = http.createServer(app)
const io = new Server(server, {
	cors: {
		origin: '*', // 모든 출처 허용 (개발용)
		methods: ['GET', 'POST'],
	},
})

const PORT = 3002;

app.use(cors())
app.use(bodyParser.json())
app.use(bodyParser.urlencoded({ extended: true }))

app.use((req, resm, next) => {
	req.io = io;
	next()
})

app.use('/api/sensor', sensorRoutes)

io.on('connection', (socket) => {
	console.log('New client connected:', socket.id)
	socket.on('disconnect', () => {
		console.log('Client disconnected:', socket.id)
	})
})

server.listen(PORT, () => {
	console.log(`Server running on port ${PORT}`)
})