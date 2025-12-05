import subprocess
import time
import os
import shutil
import hashlib
import random
import sys
import signal

# Configuration
SERVER_BIN = "./build/bin/dfs-server"
CLIENT_BIN = "./build/bin/dfs-client"
MOUNT_ROOT = "/tmp/dfs_test_mounts"
SERVER_ADDR = "0.0.0.0:50403"

class DFSCluster:
    def __init__(self, num_clients=2):
        self.server_process = None
        self.client_processes = []
        self.num_clients = num_clients
        self.mount_points = []

    def setup(self):
        # Cleanup previous runs
        if os.path.exists(MOUNT_ROOT):
            shutil.rmtree(MOUNT_ROOT)
        os.makedirs(MOUNT_ROOT)
        
        # Create directories
        self.server_mount = os.path.join(MOUNT_ROOT, "server")
        os.makedirs(self.server_mount)
        
        for i in range(self.num_clients):
            client_mount = os.path.join(MOUNT_ROOT, f"client_{i}")
            os.makedirs(client_mount)
            self.mount_points.append(client_mount)

    def start_server(self):
        print(f"[Cluster] Starting Server at {self.server_mount}")
        env = os.environ.copy()
        env["DFS_VERIFY_LOGS"] = "1"
        
        self.server_log = open(f"server.log", "w")
        self.server_process = subprocess.Popen(
            [SERVER_BIN, "-m", self.server_mount, "-a", SERVER_ADDR, "-d", "3"],
            env=env,
            stdout=self.server_log,
            stderr=subprocess.STDOUT
        )
        time.sleep(1) # Wait for startup

    def start_clients(self):
        for i, mount in enumerate(self.mount_points):
            print(f"[Cluster] Starting Client {i} at {mount}")
            log_file = open(f"client_{i}.log", "w")
            proc = subprocess.Popen(
                [CLIENT_BIN, "-m", mount, "-a", SERVER_ADDR, "-d", "3", "mount"],
                stdout=log_file,
                stderr=subprocess.STDOUT
            )
            self.client_processes.append(proc)
            # self.client_logs.append(log_file) # Keep ref if needed
        time.sleep(2) # Wait for connections

    def stop_all(self):
        print("[Cluster] Stopping all processes...")
        for p in self.client_processes:
            if p.poll() is None:
                p.terminate()
                p.wait()
        
        if self.server_process and self.server_process.poll() is None:
            self.server_process.terminate()
            self.server_process.wait()

    def get_client_file_path(self, client_idx, filename):
        return os.path.join(self.mount_points[client_idx], filename)

    def get_server_file_path(self, filename):
        return os.path.join(self.server_mount, filename)

def calculate_md5(filepath):
    if not os.path.exists(filepath):
        return None
    hash_md5 = hashlib.md5()
    with open(filepath, "rb") as f:
        for chunk in iter(lambda: f.read(4096), b""):
            hash_md5.update(chunk)
    return hash_md5.hexdigest()

def test_basic_sync():
    print("\n--- Test: Basic Synchronization ---")
    cluster = DFSCluster(num_clients=2)
    cluster.setup()
    
    try:
        cluster.start_server()
        cluster.start_clients()

        # Client 0 writes a file
        file_path_0 = cluster.get_client_file_path(0, "test_basic.txt")
        content = b"Hello Distributed World"
        with open(file_path_0, "wb") as f:
            f.write(content)
        
        print("[Test] Client 0 wrote file.")
        
        # Wait for sync (inotify + grpc latency)
        # We poll for existence on Client 1
        file_path_1 = cluster.get_client_file_path(1, "test_basic.txt")
        
        synced = False
        for _ in range(20): # Max 4 seconds
            if os.path.exists(file_path_1):
                synced = True
                break
            time.sleep(0.2)
            
        if not synced:
            print("❌ FAILURE: File did not appear on Client 1")
            print("--- Server Log ---")
            with open("server.log", "r") as f: print(f.read())
            print("--- Client 0 Log ---")
            with open("client_0.log", "r") as f: print(f.read())
            print("--- Client 1 Log ---")
            with open("client_1.log", "r") as f: print(f.read())
            return False

        # Check Content
        if calculate_md5(file_path_1) == calculate_md5(file_path_0):
            print("✅ SUCCESS: File content matches.")
            return True
        else:
            print("❌ FAILURE: File content mismatch.")
            return False

    finally:
        cluster.stop_all()

def test_lock_contention():
    print("\n--- Test: Lock Contention (Torture) ---")
    # 3 Clients trying to write to the same file continuously
    cluster = DFSCluster(num_clients=3)
    cluster.setup()
    
    try:
        cluster.start_server()
        cluster.start_clients()
        
        filename = "contention.txt"
        iterations = 20 # Write attempts per client
        
        # Create initial file
        initial_path = cluster.get_client_file_path(0, filename)
        with open(initial_path, "w") as f:
            f.write("Initial")
        time.sleep(1)

        # We simulate concurrent writes by effectively 'touching' and writing 
        # to the file in a tight loop from the Python script (which acts as the User)
        
        # Since we can't easily multi-thread the Python *test runner* to simulate 
        # user actions without more code, we'll do a randomized sequential hammer
        # which is still fast enough to trigger race conditions in the async delivery.
        
        for i in range(iterations):
            client_idx = random.randint(0, 2)
            path = cluster.get_client_file_path(client_idx, filename)
            
            content = f"Update_{i}_by_Client_{client_idx}"
            # print(f"Operation {i}: Client {client_idx} writing...")
            try:
                with open(path, "w") as f:
                    f.write(content)
            except Exception as e:
                print(f"Write failed (expected potentially): {e}")
            
            # Don't sleep much, provoke conflict
            time.sleep(random.uniform(0.01, 0.05))

        print("[Test] Hammering complete. Waiting for quiescence...")
        time.sleep(3)

        # Verification: All clients + Server must have the SAME content
        # (LWW means we don't know *which* one won, but they must agree) 
        
        hashes = []
        server_hash = calculate_md5(cluster.get_server_file_path(filename))
        hashes.append(server_hash)
        
        for i in range(3):
            h = calculate_md5(cluster.get_client_file_path(i, filename))
            hashes.append(h)
            print(f"Client {i} Hash: {h}")
        
        print(f"Server   Hash: {server_hash}")

        if len(set(hashes)) == 1 and hashes[0] is not None:
             print("✅ SUCCESS: Convergence achieved.")
             return True
        else:
             print("❌ FAILURE: Divergence detected.")
             return False

    finally:
        cluster.stop_all()

if __name__ == "__main__":
    print("Starting DFS Torture Suite...")
    
    if not test_basic_sync():
        sys.exit(1)
        
    if not test_lock_contention():
        sys.exit(1)
        
    print("\n🎉 ALL TESTS PASSED")
