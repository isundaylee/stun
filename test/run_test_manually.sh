#!/bin/bash
# 
# Helper script to run test without buck until buck supports Python test filtering

buck build //test:dockerfile //:main#release

DOCKERFILE_PATH="buck-out/gen/test/dockerfile/dockerfile" \
    BINARY_PATH="buck-out/gen/main#release" \
    pytest test/basic.py --numprocesses=12 -k $1
