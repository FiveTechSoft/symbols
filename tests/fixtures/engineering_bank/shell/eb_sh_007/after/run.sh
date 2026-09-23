#!/bin/sh
if [ -f helper.sh ]; then
    sh helper.sh
else
    exit 2
fi
