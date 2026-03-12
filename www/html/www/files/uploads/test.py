import sys
import os

print("Content-Type: text/html\r\n\r\n", end="")
print("<h1>IT WORKS! YOU ARE A GENIUS!</h1>")

method = os.environ.get("REQUEST_METHOD", "UNKNOWN")
print("<p>The button you clicked was: " + method + "</p>")