#!/bin/bash
cd tests
./build_tests.py
chmod +x ./run_tests.py
./run_tests.py
cd ..
