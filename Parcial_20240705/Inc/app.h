/* app.h - Logica de la aplicacion. */
#ifndef APP_H
#define APP_H

void App_Init(void);    /* llamar una vez, despues de inicializar los perifericos */
void App_Loop(void);    /* llamar en cada vuelta del lazo principal (sin esperas) */

#endif /* APP_H */
