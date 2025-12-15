#!/bin/bash


mkdir -p mnt/server
rm -rf mnt/demo_agent 
mkdir -p logs


echo "[Startup] Launching DFS Server..."
./build/bin/dfs-server -a 0.0.0.0:50051 -m mnt/server -n 4 > logs/server.log 2>&1 &
SERVER_PID=$!


echo "[Startup] Launching Demo Agent..."
python3 tests/integration/demo_agent.py > logs/demo_agent.log 2>&1 &
AGENT_PID=$!

echo "[Startup] Services are running."
echo "   - DFS Server PID: $SERVER_PID"
echo "   - Demo Agent PID: $AGENT_PID"
echo "   - Logs are located in /app/logs/"


wait $SERVER_PID $AGENT_PID