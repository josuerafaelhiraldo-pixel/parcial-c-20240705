#!/usr/bin/env python3
"""Analiza los registros de consola de las pruebas de aceptacion (Hito 5).

Uso:  python analizar_logs.py [carpeta_con_los_logs]      (por defecto: ../../evidencias/hito5)

Lee cada `prueba_NN_*.log` generado por los macros de Tera Term y comprueba las respuestas, los
contadores, la telemetria y la rampa contra el resultado esperado. Solo usa la biblioteca estandar.
Codigo de salida: 0 si todo paso, 1 si alguna comprobacion fallo.
"""
import os
import re
import sys

M32 = 1 << 32
RESULTADOS = []          # (prueba, nombre, ok, detalle)
_actual = ['']


def chk(nombre, cond, detalle=''):
    RESULTADOS.append((_actual[0], nombre, bool(cond), detalle))
    return bool(cond)


# ---------------------------------------------------------------- lectura de registros

def leer(ruta):
    with open(ruta, encoding='utf-8', errors='replace') as f:
        texto = f.read()
    lineas = texto.replace('\r\n', '\n').replace('\r', '\n').split('\n')
    # la telemetria puede aparecer en medio del eco de una orden: se separa en lineas propias
    salida = []
    for l in lineas:
        partes = re.split(r'(?=TEL up=|EVT B1 )', l)
        for p in partes:
            if p != '':
                salida.append(p)
    return salida


def kv(linea):
    return dict(re.findall(r'(\w+)=(\S+)', linea))


def es_resp(l):
    return l.startswith('OK') or l.startswith('ERR ')


def respuestas(lineas):
    """Lineas de respuesta (OK / ERR), sin telemetria ni eventos, en orden."""
    return [l for l in lineas if es_resp(l)]


def estados(lineas):
    out = []
    for l in lineas:
        if l.startswith('OK mode='):
            d = kv(l)
            n = {k: (int(v) if re.fullmatch(r'-?\d+', v) else v) for k, v in d.items()}
            out.append(n)
    return out


def tels(lineas):
    out = []
    for l in lineas:
        if l.startswith('TEL up='):
            d = kv(l)
            out.append({k: (int(v) if re.fullmatch(r'-?\d+', v) else v) for k, v in d.items()})
    return out


def evts(lineas):
    return [l for l in lineas if l.startswith('EVT B1 ')]


def comunes(lineas):
    # un reinicio a mitad del registro seria un banner despues de la primera respuesta
    visto = False
    reinicio = False
    for l in lineas:
        if es_resp(l):
            visto = True
        if visto and l.startswith('=== Parcial'):
            reinicio = True
    chk('Sin reinicios durante la prueba', not reinicio)
    t = tels(lineas)
    if len(t) >= 2:
        difs = [(t[i + 1]['up'] - t[i]['up']) % M32 for i in range(len(t) - 1)]
        return t, difs
    return t, []


# ---------------------------------------------------------------- modelo de la rampa

def n_desde(r, d):
    """Paso (0..159) del ciclo triangular segun valor y direccion."""
    return (r - 10) if d == 'up' else (170 - r)


