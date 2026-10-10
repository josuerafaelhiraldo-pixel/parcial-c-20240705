/* Pruebas en PC de button.c: rebotes, 30 ms, corta/larga, 20 pulsaciones,
 * limites 1499/1500 ms, desbordamiento de uint32 y arranque con B1 pulsado.
 * Cada milisegundo simulado llama a Button_Service una vez, como hace app.c. */
#include "button.h"
#include "app_config.h"
#include "hal_sim.h"
#include <stdio.h>

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALLO linea %d: %s\n", __LINE__, #c); fails++; } } while (0)

static uint32_t now;
static int n_short, n_long, n_events;
static uint32_t t_long;                       /* instante en que salio la ultima larga */
static uint32_t edges_base;                   /* flancos de la ISR al empezar el caso */

static void ms(uint32_t n)
{
    while (n--) {
        ButtonEvent ev;
        now++;
        ev = Button_Service(now);
        if (ev == BTN_EVT_SHORT) { n_short++; n_events++; }
        if (ev == BTN_EVT_LONG)  { n_long++;  n_events++; t_long = now; }
    }
}
/* Flanco: cambia el nivel del pin y dispara la ISR de EXTI13. */
static void pin(int pressed)
{
    sim_set_btn(pressed ? 0 : 1);             /* activo en bajo */
    HAL_GPIO_EXTI_Callback(GPIO_PIN_13);
}
static void reset_case(uint32_t start)
{
    sim_set_btn(1);
    now = start;
    Button_Init(now);
    n_short = n_long = n_events = 0;
    { ButtonStats b; Button_GetStats(&b); edges_base = b.edgesIsr; }   /* el contador de la ISR no se reinicia */
}
static uint32_t edges(void) { ButtonStats b; Button_GetStats(&b); return b.edgesIsr - edges_base; }
/* Pulsacion limpia: pulsa, espera hold ms, suelta, espera 100 ms. */
static void press(uint32_t hold) { pin(1); ms(hold); pin(0); ms(100); }

