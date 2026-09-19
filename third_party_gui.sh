#!/bin/bash
cd "$(dirname "$0")"
python3 python/third_party_gui.py "$@"
if [ $? -ne 0 ]; then
    read -p "Press any key to continue..."
fi
