import http.server
import socketserver
import subprocess
import json
import os
import threading
import time
import random
import hashlib

PORT = int(os.getenv("PORT", "5000"))
DFS_SERVER_ADDRESS = os.getenv("DFS_SERVER_ADDRESS", "dfs-server:50051")
DFS_CLIENT_BIN = os.getenv("DFS_CLIENT_BIN", "dfs-client")
DFS_SERVER_DATA_DIR = os.getenv("DFS_SERVER_DATA_DIR", "/data")
MOUNT_ROOT = os.getenv("DFS_AGENT_MOUNT_ROOT", "/mnt/demo_agent")


def ensure_dirs() -> None:
    os.makedirs(f"{MOUNT_ROOT}/client_a", exist_ok=True)
    os.makedirs(f"{MOUNT_ROOT}/client_b", exist_ok=True)


def calculate_hash(path: str) -> str:
    if not os.path.exists(path):
        return "MISSING"
    with open(path, "rb") as f:
        return hashlib.md5(f.read()).hexdigest()


def write_local(client_name: str, filename: str, content: str) -> None:
    with open(f"{MOUNT_ROOT}/{client_name}/{filename}", "w") as f:
        f.write(content)


def run_client(client_name: str, cmd: str, file: str, timeout: int = 5, custom_id: str | None = None):
    mount_dir = f"{MOUNT_ROOT}/{client_name}"
    args = [DFS_CLIENT_BIN, "-a", DFS_SERVER_ADDRESS, "-m", mount_dir, cmd, file]

    env = os.environ.copy()
    env["DFS_CLIENT_ID"] = custom_id or f"Client-{client_name.split('_')[1].upper()}"

    try:
        start_t = time.perf_counter()
        result = subprocess.run(args, env=env, capture_output=True, text=True, timeout=timeout)
        duration_ms = (time.perf_counter() - start_t) * 1000
        return result.returncode == 0, result.stdout + result.stderr, duration_ms
    except subprocess.TimeoutExpired:
        return False, "Timeout", 0.0


class DemoRequestHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):  # noqa: N802
        if self.path == "/run/basic":
            self.run_basic_scenario()
        elif self.path == "/run/conflict":
            self.run_conflict_scenario()
        elif self.path == "/run/performance":
            self.run_performance_scenario()
        else:
            self.send_error(404)

    def run_basic_scenario(self):
        filename = f"basic_{int(time.time())}.txt"
        content = f"Basic Consistency Data {random.randint(1000, 9999)}"
        write_local("client_a", filename, content)

        run_client("client_a", "store", filename, custom_id="Client-A")
        run_client("client_b", "fetch", filename, custom_id="Client-B")

        server_path = f"{DFS_SERVER_DATA_DIR}/{filename}"
        reader_path = f"{MOUNT_ROOT}/client_b/{filename}"
        writer_path = f"{MOUNT_ROOT}/client_a/{filename}"

        self._send_json(
            {
                "status": "success",
                "type": "consistency",
                "client_hash": calculate_hash(reader_path),
                "server_hash": calculate_hash(server_path),
                "message": "Client-A stored, Client-B fetched.",
                "writer_hash": calculate_hash(writer_path),
            }
        )

    def run_conflict_scenario(self):
        filename = "hotspot_config.json"
        duration_s = 10

        write_local("client_a", filename, "Initial Config")
        run_client("client_a", "store", filename)

        def hammer(client_name: str, client_id: str):
            end_time = time.time() + duration_s
            while time.time() < end_time:
                content = f"Update from {client_name} at {time.time()}"
                write_local(client_name, filename, content)
                run_client(client_name, "store", filename, timeout=2, custom_id=client_id)
                time.sleep(random.uniform(0.1, 0.3))

        t1 = threading.Thread(target=hammer, args=("client_a", "Client-A"))
        t2 = threading.Thread(target=hammer, args=("client_b", "Client-B"))
        t1.start()
        t2.start()
        t1.join()
        t2.join()

        run_client("client_a", "fetch", filename)
        run_client("client_b", "fetch", filename)

        server_path = f"{DFS_SERVER_DATA_DIR}/{filename}"
        hash_a = calculate_hash(f"{MOUNT_ROOT}/client_a/{filename}")
        hash_b = calculate_hash(f"{MOUNT_ROOT}/client_b/{filename}")
        hash_s = calculate_hash(server_path)

        is_consistent = hash_a == hash_b == hash_s
        self._send_json(
            {
                "status": "success" if is_consistent else "failure",
                "type": "consistency",
                "client_hash": hash_a,
                "server_hash": hash_s,
                "message": "Conflict Stress Test Complete.",
            }
        )

    def run_performance_scenario(self):
        latencies = []
        filename = "perf_test.dat"

        for i in range(5):
            content = os.urandom(1024).hex()
            client_name = "client_a" if i % 2 == 0 else "client_b"
            client_id = "Perf-A" if client_name == "client_a" else "Perf-B"
            write_local(client_name, filename, content)

            success, out, ms = run_client(client_name, "store", filename, custom_id=client_id)
            if success:
                latencies.append(ms)
            else:
                print(f"Perf run failed: {out}", flush=True)

            time.sleep(0.1)

        if not latencies:
            self._send_json({"status": "error", "message": "All performance runs failed."})
            return

        avg_ms = sum(latencies) / len(latencies)
        self._send_json(
            {
                "status": "success",
                "type": "performance",
                "stats": {
                    "avg": round(avg_ms, 2),
                    "max": round(max(latencies), 2),
                    "min": round(min(latencies), 2),
                    "samples": len(latencies),
                },
                "message": "Latency Test Complete",
            }
        )

    def _send_json(self, data):
        body = json.dumps(data).encode()
        self.send_response(200)
        self.send_header("Content-type", "application/json")
        self.send_header("Content-length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, fmt, *args):  # noqa: N802
        return


def main() -> None:
    ensure_dirs()
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("", PORT), DemoRequestHandler) as httpd:
        print(f"Demo Agent listening on port {PORT}", flush=True)
        httpd.serve_forever()


if __name__ == "__main__":
    main()