int main(void)
{
    ButtonStats st;

    /* --- ISR: solo cuenta flancos del pin correcto --- */
    reset_case(1000);
    CHECK(edges() == 0U);
    HAL_GPIO_EXTI_Callback(GPIO_PIN_5);       /* otro pin: se ignora */
    CHECK(edges() == 0U);
    pin(1); pin(0);
    CHECK(edges() == 2U);

    /* --- pulsacion corta limpia de 100 ms --- */
    reset_case(1000);
    press(100);
    CHECK(n_short == 1 && n_long == 0 && n_events == 1);
    Button_GetStats(&st);
    CHECK(st.shortCount == 1U && st.longCount == 0U && st.pressed == 0U);
    CHECK(st.lastDurationMs >= 99U && st.lastDurationMs <= 101U);   /* medido sobre estado validado */

    /* --- rebotes al pulsar y al soltar: una sola corta --- */
    reset_case(1000);
    pin(1); ms(1); pin(0); ms(2); pin(1); ms(1); pin(0); ms(1); pin(1);   /* rebotes ~8 ms */
    ms(200);                                  /* pulsado estable */
    pin(0); ms(1); pin(1); ms(2); pin(0); ms(1); pin(1); ms(1); pin(0);   /* rebotes al soltar */
    ms(200);
    CHECK(n_short == 1 && n_long == 0 && n_events == 1);
    CHECK(edges() == 10U);                     /* 10 flancos, 1 pulsacion */

    /* --- pulsos de ruido cortos: no son pulsacion --- */
    reset_case(1000);
    pin(1); ms(10); pin(0); ms(100);
    pin(1); ms(25); pin(0); ms(100);
    CHECK(n_events == 0);
    Button_GetStats(&st); CHECK(st.pressed == 0U && edges() == 4U);

    /* --- pulso de 40 ms: ya es pulsacion valida (corta) --- */
    reset_case(1000);
    press(40);
    CHECK(n_short == 1 && n_long == 0);
    Button_GetStats(&st); CHECK(st.lastDurationMs >= 30U && st.lastDurationMs <= 41U);

    /* --- un flanco de la ISR sin cambio de nivel reinicia la ventana de 30 ms --- */
    reset_case(1000);
    pin(1);
    ms(20);
    HAL_GPIO_EXTI_Callback(GPIO_PIN_13);      /* glitch entre dos muestras: el pin se ve igual */
    ms(20);
    Button_GetStats(&st); CHECK(st.pressed == 0U);   /* 40 ms desde el flanco, pero reiniciado */
    ms(15);
    Button_GetStats(&st); CHECK(st.pressed == 1U);   /* ya hubo 30+ ms estables tras el glitch */
    pin(0); ms(100);
    CHECK(n_short == 1);

    /* --- sin interrupcion (EXTI enmascarada): el muestreo por si solo tambien exige 30 ms --- */
    reset_case(1000);
    ms(100);                                  /* reposo largo */
    sim_set_btn(0); ms(10);
    Button_GetStats(&st); CHECK(st.pressed == 0U);   /* a los 10 ms aun no es valido */
    sim_set_btn(1); ms(100);
    CHECK(n_events == 0);                     /* 10 ms de pulso: no se acepta */
    sim_set_btn(0); ms(100);
    Button_GetStats(&st); CHECK(st.pressed == 1U && edges() == 0U);
    sim_set_btn(1); ms(100);
    CHECK(n_short == 1 && n_long == 0);

    /* --- 20 pulsaciones cortas separadas: 20 eventos, sin duplicados --- */
    reset_case(1000);
    for (int i = 0; i < 20; i++) {
        pin(1); ms(2); pin(0); ms(1); pin(1);          /* con un rebote al pulsar */
        ms(120);
        pin(0); ms(1); pin(1); ms(1); pin(0);          /* y otro al soltar */
        ms(200);
    }
    CHECK(n_short == 20 && n_long == 0 && n_events == 20);
    Button_GetStats(&st); CHECK(st.shortCount == 20U && st.longCount == 0U);

    /* --- mantener 3 s: una larga a ~1500 ms, nada al soltar, sin repeticiones --- */
    reset_case(1000);
    pin(1);
    ms(3000);
    CHECK(n_long == 1 && n_short == 0);
    CHECK(t_long - 1000U >= 1500U && t_long - 1000U <= 1535U);   /* 30 ms de validacion + 1500 */
    CHECK(t_long - 1000U == 1531U);           /* exacto: flanco, 1 ms de muestreo, 30 ms de ventana, 1500 ms */
    pin(0); ms(200);
    CHECK(n_long == 1 && n_short == 0 && n_events == 1);          /* al soltar no hay corta */
    Button_GetStats(&st);
    CHECK(st.longCount == 1U && st.shortCount == 0U && st.lastDurationMs >= 2900U);

    /* --- mantener 10 s: sigue siendo una sola larga (sin repeticion) --- */
    reset_case(1000);
    pin(1); ms(10000);
    CHECK(n_long == 1 && n_events == 1);
    pin(0); ms(200); CHECK(n_events == 1);

    /* --- barrido del limite: toda pulsacion da exactamente UN evento,
     *     corta si lo validado dura menos de 1500 ms y larga si dura 1500 o mas --- */
    for (uint32_t hold = 1400U; hold <= 1600U; hold++) {
        reset_case(5000);
        press(hold);
        Button_GetStats(&st);
        CHECK(n_events == 1);
        if (st.lastDurationMs < APP_LONG_PRESS_MS) {
            CHECK(n_short == 1 && n_long == 0);
        } else {
            CHECK(n_long == 1 && n_short == 0);
        }
    }
    /* y se comprueba que el barrido pasa por los dos lados del limite */
    reset_case(5000); press(1450); Button_GetStats(&st);
    CHECK(n_short == 1 && st.lastDurationMs < 1500U);
    reset_case(5000); press(1550); Button_GetStats(&st);
    CHECK(n_long == 1 && st.lastDurationMs >= 1500U);

    /* --- barrido de pulsaciones muy cortas: ningun evento con duracion < 30 ms --- */
    for (uint32_t hold = 0U; hold <= 120U; hold++) {
        reset_case(5000);
        press(hold);
        Button_GetStats(&st);
        CHECK(n_events <= 1);
        if (n_events == 1) {
            CHECK(n_short == 1 && st.lastDurationMs >= APP_SHORT_MIN_MS);
        }
    }

    /* --- el tiempo cruza el desbordamiento de uint32 --- */
    reset_case(0xFFFFFFF0U);
    pin(1); ms(25);                           /* el desbordamiento ocurre dentro de la ventana de 30 ms */
    Button_GetStats(&st); CHECK(st.pressed == 0U);     /* aun no pasaron 30 ms */
    ms(10);
    Button_GetStats(&st); CHECK(st.pressed == 1U);     /* ya paso la ventana */
    pin(0); ms(100);
    CHECK(n_short == 1 && n_long == 0);
    Button_GetStats(&st); CHECK(st.lastDurationMs >= 34U && st.lastDurationMs <= 36U);
    reset_case(0xFFFFFFF0U);
    press(100);                               /* corta cruzando UINT32_MAX */
    CHECK(n_short == 1 && n_long == 0);
    Button_GetStats(&st); CHECK(st.lastDurationMs >= 99U && st.lastDurationMs <= 101U);
    reset_case(0xFFFFFE00U);                  /* larga: 1500 ms despues cruza el desbordamiento */
    pin(1); ms(3000);
    CHECK(n_long == 1 && n_short == 0);
    CHECK((uint32_t)(t_long - 0xFFFFFE00U) >= 1500U && (uint32_t)(t_long - 0xFFFFFE00U) <= 1535U);
    pin(0); ms(200); CHECK(n_events == 1);
    reset_case(0xFFFFFFFEU);                  /* larga que arranca a 2 ms del desbordamiento */
    pin(1); ms(2000);
    CHECK(n_long == 1);
    CHECK((uint32_t)(t_long - 0xFFFFFFFEU) >= 1500U && (uint32_t)(t_long - 0xFFFFFFFEU) <= 1535U);
    reset_case(0xFFFFFFF0U);                  /* 20 cortas a caballo del desbordamiento */
    for (int i = 0; i < 20; i++) press(150);
    CHECK(n_short == 20);

    /* --- arranque con B1 pulsado: esa pulsacion no genera eventos --- */
    sim_set_btn(0);
    now = 2000; Button_Init(now);
    n_short = n_long = n_events = 0;
    ms(3000);
    CHECK(n_events == 0);
    Button_GetStats(&st); CHECK(st.pressed == 1U);
    pin(0); ms(0); sim_set_btn(1); HAL_GPIO_EXTI_Callback(GPIO_PIN_13); ms(200);
    CHECK(n_events == 0);                     /* al soltar tampoco */
    Button_GetStats(&st); CHECK(st.pressed == 0U);
    press(100);                               /* y despues funciona normal */
    CHECK(n_short == 1 && n_events == 1);

    printf(fails ? "BOTON: %d FALLOS\n" : "BOTON: todas las pruebas OK\n", fails);
    return fails != 0;
}
