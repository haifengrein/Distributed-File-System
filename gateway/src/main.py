import asyncio
import logging
import os
import httpx
import json
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
import grpc
from proto_gen import dfs_service_pb2, dfs_service_pb2_grpc

# Configuration
DFS_SERVER_ADDRESS = os.getenv("DFS_SERVER_ADDRESS", "localhost:50051")
DEMO_AGENT_URL = "http://dfs-dev:5000" 

# Logging Setup
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("gateway")

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

class ConnectionManager:
    def __init__(self):
        self.active_connections: list[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)

    async def broadcast(self, message: dict):
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except Exception as e:
                logger.error(f"Broadcast error: {e}")

manager = ConnectionManager()

async def monitor_events():
    logger.info(f"Connecting to DFS Server at {DFS_SERVER_ADDRESS}...")
    while True:
        try:
            async with grpc.aio.insecure_channel(DFS_SERVER_ADDRESS) as channel:
                stub = dfs_service_pb2_grpc.MonitorServiceStub(channel)
                request = dfs_service_pb2.MonitorRequest(include_past_events=False)
                logger.info("Stream connected.")
                async for event in stub.StreamEvents(request):
                    event_data = {
                        "type": event.type, 
                        "timestamp": event.timestamp,
                        "source": event.source_node,
                        "target": event.target_node,
                        "resource": event.resource,
                        "metadata": dict(event.metadata)
                    }
                    await manager.broadcast(event_data)
        except Exception:
            await asyncio.sleep(5)

@app.on_event("startup")
async def startup_event():
    asyncio.create_task(monitor_events())

@app.get("/")
async def get():
    return {"status": "running"}

# Generic Runner Endpoint
@app.post("/api/run/{scenario}")
async def run_scenario(scenario: str):
    async with httpx.AsyncClient(timeout=30.0) as client: # Increased timeout for conflict test
        try:
            # Relay to Agent
            resp = await client.post(f"{DEMO_AGENT_URL}/run/{scenario}")
            result = resp.json()
            
            await manager.broadcast({
                "type": 999,
                "timestamp": 0,
                "source": "System",
                "target": "",
                "resource": "ScenarioResult",
                "metadata": {
                    "result_json": json.dumps(result) 
                } 
            })
            return result
        except Exception as e:
            return {"status": "error", "message": str(e)}

@app.websocket("/ws/events")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True: await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)