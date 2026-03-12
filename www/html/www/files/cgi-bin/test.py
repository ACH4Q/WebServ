#!/usr/bin/python3
import sys
import os

# 1. THE HEADER: Crucial for the browser to understand the output
# We use \r\n\r\n to signal the end of the HTTP headers
print("Content-Type: text/html\r\n\r\n", end="")

# 2. THE BODY: The actual HTML that will appear on your dashboard
print("<html>")
print("<body style='font-family: monospace; color: #a6e3a1;'>") # Success Green
print("<h2>🚀 CGI CHEF: MISSION ACCOMPLISHED</h2>")

# 3. GET TEST: Extract data from the URL (Query String)
method = os.environ.get("REQUEST_METHOD", "UNKNOWN")
query = os.environ.get("QUERY_STRING", "Empty")

print(f"<p><b>Method:</b> {method}</p>")
print(f"<p><b>Query String:</b> {query}</p>")

# 4. POST TEST: Extract data from the Standard Input (The Pipe)
if method == "POST":
    # sys.stdin.read() grabs the data your C++ Waiter sent through the pipe
    body = sys.stdin.read()
    print("<p><b>Standard Input (POST Data):</b></p>")
    print(f"<pre style='background: #313244; padding: 10px;'>{body}</pre>")

print("</body>")
print("</html>")