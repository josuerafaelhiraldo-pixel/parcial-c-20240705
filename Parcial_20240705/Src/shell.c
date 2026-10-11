/* shell.c - Interprete de ordenes y telemetria. Ver shell.h. */
#include "shell.h"
#include "app_config.h"
#include "cmd.h"
#include "console.h"
#include "ctrl.h"
#include "line.h"
#include "pwm.h"
#include "pwm_check.h"
#include "tick.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define RESP_MAX  384U      /* respuesta formateada mas larga (STATUS) */

static const char k_help[] =
    "Ordenes (en MAYUSCULAS, una por linea, hasta 63 caracteres; terminador CR, LF o CRLF):\r\n"
    "  HELP              esta lista\r\n"
    "  STATUS            modo, frecuencia, duty, uptime, contadores y errores\r\n"
    "  MODE MANUAL       modo MANUAL: restaura el ultimo duty manual\r\n"
    "  MODE AUTO         modo AUTO: rampa 10..90 % desde 10 %, subiendo\r\n"
    "  DUTY n            duty manual, entero 0..100 (solo en MANUAL)\r\n"
    "  FREQ n            frecuencia del PWM: 500, 1000 o 2000 Hz (MANUAL o AUTO)\r\n"
    "  PAUSE             pausa (PWM al 0 %); si ya esta en pausa responde OK\r\n"
    "  RESUME            reanuda el estado previo (fuera de PAUSA: ERR STATE)\r\n"
    "  STREAM ON | OFF   telemetria TEL cada 1000 ms (inicialmente OFF)\r\n"
#if APP_DIAG_ENABLED
    "Diagnostico (no es parte del protocolo): DIAG MEASURE | DIAG PWMALL | DIAG STALL n | DIAG WRAP\r\n"
#endif
    "B1: pulsacion corta = MANUAL <-> AUTO; larga (1,5 s) = entrar o salir de PAUSA\r\n"
    "OK\r\n";
_Static_assert(sizeof(k_help) < APP_TX_BUF_SIZE, "HELP debe caber en la cola TX");

typedef enum { E_COMMAND = 0, E_ARG, E_RANGE, E_STATE, E_OVERFLOW, E_COUNT } ErrKind;
static const char *const k_errText[E_COUNT] = {
    "ERR COMMAND\r\n", "ERR ARG\r\n", "ERR RANGE\r\n", "ERR STATE\r\n", "ERR OVERFLOW\r\n"
};

static LineReader  s_lr;
static uint32_t    s_faultSeen;          /* ultimo Console_RxFaultCount() atendido */
static bool        s_stream;
static uint32_t    s_telLast;
static bool        s_skipTel;            /* DIAG WRAP: no emitir telemetria con la hora vieja */
static bool        s_stall;              /* DIAG STALL: main no lee RX durante un rato */
static uint32_t    s_stallStart, s_stallMs;
static uint32_t    s_cmdOk, s_err[E_COUNT], s_rxFaultDiscards;

static char        s_buf[RESP_MAX];      /* respuesta formateada */
static const char *s_msg;                /* respuesta pendiente de envio */
static uint16_t    s_msgLen;

/* ---------- salida ---------- */

/* Envia la respuesta pendiente si cabe entera en la cola TX. true = ya no queda nada pendiente. */
static bool flush_pending(void)
{
    if (s_msgLen == 0U) {
        return true;
    }
    if (Console_TxFree() < s_msgLen) {
        return false;               /* espera: no se pierde ni se corta la respuesta */
    }
    (void)Console_WriteBytes((const uint8_t *)s_msg, s_msgLen);
    s_msgLen = 0U;
    return true;
}

static void reply(const char *p, uint16_t n)
{
    s_msg    = p;
    s_msgLen = n;
    (void)flush_pending();
}

static void reply_err(ErrKind e)
{
    s_err[e]++;
    reply(k_errText[e], (uint16_t)strlen(k_errText[e]));
}

/* Traduce el rechazo del controlador al codigo de error del protocolo. */
static void reply_ctrl_err(CtrlResult r)
{
    reply_err((r == CTRL_ERR_RANGE) ? E_RANGE : E_STATE);
}

