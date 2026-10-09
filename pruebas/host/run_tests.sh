#!/bin/sh
# Compila y ejecuta las pruebas en el PC (gcc). Uso:  sh run_tests.sh
# Estas pruebas verifican la LOGICA (buffers, cola TX, teclas). No sustituyen
# las pruebas en la placa real.
set -e
cd "$(dirname "$0")"
INC=../../Parcial_20240705/Inc
SRC=../../Parcial_20240705/Src
CF="-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I. -I$INC"
gcc $CF test_ringbuf.c $SRC/ringbuf.c -o t_ringbuf && ./t_ringbuf
gcc $CF test_console.c hal_sim.c $SRC/console.c $SRC/ringbuf.c -o t_console && ./t_console
gcc $CF test_app.c hal_sim.c $SRC/app.c $SRC/console.c $SRC/ringbuf.c -o t_app && ./t_app
rm -f t_ringbuf t_console t_app
