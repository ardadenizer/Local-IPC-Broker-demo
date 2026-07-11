#!/usr/bin/env python3
import json
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

HOST = "127.0.0.1"
PORT = 8081
PATH = "/mock-cloud/upload"


class MockCloudHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        if self.path != PATH:
            self.send_response(404)
            self.end_headers()
            self.wfile.write(b"not found")
            return

        length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(length)

        try:
            body = json.loads(raw_body.decode("utf-8")) if raw_body else {}
        except json.JSONDecodeError:
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b"invalid json")
            return

        print(f"[mock-cloud] upload received: {json.dumps(body, separators=(',', ':'))}", flush=True)

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(b'{"status":"ok"}')

    def log_message(self, _format, *_args):
        return


def main():
    server = ThreadingHTTPServer((HOST, PORT), MockCloudHandler)
    print(f"[mock-cloud] listening on http://{HOST}:{PORT}{PATH}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
