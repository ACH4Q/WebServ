#!/bin/bash

echo "Content-Type: text/html"
echo ""

name=$(echo "$QUERY_STRING" | cut -d '=' -f2)

echo "<html>"
echo "<body>"
echo "<h1>Hello $name</h1>"
echo "</body>"
echo "</html>"