static void reply_ok_fmt(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
static void reply_ok_fmt(const char *fmt, ...)
{
    va_list ap;
    int     n;

    s_cmdOk++;
    va_start(ap, fmt);
    n = vsnprintf(s_buf, sizeof s_buf, fmt, ap);
    va_end(ap);
    if (n < 0) {
        n = 0;
    }
    if ((size_t)n >= sizeof s_buf) {
        n = (int)sizeof s_buf - 1;
    }
    reply(s_buf, (uint16_t)n);
}

/* Mensaje no solicitado (TEL, EVT): solo si cabe entero. */
static bool emit(const char *p, uint16_t n)
{
    if (Console_TxFree() < n) {
        return false;
    }
    (void)Console_WriteBytes((const uint8_t *)p, n);
    return true;
}

/* ---------- STATUS ---------- */

static void send_status(uint32_t now)
{
    CtrlInfo     ci;
    ButtonStats  bs;
    ConsoleStats cs;

    Ctrl_GetInfo(&ci);
    Button_GetStats(&bs);
    Console_GetStats(&cs);
    reply_ok_fmt("OK mode=%s freq=%lu duty=%lu manual=%lu ramp=%lu dir=%s resume=%s uptime_ms=%lu stream=%s "
                 "short=%lu long=%lu short_ign=%lu cmd_ok=%lu err_cmd=%lu err_arg=%lu err_range=%lu "
                 "err_state=%lu err_ovf=%lu rx_lost=%lu uart_err=%lu tx_drop=%lu\r\n",
                 Ctrl_ModeName(ci.mode), (unsigned long)ci.freqHz, (unsigned long)ci.dutyApplied,
                 (unsigned long)ci.dutyManual, (unsigned long)ci.rampDuty,
                 (ci.rampDir > 0) ? "up" : "down",
                 (ci.mode == MODE_PAUSE) ? Ctrl_ModeName(ci.resumeTo) : "-",
                 (unsigned long)now, s_stream ? "ON" : "OFF",
                 (unsigned long)bs.shortCount, (unsigned long)bs.longCount, (unsigned long)ci.shortIgnored,
                 (unsigned long)s_cmdOk, (unsigned long)s_err[E_COMMAND], (unsigned long)s_err[E_ARG],
                 (unsigned long)s_err[E_RANGE], (unsigned long)s_err[E_STATE],
                 (unsigned long)s_err[E_OVERFLOW], (unsigned long)cs.rxRingOverflow,
                 (unsigned long)(cs.uartOverrun + cs.uartFraming + cs.uartNoise + cs.uartParity),
                 (unsigned long)cs.txDropped);
}

/* ---------- ejecucion de una orden ---------- */

static void execute(const char *line, uint32_t now)
{
    Cmd        c;
    CmdParse   pr = Cmd_Parse(line, &c);
    CtrlInfo   ci;
    CtrlResult r;
    bool       locked = PwmCheck_Busy();     /* hay una verificacion del PWM en curso */

    if (pr == CMD_PARSE_COMMAND) { reply_err(E_COMMAND); return; }
    if (pr == CMD_PARSE_ARG)     { reply_err(E_ARG);     return; }
    if (pr == CMD_PARSE_RANGE)   { reply_err(E_RANGE);   return; }

    switch (c.id) {
    case CMD_HELP:
        s_cmdOk++;
        reply(k_help, (uint16_t)(sizeof k_help - 1U));
        return;
    case CMD_STATUS:
        send_status(now);
        return;
    case CMD_STREAM_ON:
        if (!s_stream) {
            s_stream = true;
            s_telLast = now;
        }
        reply_ok_fmt("OK STREAM ON\r\n");
        return;
    case CMD_STREAM_OFF:
        s_stream = false;
        reply_ok_fmt("OK STREAM OFF\r\n");
        return;
    default:
        break;
    }

    /* A partir de aqui las ordenes cambian el estado: se rechazan mientras corre una verificacion. */
    if (locked) {
        reply_err(E_STATE);
        return;
    }

    switch (c.id) {
    case CMD_MODE_MANUAL:
        r = Ctrl_SetManual(now);
        if (r != CTRL_OK) { reply_ctrl_err(r); return; }
        Ctrl_GetInfo(&ci);
        reply_ok_fmt("OK MODE MANUAL duty=%lu\r\n", (unsigned long)ci.dutyApplied);
        return;
    case CMD_MODE_AUTO:
        r = Ctrl_SetAuto(now);
        if (r != CTRL_OK) { reply_ctrl_err(r); return; }
        Ctrl_GetInfo(&ci);
        reply_ok_fmt("OK MODE AUTO duty=%lu\r\n", (unsigned long)ci.dutyApplied);
        return;
    case CMD_DUTY:
        r = Ctrl_SetDuty(c.value);
        if (r != CTRL_OK) { reply_ctrl_err(r); return; }
        reply_ok_fmt("OK DUTY %lu\r\n", (unsigned long)c.value);
        return;
    case CMD_FREQ:
        r = Ctrl_SetFreq(c.value);
        if (r != CTRL_OK) { reply_ctrl_err(r); return; }
        reply_ok_fmt("OK FREQ %lu\r\n", (unsigned long)c.value);
        return;
    case CMD_PAUSE:
        (void)Ctrl_Pause(now);
        reply_ok_fmt("OK PAUSE\r\n");
        return;
    case CMD_RESUME:
        r = Ctrl_Resume(now);
        if (r != CTRL_OK) { reply_ctrl_err(r); return; }
        Ctrl_GetInfo(&ci);
        reply_ok_fmt("OK RESUME mode=%s duty=%lu\r\n", Ctrl_ModeName(ci.mode), (unsigned long)ci.dutyApplied);
        return;
    default:
        break;
    }

#if APP_DIAG_ENABLED
    Ctrl_GetInfo(&ci);
    switch (c.id) {
    case CMD_DIAG_MEASURE:
    case CMD_DIAG_PWMALL:
        if (ci.mode != MODE_MANUAL) {       /* en AUTO el duty cambia durante la medicion */
            reply_err(E_STATE);
            return;
        }
        if (c.id == CMD_DIAG_MEASURE) {
            PwmInfo pi;

            Pwm_GetInfo(&pi);
            reply_ok_fmt("OK DIAG MEASURE (TIM3: reloj %lu Hz, PSC=%lu, ARR=%lu, CCR=%lu)\r\n",
                         (unsigned long)pi.timerClkHz, (unsigned long)pi.prescaler,
                         (unsigned long)pi.arr, (unsigned long)pi.ccr);
            (void)PwmCheck_StartOne(now);
        } else {
            reply_ok_fmt("OK DIAG PWMALL\r\n");
            (void)PwmCheck_StartAll(now);
        }
        return;
    case CMD_DIAG_STALL:
        s_stall      = true;
        s_stallStart = now;
        s_stallMs    = c.value;
        reply_ok_fmt("OK DIAG STALL %lu\r\n", (unsigned long)c.value);
        return;
    case CMD_DIAG_WRAP: {
        uint32_t t;

        Tick_SetMs(APP_WRAP_TEST_START);
        t = Tick_Ms();
        Ctrl_Rebase(t);
        s_telLast = t;
        s_skipTel = true;
        reply_ok_fmt("OK DIAG WRAP tick=%lu (UINT32_MAX en 2048 ms)\r\n", (unsigned long)t);
        return;
    }
    default:
        break;
    }
#endif
    reply_err(E_COMMAND);
}

/* ---------- RX ---------- */

static void echo(LineEvent ev, uint8_t c)
{
#if APP_CONSOLE_ECHO
    switch (ev) {
    case LINE_CHAR:
        (void)Console_WriteBytes(&c, 1U);
        break;
    case LINE_ERASE:
        (void)Console_Write("\b \b");
        break;
    case LINE_READY:
    case LINE_EMPTY:
    case LINE_OVERFLOW:
    case LINE_BAD:
        (void)Console_Write("\r\n");
        break;
    default:
        break;
    }
#else
    (void)ev;
    (void)c;
#endif
}

/* Si RX perdio o danio bytes desde la ultima vez, no se puede confiar en nada de lo que hay
 * en el buffer ni en la linea en curso: se descarta todo y la linea afectada se ignora hasta
 * su terminador (se responde ERR OVERFLOW). A lo sumo se pierde alguna orden valida que
 * esperaba en el buffer; nunca se ejecuta un fragmento. */
static void check_rx_fault(void)
{
    uint32_t f = Console_RxFaultCount();
    uint8_t  b;

    if (f != s_faultSeen) {
        s_faultSeen = f;
        while (Console_ReadByte(&b)) {
            /* se tira lo que habia en el buffer */
        }
        Line_Poison(&s_lr);
        s_rxFaultDiscards++;
    }
}

static void consume_rx(uint32_t now)
{
    uint8_t  c;
    uint32_t n = 0U;

    while ((n < APP_BYTES_PER_LOOP) && Console_ReadByte(&c)) {
        LineEvent ev = Line_Feed(&s_lr, c);

        n++;
        echo(ev, c);
        if (ev == LINE_READY) {
            execute(Line_Text(&s_lr), now);
            return;                          /* una orden por vuelta */
        }
        if (ev == LINE_OVERFLOW) {
            reply_err(E_OVERFLOW);
            return;
        }
        if (ev == LINE_BAD) {
            reply_err(E_COMMAND);
            return;
        }
    }
}

/* ---------- telemetria ---------- */

static void telemetry(uint32_t now)
{
    CtrlInfo    ci;
    ButtonStats bs;
    char        t[128];
    int         n;

    if (!s_stream || s_skipTel || ((uint32_t)(now - s_telLast) < APP_TELEMETRY_MS)) {
        return;
    }
    Ctrl_GetInfo(&ci);
    Button_GetStats(&bs);
    n = snprintf(t, sizeof t, "TEL up=%lu mode=%s freq=%lu duty=%lu manual=%lu short=%lu long=%lu\r\n",
                 (unsigned long)now, Ctrl_ModeName(ci.mode), (unsigned long)ci.freqHz,
                 (unsigned long)ci.dutyApplied, (unsigned long)ci.dutyManual,
                 (unsigned long)bs.shortCount, (unsigned long)bs.longCount);
    if ((n > 0) && emit(t, (uint16_t)n)) {
        s_telLast += APP_TELEMETRY_MS;       /* sin deriva */
        if ((uint32_t)(now - s_telLast) >= APP_TELEMETRY_MS) {
            s_telLast = now;                 /* si main se atraso, no se manda una rafaga */
        }
    }                                        /* si no cupo, se reintenta en la vuelta siguiente */
}

/* ---------- interfaz publica ---------- */

void Shell_Init(uint32_t now)
{
    Line_Init(&s_lr);
    s_faultSeen = Console_RxFaultCount();
    s_stream    = false;
    s_telLast   = now;
    s_skipTel   = false;
    s_stall     = false;
    s_cmdOk     = 0U;
    s_msgLen    = 0U;
    s_rxFaultDiscards = 0U;
    memset(s_err, 0, sizeof s_err);
}

void Shell_Service(uint32_t now)
{
    s_skipTel = false;
    if (!flush_pending()) {
        return;                              /* respuesta pendiente: no se lee la orden siguiente */
    }
    if (s_stall && ((uint32_t)(now - s_stallStart) >= s_stallMs)) {
        s_stall = false;
    }
    if (!s_stall) {
        check_rx_fault();
        consume_rx(now);
    }
    telemetry(now);
}

void Shell_ButtonEvent(ButtonEvent ev, bool changed)
{
    CtrlInfo    ci;
    ButtonStats bs;
    char        t[96];
    int         n;

    if (!s_stream || (ev == BTN_EVT_NONE)) {
        return;
    }
    Ctrl_GetInfo(&ci);
    Button_GetStats(&bs);
    n = snprintf(t, sizeof t, "EVT B1 %s %s mode=%s duty=%lu n_short=%lu n_long=%lu\r\n",
                 (ev == BTN_EVT_SHORT) ? "short" : "long", changed ? "ok" : "ignored",
                 Ctrl_ModeName(ci.mode), (unsigned long)ci.dutyApplied,
                 (unsigned long)bs.shortCount, (unsigned long)bs.longCount);
    if (n > 0) {
        (void)emit(t, (uint16_t)n);
    }
}

void Shell_GetStats(ShellStats *out)
{
    out->cmdOk           = s_cmdOk;
    out->errCommand      = s_err[E_COMMAND];
    out->errArg          = s_err[E_ARG];
    out->errRange        = s_err[E_RANGE];
    out->errState        = s_err[E_STATE];
    out->errOverflow     = s_err[E_OVERFLOW];
    out->rxFaultDiscards = s_rxFaultDiscards;
    out->stream          = s_stream;
    out->stalled         = s_stall;
}