def rampa(t, inicio):
    n = ((t - inicio) // 20) % 160
    return 10 + n if n <= 80 else 170 - n


def inicio_de(u, r, d):
    return u - 20 * n_desde(r, d)


def dif_circ(a, b, periodo=3200):
    x = (a - b) % periodo
    return x if x <= periodo // 2 else x - periodo


# ---------------------------------------------------------------- pruebas

def t00(lineas):
    e = estados(lineas)
    chk('STATUS responde', len(e) >= 1)
    chk('HELP lista las ordenes y termina en OK',
        any('STREAM ON | OFF' in l for l in lineas) and any(l == 'OK' for l in lineas))
    if e:
        chk('Estado inicial MANUAL, 1000 Hz, 25 %, STREAM OFF',
            e[0]['mode'] == 'MANUAL' and e[0]['freq'] == 1000 and e[0]['duty'] == 25 and e[0]['stream'] == 'OFF')


def t01(lineas):
    t, difs = comunes(lineas)
    e = estados(lineas)
    esp = [(1000, 25), (1000, 0), (1000, 50), (1000, 100), (500, 50), (1000, 50), (2000, 50), (2000, 50)]
    chk('Hay 8 respuestas STATUS', len(e) == 8, 'obtenidas %d' % len(e))
    if len(e) == 8:
        for i, (f, d) in enumerate(esp):
            chk('STATUS %d: %d Hz, duty %d %%' % (i + 1, f, d),
                e[i]['freq'] == f and e[i]['duty'] == d and e[i]['mode'] == 'MANUAL',
                'freq=%s duty=%s' % (e[i]['freq'], e[i]['duty']))
    chk('Sin respuestas ERR', not any(l.startswith('ERR') for l in lineas))
    med = [l for l in lineas if l.startswith('[medida]')]
    chk('Hay 6 medidas', len(med) == 6, 'obtenidas %d' % len(med))
    esp_m = [(1000, 0), (1000, 50), (1000, 100), (500, 50), (1000, 50), (2000, 50)]
    for i, l in enumerate(med[:6]):
        f, d = esp_m[i]
        ok = l.rstrip().endswith('OK') and 'FALLA' not in l
        if d == 0:
            ok = ok and 'sin flancos' in l and 'BAJO' in l
        elif d == 100:
            ok = ok and 'sin flancos' in l and 'ALTO' in l
        else:
            m = re.search(r'f=([\d.]+) Hz \(([+-][\d.]+) %\) duty=([\d.]+) % \(([+-][\d.]+) pp\)', l)
            ok = ok and m is not None and abs(float(m.group(2))) <= 2.0 and abs(float(m.group(4))) <= 2.0
        chk('Medida %d (%d Hz, %d %%)' % (i + 1, f, d), ok, l.strip())
    chk('Telemetria: al menos 15 lineas', len(t) >= 15, 'lineas TEL: %d' % len(t))
    chk('Tick de 1 ms intacto: todas las lineas TEL separadas exactamente 1000 ms durante los cambios de frecuencia',
        difs and all(x == 1000 for x in difs),
        'separaciones distintas de 1000: %s' % [x for x in difs if x != 1000][:5])


def t02(lineas):
    t, difs = comunes(lineas)
    e = estados(lineas)
    chk('Hay 8 respuestas STATUS', len(e) == 8, 'obtenidas %d' % len(e))
    if len(e) != 8:
        return
    resp = respuestas(lineas)
    chk('MODE AUTO responde OK MODE AUTO duty=10', 'OK MODE AUTO duty=10' in resp)
    s0, s1, s2, s3, s4, s5, s6, s7 = e
    chk('Antes de AUTO: MANUAL 25 %', s0['mode'] == 'MANUAL' and s0['duty'] == 25)
    chk('En AUTO: modo AUTO', s1['mode'] == 'AUTO' and s2['mode'] == 'AUTO')
    ini = inicio_de(s1['uptime_ms'], s1['ramp'], s1['dir'])
    # valores de rampa en el tramo previo a la pausa
    pre = [(x['uptime_ms'], x['ramp'], x['dir']) for x in (s1, s2)]
    desv = [abs(dif_circ(inicio_de(u, r, d), ini)) for u, r, d in pre]
    chk('Fase de la rampa constante entre muestras (ciclo de 3,2 s, error maximo %d ms)' % max(desv),
        max(desv) <= 60, 'desviaciones (ms): %s' % desv)
    # telemetria con AUTO antes de la pausa
    tp = [x for x in t if x['mode'] == 'AUTO' and s1['uptime_ms'] <= x['up'] <= s3['uptime_ms']]
    errs = [abs(rampa(x['up'], ini) - x['duty']) for x in tp]
    chk('Duty de la telemetria coincide con la rampa 10-90-10 (max diferencia %s puntos, %d lineas)' %
        (max(errs) if errs else '-', len(errs)), errs and max(errs) <= 3, 'diferencias: %s' % errs)
    vals = [x['duty'] for x in tp] + [s1['ramp'], s2['ramp']]
    chk('La rampa se mantuvo entre 10 y 90 %', vals and min(vals) >= 10 and max(vals) <= 90,
        'minimo %s maximo %s' % (min(vals), max(vals)))
    # pausa
    chk('PAUSA: modo PAUSA y duty 0', s3['mode'] == 'PAUSA' and s3['duty'] == 0 and s4['mode'] == 'PAUSA' and s4['duty'] == 0)
    chk('PAUSA conserva rampa, direccion y duty manual (2,5 s despues no cambian)',
        s3['ramp'] == s4['ramp'] and s3['dir'] == s4['dir'] and s3['manual'] == s4['manual'] == 25,
        'ramp %s/%s dir %s/%s' % (s3['ramp'], s4['ramp'], s3['dir'], s4['dir']))
    tpa = [x for x in t if x['mode'] == 'PAUSA']
    chk('La telemetria sigue en PAUSA con duty 0', len(tpa) >= 2 and all(x['duty'] == 0 for x in tpa),
        'lineas TEL en PAUSA: %d' % len(tpa))
    # reanudacion
    r = [l for l in resp if l.startswith('OK RESUME')]
    m = re.match(r'OK RESUME mode=AUTO duty=(\d+)', r[0]) if r else None
    chk('RESUME responde con el valor guardado', m is not None and int(m.group(1)) == s3['ramp'],
        r[0] if r else 'sin respuesta')
    chk('Tras RESUME: modo AUTO, sin recuperar pasos del tiempo en pausa (diferencia <= 25 puntos)',
        s5['mode'] == 'AUTO' and abs(s5['ramp'] - s3['ramp']) <= 25,
        'guardado %s, 0,1-0,3 s despues %s' % (s3['ramp'], s5['ramp']))
    ini2 = inicio_de(s5['uptime_ms'], s5['ramp'], s5['dir'])
    d6 = abs(rampa(s6['uptime_ms'], ini2) - s6['ramp'])
    tr = [x for x in t if x['mode'] == 'AUTO' and x['up'] >= s5['uptime_ms'] and x['up'] <= s6['uptime_ms']]
    e6 = [abs(rampa(x['up'], ini2) - x['duty']) for x in tr]
    chk('La rampa continua tras RESUME con el mismo ritmo (max diferencia %s puntos)' % max([d6] + e6),
        max([d6] + e6) <= 3)
    chk('Volver a MANUAL restaura el duty manual (25 %)', s7['mode'] == 'MANUAL' and s7['duty'] == 25)
    chk('Telemetria separada 1000 ms', difs and all(x == 1000 for x in difs),
        'distintas de 1000: %s' % [x for x in difs if x != 1000][:5])
    chk('Sin respuestas ERR', not any(l.startswith('ERR') for l in lineas))


ESP03 = [
    ('STATUS', 'OK mode='), ('DUTY 40', 'OK DUTY 40'), ('STATUS', 'OK mode='),
    ('DUTY 20abc', 'ERR ARG'), ('DUTY -1', 'ERR RANGE'), ('DUTY 101', 'ERR RANGE'), ('DUTY', 'ERR ARG'),
    ('DUTY 5 6', 'ERR ARG'), ('DUTY 99999999999999999999', 'ERR RANGE'), ('DUTY 4294967396', 'ERR RANGE'),
    ('FREQ 750', 'ERR RANGE'), ('FREQ', 'ERR ARG'), ('MODE', 'ERR ARG'), ('MODE FOO', 'ERR ARG'),
    ('MODE AUTO X', 'ERR ARG'), ('duty 10', 'ERR COMMAND'), ('FOO', 'ERR COMMAND'), ('STREAM', 'ERR ARG'),
    ('STREAM MAYBE', 'ERR ARG'), ('HELP me', 'ERR ARG'), ('RESUME', 'ERR STATE'), ('STATUS', 'OK mode='),
    ('   DUTY 45   ', 'OK DUTY 45'), ('STATUS', 'OK mode='), ('STATUS + 57 espacios (63 caracteres)', 'OK mode='),
    ('STATUS + 58 espacios (64 caracteres)', 'ERR OVERFLOW'), ('STATUS', 'OK mode='),
    ('100 caracteres X', 'ERR OVERFLOW'), ('STATUS', 'OK mode='),
]


def t03(lineas):
    comunes(lineas)
    resp = respuestas(lineas)
    chk('Numero de respuestas = numero de ordenes (%d)' % len(ESP03), len(resp) == len(ESP03),
        'obtenidas %d' % len(resp))
    for i, (orden, esp) in enumerate(ESP03):
        if i < len(resp):
            chk('%-42s -> %s' % (orden, esp), resp[i].startswith(esp), 'obtenida: %s' % resp[i][:60])
    e = estados(lineas)
    if len(e) >= 7:
        chk('El duty no cambio con ningun rechazo (40 antes y despues de las 19 ordenes invalidas)',
            e[1]['duty'] == 40 and e[2]['duty'] == 40 and e[2]['manual'] == 40 and e[2]['mode'] == 'MANUAL')
        f = e[-1]
        chk('Contadores finales: err_cmd=2 err_arg=10 err_range=5 err_state=1 err_ovf=2',
            (f['err_cmd'], f['err_arg'], f['err_range'], f['err_state'], f['err_ovf']) == (2, 10, 5, 1, 2),
            'obtenidos %s %s %s %s %s' % (f['err_cmd'], f['err_arg'], f['err_range'], f['err_state'], f['err_ovf']))
        chk('Sin bytes perdidos ni errores de UART', f['rx_lost'] == 0 and f['uart_err'] == 0 and f['tx_drop'] == 0)
        chk('El duty final es el de la ultima orden valida (45)', f['duty'] == 45)


def t04(lineas):
    comunes(lineas)
    resp = respuestas(lineas)
    esp = ['OK mode='] + ['OK DUTY %d' % n for n in
                          list(range(30, 39)) + [50, 51, 52, 60, 61, 62, 70, 71, 72]] + ['OK mode=']
    chk('Una respuesta por orden: %d respuestas (esperadas %d)' % (len(resp), len(esp)), len(resp) == len(esp))
    ok = len(resp) == len(esp) and all(r.startswith(x) for r, x in zip(resp, esp))
    chk('Cada respuesta corresponde a su orden y en orden (CR, LF y CRLF, solos y en rafaga)', ok)
    chk('Ninguna respuesta de error', not any(l.startswith('ERR') for l in lineas))
    e = estados(lineas)
    if len(e) == 2:
        chk('cmd_ok aumento exactamente en las 18 ordenes (+ el STATUS inicial)',
            e[1]['cmd_ok'] - e[0]['cmd_ok'] == 19, 'de %s a %s' % (e[0]['cmd_ok'], e[1]['cmd_ok']))
        chk('Sin errores de ordenes ni de UART',
            e[1]['err_cmd'] == e[1]['err_arg'] == e[1]['err_range'] == e[1]['err_state'] == e[1]['err_ovf'] == 0
            and e[1]['rx_lost'] == e[1]['uart_err'] == 0)
        chk('Duty final 72', e[1]['duty'] == 72)


def t05(lineas):
    t, difs = comunes(lineas)
    e = estados(lineas)
    resp = respuestas(lineas)
    chk('Sin respuestas ERR bajo carga', not any(l.startswith('ERR') for l in lineas),
        '%d lineas ERR' % sum(1 for l in lineas if l.startswith('ERR')))
    n500 = sum(1 for r in resp if r == 'OK FREQ 500')
    n2000 = sum(1 for r in resp if r == 'OK FREQ 2000')
    chk('100 respuestas OK FREQ 500 y 100 OK FREQ 2000', n500 == 100 and n2000 == 100, '%d y %d' % (n500, n2000))
    chk('Hay 103 respuestas STATUS (1 + 100 + 2)', len(e) == 103, 'obtenidas %d' % len(e))
    if len(e) >= 4:
        f = e[-1]
        antes = [r for r in resp[:-1]]   # respuestas OK antes del ultimo STATUS
        okc = sum(1 for r in antes if r.startswith('OK'))
        chk('cmd_ok del ultimo STATUS = respuestas OK recibidas antes (ninguna orden perdida)',
            f['cmd_ok'] == okc, 'cmd_ok=%s respuestas=%d' % (f['cmd_ok'], okc))
        chk('Contadores de error en 0 (ordenes, UART, bytes perdidos, cola TX)',
            f['err_cmd'] == f['err_arg'] == f['err_range'] == f['err_state'] == f['err_ovf'] == 0 and
            f['rx_lost'] == f['uart_err'] == f['tx_drop'] == 0,
            'err_ovf=%s rx_lost=%s uart_err=%s tx_drop=%s' % (f['err_ovf'], f['rx_lost'], f['uart_err'], f['tx_drop']))
        ups = [x['uptime_ms'] for x in e]
        chk('El uptime siempre crece (sin reinicios)', all(b > a for a, b in zip(ups, ups[1:])))
        if len(e) >= 102:
            dur = (e[101]['uptime_ms'] - e[1]['uptime_ms'] - 1500) / 1000.0
            chk('Ritmo logrado: %.1f ordenes por segundo (300 ordenes en %.1f s)' % (300 / dur, dur),
                300 / dur >= 7.0, 'el enunciado pide unas 10 por segundo durante 30 s')
        ramps = set(x['ramp'] for x in e if x['mode'] == 'AUTO')
        chk('La rampa de AUTO siguio funcionando bajo carga (%d valores distintos en los STATUS)' % len(ramps),
            len(ramps) >= 10)
    chk('Telemetria: %d lineas separadas exactamente 1000 ms' % len(t), len(t) >= 28 and difs and
        all(x == 1000 for x in difs), 'distintas de 1000: %s' % [x for x in difs if x != 1000][:5])
    ev = evts(lineas)
    chk('Eventos de B1 durante la carga: %d' % len(ev), len(ev) >= 1,
        'se esperaban unas 6 pulsaciones cortas; ' + ('; '.join(x[:44] for x in ev[:8])))
    chk('Todos los eventos de B1 son pulsaciones cortas validas (short ok)',
        ev and all(x.startswith('EVT B1 short ok') for x in ev))


def t06(lineas):
    comunes(lineas)
    resp = respuestas(lineas)
    esp = ['OK mode=', 'OK DIAG STALL 3000', 'ERR OVERFLOW', 'OK mode=', 'OK DUTY 44', 'OK mode=']
    chk('Secuencia de respuestas: STATUS, STALL, ERR OVERFLOW, STATUS, DUTY 44, STATUS',
        len(resp) == len(esp) and all(r.startswith(x) for r, x in zip(resp, esp)),
        ' | '.join(r[:30] for r in resp))
    e = estados(lineas)
    if len(e) == 3:
        chk('Antes: sin errores ni bytes perdidos', e[0]['rx_lost'] == 0 and e[0]['err_ovf'] == 0)
        chk('Despues: error contabilizado (err_ovf=%s, rx_lost=%s bytes perdidos)' % (e[1]['err_ovf'], e[1]['rx_lost']),
            e[1]['err_ovf'] >= 1 and (e[1]['rx_lost'] + e[1]['uart_err']) > 0)
        chk('Recuperacion: la orden siguiente (DUTY 44) se ejecuta y el STATUS final lo confirma',
            e[2]['duty'] == 44 and e[2]['mode'] == 'MANUAL')
        chk('Ningun fragmento se ejecuto: el duty seguia en 25 % tras el desbordamiento', e[1]['duty'] == 25)


def t07(lineas):
    t, difs = comunes(lineas)
    e = estados(lineas)
    chk('Hay 3 respuestas STATUS', len(e) == 3, 'obtenidas %d' % len(e))
    resp = respuestas(lineas)
    w = [r for r in resp if r.startswith('OK DIAG WRAP')]
    chk('DIAG WRAP responde con el contador a 2048 ms de UINT32_MAX', bool(w), w[0] if w else 'sin respuesta')
    antes = [x for x in t if x['up'] > M32 // 2]
    despues = [x for x in t if x['up'] < M32 // 2]
    chk('La telemetria cruzo el desbordamiento (hay valores cerca de 4294967296 y otros pequenos despues)',
        len(antes) >= 1 and len(despues) >= 3, 'antes %d, despues %d' % (len(antes), len(despues)))
    chk('Telemetria separada exactamente 1000 ms, tambien al dar la vuelta', difs and all(x == 1000 for x in difs),
        'distintas de 1000: %s' % [x for x in difs if x != 1000][:5])
    if len(e) == 3:
        s1 = e[1]
        chk('Tras el desbordamiento sigue en AUTO con la rampa en marcha', s1['mode'] == 'AUTO' and 10 <= s1['ramp'] <= 90)
        chk('El uptime de STATUS dio la vuelta (valor pequeno)', s1['uptime_ms'] < 20000, 'uptime_ms=%s' % s1['uptime_ms'])
        ini = inicio_de(s1['uptime_ms'], s1['ramp'], s1['dir'])
        pos = [x for x in t if x['up'] < M32 // 2 and x['mode'] == 'AUTO' and x['up'] < s1['uptime_ms']]
        errs = [abs(rampa(x['up'], ini) - x['duty']) for x in pos]
        chk('La rampa sigue el ritmo de 20 ms por punto tras dar la vuelta (max diferencia %s)' %
            (max(errs) if errs else '-'), errs and max(errs) <= 3, 'diferencias: %s' % errs)
        chk('Volver a MANUAL restaura el duty manual', e[2]['mode'] == 'MANUAL' and e[2]['duty'] == 25)
    chk('Sin respuestas ERR', not any(l.startswith('ERR') for l in lineas))


PRUEBAS = [
    ('prueba_00_humo', 'Humo (macro y registro)', t00),
    ('prueba_01_duty_freq', 'DUTY 0/50/100 y FREQ 500/1000/2000', t01),
    ('prueba_02_auto_pausa', 'AUTO 3,2 s, PAUSA y RESUME', t02),
    ('prueba_03_errores_y_lineas', 'Errores y limites de linea', t03),
    ('prueba_04_terminadores', 'CR, LF y CRLF', t04),
    ('prueba_05_carga_10_por_segundo', 'STREAM ON y 10 ordenes por segundo', t05),
    ('prueba_06_overflow_rx', 'Desbordamiento de RX', t06),
    ('prueba_07_desbordamiento_uint32', 'Tiempo cerca de UINT32_MAX', t07),
]


def main():
    base = os.path.dirname(os.path.abspath(__file__))
    carpeta = sys.argv[1] if len(sys.argv) > 1 else os.path.join(base, '..', '..', 'evidencias', 'hito5')
    faltan = []
    for arch, titulo, fn in PRUEBAS:
        ruta = os.path.join(carpeta, arch + '.log')
        if not os.path.exists(ruta):
            faltan.append(arch)
            continue
        _actual[0] = arch
        print('\n== %s: %s' % (arch, titulo))
        try:
            fn(leer(ruta))
        except Exception as ex:                      # un registro dañado no debe ocultar los demas
            chk('El registro se pudo analizar', False, repr(ex))
        for p, n, ok, d in [r for r in RESULTADOS if r[0] == arch]:
            print('  [%s] %s%s' % ('OK ' if ok else 'FALLA', n, '' if ok or not d else '\n         -> ' + d))
    for a in faltan:
        print('\n(sin registro: %s)' % a)
    total = len(RESULTADOS)
    bien = sum(1 for r in RESULTADOS if r[2])
    print('\nResumen: %d de %d comprobaciones correctas' % (bien, total))
    for arch, titulo, _ in PRUEBAS:
        rs = [r for r in RESULTADOS if r[0] == arch]
        if rs:
            print('  %-34s %s (%d/%d)' % (arch, 'OK' if all(r[2] for r in rs) else 'REVISAR',
                                         sum(1 for r in rs if r[2]), len(rs)))
    return 0 if (bien == total and not faltan) else 1


if __name__ == '__main__':
    sys.exit(main())
