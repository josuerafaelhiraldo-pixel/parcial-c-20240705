#!/bin/sh
# Compila y ejecuta las pruebas en el PC (gcc). Uso:  sh run_tests.sh
# Estas pruebas verifican la LOGICA (buffers, cola TX, tick, boton, PWM, medidor, lineas, parser,
# maquina de estados, protocolo de ordenes e integracion).
# No sustituyen las pruebas en la placa real.
# Termina con error (codigo distinto de 0) si algo no compila o alguna prueba falla.
set -e
cd "$(dirname "$0")"
INC=../../Parcial_20240705/Inc
SRC=../../Parcial_20240705/Src
CF="-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -I. -I$INC"

gcc $CF test_ringbuf.c $SRC/ringbuf.c -o t_ringbuf
./t_ringbuf
gcc $CF test_console.c hal_sim.c $SRC/console.c $SRC/ringbuf.c -o t_console
./t_console
gcc $CF test_tick.c hal_sim.c $SRC/tick.c -o t_tick
./t_tick
gcc $CF test_button.c hal_sim.c $SRC/button.c -o t_button
./t_button
gcc $CF test_pwm.c hal_sim.c $SRC/pwm.c -o t_pwm
./t_pwm
gcc $CF test_meter.c hal_sim.c $SRC/pwm.c $SRC/pwm_meter.c -o t_meter
./t_meter
gcc $CF test_check.c hal_sim.c $SRC/pwm.c $SRC/pwm_meter.c $SRC/pwm_check.c $SRC/console.c $SRC/ringbuf.c $SRC/tick.c -o t_check
./t_check
gcc $CF test_line.c $SRC/line.c -o t_line
./t_line
gcc $CF test_cmd.c hal_sim.c $SRC/cmd.c $SRC/pwm.c -o t_cmd
./t_cmd
gcc $CF test_ctrl.c hal_sim.c $SRC/ctrl.c $SRC/pwm.c -o t_ctrl
./t_ctrl
ALL="hal_sim.c $SRC/app.c $SRC/shell.c $SRC/line.c $SRC/cmd.c $SRC/ctrl.c $SRC/console.c $SRC/ringbuf.c $SRC/tick.c $SRC/button.c $SRC/pwm.c $SRC/pwm_meter.c $SRC/pwm_check.c"
gcc $CF test_protocol.c $ALL -o t_protocol
./t_protocol
gcc $CF test_app.c $ALL -o t_app
./t_app

rm -f t_ringbuf t_console t_tick t_button t_pwm t_meter t_check t_line t_cmd t_ctrl t_protocol t_app
echo "TODAS LAS PRUEBAS EN PC: OK"
