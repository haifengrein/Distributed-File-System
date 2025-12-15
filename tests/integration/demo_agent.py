import http.server
import socketserver
import subprocess
import json
import os
import threading
import time
import random
import hashlib

PORT = 5000
MOUNT_ROOT = "mnt/demo_agent"
SERVER_ADDR = "localhost:50051"

# Ensure directories
os.makedirs(f"{MOUNT_ROOT}/server", exist_ok=True)
os.makedirs(f"{MOUNT_ROOT}/client_a", exist_ok=True)
os.makedirs(f"{MOUNT_ROOT}/client_b", exist_ok=True)

class DemoRequestHandler(http.server.SimpleHTTPRequestHandler):
    def do_POST(self):
        if self.path == '/run/basic':
            self.run_basic_scenario()
        elif self.path == '/run/conflict':
            self.run_conflict_scenario()
        elif self.path == '/run/performance':
            self.run_performance_scenario()
        else:
            self.send_error(404)

    def _run_client(self, client_name, cmd, file, timeout=5, custom_id=None):
        mount_dir = f"{MOUNT_ROOT}/{client_name}"
        args = ["./build/bin/dfs-client", "-a", SERVER_ADDR, "-m", mount_dir, cmd, file]
        
        env = os.environ.copy()
        if custom_id:
            env["DFS_CLIENT_ID"] = custom_id
        else:
            env["DFS_CLIENT_ID"] = f"Client-{client_name.split('_')[1].upper()}"

        try:
            # Measure time roughly here (including process overhead)
            start_t = time.perf_counter()
            result = subprocess.run(args, env=env, capture_output=True, text=True, timeout=timeout)
            end_t = time.perf_counter()
            
            duration_ms = (end_t - start_t) * 1000
            return result.returncode == 0, result.stdout + result.stderr, duration_ms
        except subprocess.TimeoutExpired:
            return False, "Timeout", 0

    def _write_local(self, client_name, filename, content):
        with open(f"{MOUNT_ROOT}/{client_name}/{filename}", "w") as f:
            f.write(content)

    def _calculate_hash(self, path):
        if not os.path.exists(path): return "MISSING"
        with open(path, "rb") as f:
            return hashlib.md5(f.read()).hexdigest()

    def run_basic_scenario(self):
        filename = f"basic_{int(time.time())}.txt"
        content = f"Basic Consistency Data {random.randint(1000,9999)}"
        
        self._write_local("client_a", filename, content)
        
        # Store
        _, _, _ = self._run_client("client_a", "store", filename)
        # Fetch
        _, _, _ = self._run_client("client_a", "fetch", filename)
        
        server_path = f"mnt/server/{filename}"
        client_path = f"{MOUNT_ROOT}/client_a/{filename}"
        
        res = {
            "status": "success",
            "type": "consistency",
            "client_hash": self._calculate_hash(client_path),
            "server_hash": self._calculate_hash(server_path),
            "message": "Data successfully synchronized."
        }
        self._send_json(res)

    def run_conflict_scenario(self):
        filename = "hotspot_config.json"
        duration = 10 
        
        self._write_local("client_a", filename, "Initial Config")
        self._run_client("client_a", "store", filename)
        
        def hammer(client_name, client_id):
            end_time = time.time() + duration
            while time.time() < end_time:
                content = f"Update from {client_name} at {time.time()}"
                self._write_local(client_name, filename, content)
                self._run_client(client_name, "store", filename, timeout=2, custom_id=client_id)
                time.sleep(random.uniform(0.1, 0.3))

        t1 = threading.Thread(target=hammer, args=("client_a", "Client-A"))
        t2 = threading.Thread(target=hammer, args=("client_b", "Client-B"))
        
        t1.start()
        t2.start()
        t1.join()
        t2.join()
        
        self._run_client("client_a", "fetch", filename)
        self._run_client("client_b", "fetch", filename)
        
        server_path = f"mnt/server/{filename}"
        hash_a = self._calculate_hash(f"{MOUNT_ROOT}/client_a/{filename}")
        hash_b = self._calculate_hash(f"{MOUNT_ROOT}/client_b/{filename}")
        hash_s = self._calculate_hash(server_path)
        
        is_consistent = (hash_a == hash_b == hash_s)
        
        res = {
            "status": "success" if is_consistent else "failure",
            "type": "consistency",
            "client_hash": hash_a,
            "server_hash": hash_s,
            "message": "Conflict Stress Test Complete."
        }
        self._send_json(res)

    def run_performance_scenario(self):
        """
        Runs 5 sequential writes and calculates latency statistics.
        """
        latencies = []
        iterations = 5
        filename = "perf_test.dat"
        
        for i in range(iterations):
            content = os.urandom(1024).hex() # 2KB file
            self._write_local("client_a", filename, content)
            
            success, out, ms = self._run_client("client_a", "store", filename, custom_id="Perf-Tester")
            
            if success:
                latencies.append(ms)
            else:
                print(f"Perf run failed: {out}")
            
            time.sleep(0.1) # Cool down slightly
            
        if not latencies:
            self._send_json({"status": "error", "message": "All performance runs failed."})
            return

        avg_ms = sum(latencies) / len(latencies)
        max_ms = max(latencies)
        min_ms = min(latencies)
        
        res = {
            "status": "success",
            "type": "performance",
            "stats": {
                "avg": round(avg_ms, 2),
                "max": round(max_ms, 2),
                "min": round(min_ms, 2),
                "samples": len(latencies)
            },
            "message": "Latency Test Complete"
        }
        self._send_json(res)

    def _send_json(self, data):
        self.send_response(200)
        self.send_header('Content-type', 'application/json')
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

socketserver.TCPServer.allow_reuse_address = True
print(f"🚀 Demo Agent listening on port {PORT}")
with socketserver.TCPServer(("", PORT), DemoRequestHandler) as httpd:
    httpd.serve_forever()