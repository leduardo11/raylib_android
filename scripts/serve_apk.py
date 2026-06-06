#!/usr/bin/env python3
# ── serve_apk.py ──────────────────────────────────────────────────────────
# HTTP server serving the latest APK + crash report upload.
# Usage: python3 scripts/serve_apk.py [--port PORT]

import http.server
import os
import sys
import time
import socket
from datetime import datetime

PORT = 8080

for i, arg in enumerate(sys.argv):
    if arg == "--port" and i + 1 < len(sys.argv):
        PORT = int(sys.argv[i + 1])
    elif arg.startswith("--port="):
        PORT = int(arg.split("=", 1)[1])

PORT = int(os.environ.get("PORT", str(PORT)))

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, ".."))

# Find APK — check artifacts/ first, then fallback to old build/ location
APK_DIR = next(
    (
        d
        for d in [
            os.path.join(PROJECT_DIR, "artifacts", "outputs", "apk", "debug"),
            os.path.join(PROJECT_DIR, "src", "app", "build", "outputs", "apk", "debug"),
        ]
        if os.path.isdir(d)
    ),
    None,
)

CRASH_DIR = "/tmp/raylib_android_crashes"
os.makedirs(CRASH_DIR, exist_ok=True)


def get_primary_ip():
    """Returns the primary LAN IP used for outbound connections."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    try:
        # Doesn't actually send packets.
        s.connect(("8.8.8.8", 80))
        return s.getsockname()[0]
    except Exception:
        return "127.0.0.1"
    finally:
        s.close()


def get_all_ips():
    """Returns all non-loopback IPv4 addresses found on the host."""
    ips = set()

    try:
        hostname = socket.gethostname()

        for ip in socket.gethostbyname_ex(hostname)[2]:
            if not ip.startswith("127."):
                ips.add(ip)
    except Exception:
        pass

    try:
        ips.add(get_primary_ip())
    except Exception:
        pass

    return sorted(ips)


class Handler(http.server.SimpleHTTPRequestHandler):
    def do_POST(self):
        if self.path == "/upload":
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length)

            path = os.path.join(
                CRASH_DIR,
                f"crash_{int(time.time())}.txt",
            )

            with open(path, "wb") as f:
                f.write(body)

            self.send_response(200)
            self.end_headers()
            self.wfile.write(b"ok")

            print(f"\n[!] Crash report saved: {path}")
            print(body.decode("utf-8", errors="replace")[:2000])

            return

        self.send_response(404)
        self.end_headers()

    def log_message(self, fmt, *args):
        msg = "  " + (fmt % args if args else fmt)
        print(msg)

    def translate_path(self, path):
        if path.startswith("/crashes"):
            return CRASH_DIR

        return super().translate_path(path)

    def do_GET(self):
        if self.path == "/":
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()

            apk = (
                next(
                    (
                        f
                        for f in os.listdir(APK_DIR)
                        if f.endswith(".apk")
                    ),
                    "raylib_android-debug.apk",
                )
                if APK_DIR
                else "raylib_android-debug.apk"
            )

            apk_path = os.path.join(APK_DIR, apk) if APK_DIR else ""

            size = (
                os.path.getsize(apk_path)
                if apk_path and os.path.exists(apk_path)
                else 0
            )

            host = self.headers.get(
                "Host",
                f"{get_primary_ip()}:{PORT}",
            )

            self.wfile.write(
                f"""<!DOCTYPE html>
<html>
<head>
<title>raylib Android</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{{font-family:monospace;background:#111;color:#eee;padding:2em;max-width:700px;margin:auto}}
a{{color:#4af;display:block;padding:.8em;margin:.5em 0;background:#222;border-radius:6px;text-align:center;font-size:1.2em;text-decoration:none}}
a:hover{{background:#333}}
h1{{color:#fa4;text-align:center}}
h2{{color:#fa4;margin-top:2em}}
.info{{color:#888;font-size:.9em;text-align:center;margin:1em 0}}
code{{background:#333;padding:.2em .4em;border-radius:3px}}
pre{{background:#000;padding:1em;border-radius:4px;overflow-x:auto;color:#aaa;white-space:pre-wrap}}
.box{{background:#1a1a2e;border:1px solid #333;border-radius:6px;padding:1em;margin:1em 0}}
</style>
</head>
<body>

<h1>raylib Android</h1>

<div class="info">
{datetime.now().strftime('%Y-%m-%d %H:%M')}
&middot;
{size / 1024 / 1024:.0f}MB
</div>

<a href="{apk}">[Download] APK</a>
<a href="/crashes/">[View] Crash Logs</a>

<h2>After a Crash</h2>

<div class="box">
<p><strong>Option A - Upload from phone browser:</strong></p>

<form action="/upload"
      method="post"
      enctype="multipart/form-data"
      style="margin:1em 0">

<input type="file"
       name="file"
       accept=".txt,.log"
       style="color:#eee;background:#333;border:1px solid #555;padding:.5em;border-radius:4px;width:100%;box-sizing:border-box">

<button type="submit"
        style="margin-top:.5em;background:#4af;color:#000;border:none;padding:.5em 1em;border-radius:4px;font-weight:bold;cursor:pointer;width:100%">
Upload Crash Log
</button>

</form>
</div>

<div class="box">
<p><strong>Option B - Use curl from Terminal:</strong></p>
<pre>curl -X POST -F "file=@crash_log.txt" http://{host}/upload</pre>
</div>

<div class="box">
<p><strong>Option C - View in Logcat Reader app:</strong></p>
<p>Install a Logcat Reader app from Play Store, look for tag <code>raylib_android</code>.</p>
</div>

</body>
</html>""".encode()
            )

            return

        super().do_GET()


if __name__ == "__main__":
    if not APK_DIR:
        print("[!] APK directory not found. Build first: ./scripts/build_apk.sh")
        sys.exit(1)

    os.chdir(APK_DIR)

    httpd = http.server.HTTPServer(("0.0.0.0", PORT), Handler)

    primary_ip = get_primary_ip()
    all_ips = get_all_ips()

    print("=" * 60)
    print(" raylib Android APK Server")
    print("=" * 60)

    print(f"  Primary LAN IP: {primary_ip}")
    print()

    print("  Available URLs:")

    for ip in all_ips:
        print(f"    http://{ip}:{PORT}/")

    print()

    for apk in os.listdir("."):
        if apk.endswith(".apk"):
            print(f"  APK:   http://{primary_ip}:{PORT}/{apk}")

    print(f"  Serve: http://{primary_ip}:{PORT}/")
    print("  Crash upload: POST /upload")
    print("  Press Ctrl+C to stop")
    print("=" * 60)

    httpd.serve_forever()