import json

with open('/opt/loki/web_ui.py', 'r') as f:
    lines = f.readlines()

start_idx, end_idx = -1, -1
for i, line in enumerate(lines):
    if "def do_GET(self):" in line and start_idx == -1:
        start_idx = i
    if "def do_POST(self):" in line and end_idx == -1:
        end_idx = i
        break

api_code = """            def do_GET(self):
                if not self._is_allowed_client():
                    self.send_error(HTTPStatus.NOT_FOUND)
                    return

                # --- NEW API ENDPOINT ---
                if self.path == "/api/status":
                    self.send_response(HTTPStatus.OK)
                    self.send_header("Content-Type", "application/json")
                    self.end_headers()
                    
                    # Hardcoding physical screen stats to test the connection
                    import json
                    status = {
                        "stage": "Egg",
                        "level": "0",
                        "xp": "0",
                        "mood": "Grumpy"
                    }
                    self.wfile.write(json.dumps(status).encode('utf-8'))
                    return

                # --- SERVE WEB UI ---
                if self.path == "/":
                    self.send_response(HTTPStatus.OK)
                    self.send_header("Content-Type", "text/html; charset=utf-8")
                    self.end_headers()

                    with open("/opt/loki/templates/index.html", "r", encoding="utf-8") as f:
                        html_content = f.read()

                    self.wfile.write(html_content.encode('utf-8'))
                    return
                
                self.send_error(HTTPStatus.NOT_FOUND)

"""

if start_idx != -1 and end_idx != -1:
    lines[start_idx:end_idx] = api_code.splitlines(True)
    with open('/opt/loki/web_ui.py', 'w') as f:
        f.writelines(lines)
    print("Success! API endpoint added.")
