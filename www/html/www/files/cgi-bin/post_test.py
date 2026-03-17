#!/usr/bin/python3
import sys
import os
import urllib.parse

# 1. Mandatory HTTP Header
print("Content-Type: text/html; charset=utf-8\r\n\r\n", end="")

# 2. Setup the UI output
print("<div style='font-family: monospace; color: #cdd6f4;'>")
print("<h2 style='color: #89b4fa;'>🚀 CGI POST Receiver Triggered</h2>")

method = os.environ.get("REQUEST_METHOD", "UNKNOWN")

if method == "POST":
    # 3. Read the exact bytes your C++ server piped into standard input
    raw_body = sys.stdin.read()
    
    print("<h3>1. Raw Body Received from C++:</h3>")
    print(f"<pre style='background: #11111b; padding: 10px; border-left: 3px solid #f38ba8;'>{raw_body}</pre>")

    # 4. Parse the 'message' parameter
    parsed_data = urllib.parse.parse_qs(raw_body)
    
    if 'message' in parsed_data:
        secret_param = parsed_data['message'][0]
        print("<h3>2. Parameter Extracted:</h3>")
        print(f"<p style='font-size: 18px;'>Message = <b style='color: #a6e3a1;'>{secret_param}</b></p>")
    else:
        print("<p style='color: #f38ba8;'>Error: 'message' parameter not found in the body.</p>")
else:
    print(f"<p style='color: #f38ba8;'>Wait, this is a {method} request! Please send a POST request.</p>")

print("</div>")