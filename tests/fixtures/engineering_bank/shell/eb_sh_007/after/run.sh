#!/bin/sh
if [ -f helper.sh ]; then
    sh helper.sh
else
    exit 3
fi
