#!/bin/bash

echo "Criando portas seriais virtuais ..."
socat -d -d pty,raw,echo=0,link="./build/uart0",unlink-close=1 pty,raw,echo=0,link="./build/uart1",unlink-close=1
