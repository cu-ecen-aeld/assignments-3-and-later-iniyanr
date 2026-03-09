#!/bin/sh
writefile=$1
writestr=$2

if [ $# -ne 2 ]; then
	echo "Error: Two arguments are required"
	exit 1
fi

writefile_dir=$(dirname "$writefile")
if [ ! -d "$writefile_dir" ]; then
    echo "Directory does not exist, creating it..."
    mkdir -p "$writefile_dir"  
    if [ $? -ne 0 ]; then
        echo "Error: Failed to create directory"
        exit 1
    fi
fi

if ! echo "$writestr" > "$writefile";then
	echo "Error: Not able to create file"
	exit 1
fi
