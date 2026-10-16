# TCS3448Manager

[Aplicación](../../README_APP.md) · [BLE](../ble/README.md) · [Driver](../../lib/Adafruit_TCS3448/README.md).

Fuente principal de verdad del sensor: valida y aplica configuración de alto
nivel, verifica registros, coordina medidas y crea muestras. No conoce UUID,
características GATT ni controles HTML.

## Archivos y tipos

| Archivo | Contenido |
| --- | --- |
| `TCS3448Types.h` | Config, Sample, Status y enums propios de gain/SMUX/autozero/error; sin Adafruit |
| `TCS3448ConfigMath.h/.cpp` | Validación pura y conversión de integración a ajustes físicos |
| `TCS3448Manager.h/.cpp` | Driver privado, configuración confirmada y estado de adquisición |

Config usa gain propio 0..12, integración double en ms, corriente uint16 (incluye
258 mA), LED bool, SMUX 6/12/18, autozero propio 0/1/2 y periodo uint32. Pines y
frecuencia I2C son de arranque y no se admiten cambios remotos.

Sample tiene array máximo de 18 uint16, `channelCount`, gain, saturación,
secuencia y timestamp. Status contiene ready, acquiring, error, contador de
muestras y revisión de configuración. Los structs no se transmiten directamente.

## API pública

| Función | Uso |
| --- | --- |
| `Init(config)` | Validar, inicializar hardware, escribir y verificar configuración |
| `GetConfig()` / `GetStatus()` | Consultar instantáneas del gestor |
| `RefreshConfig()` | Leer cada ajuste hardware y confirmar que coincide con la configuración activa |
| `ApplyConfig(config)` | Validar candidato completo, pausar, aplicar, verificar y confirmar o restaurar |
| `GetADCFullScale()` / `GetMinimumPeriodMs()` | Magnitudes derivadas por firmware |
| `StartAcquisition()` | Iniciar planificador; no altera LED ni reinicia una adquisición activa |
| `StopAcquisition(turnOffLED=false)` | Cancelar medida; apagado adicional para desconexión/fallo |
| `Update(sample)` | Devolver true solo cuando existe una muestra nueva completa |
| `ErrorName(error)` | Nombre de error para trazas |

Se conservan LEDEnable, GetGain, GetIntegrationTime, ReadAllChannels, PrintHeader
y PrintReadings para diagnóstico. LEDEnable usa ApplyConfig. ReadAllChannels es
bloqueante, se rechaza durante adquisición y no se usa en el flujo BLE.

## Conversión, límites y periodo

El cálculo de integración busca entre 256 multiplicadores y pasos 1..65535 la
combinación más próxima al tiempo pedido. No permite ATIME=ASTEP=0 ni ASTEP=65535.
El rango físico es 0,00556 ms .. 46639,9488 ms, con tolerancia de 0,0005 ms en el
extremo superior para el redondeo del protocolo a microsegundos.

```text
integration_ms = (ATIME + 1) × (ASTEP + 1) × 0,00278
adc_full_scale = min((ATIME + 1) × (ASTEP + 1), 65535)
```

La fórmula y restricciones proceden de la [ficha TCS3448, sección 10.2.2](https://look.ams-osram.com/m/1c24b057e65ee61e/original/TCS3448-14-Channel-multi-spectral-sensor.pdf).

Gain solo admite códigos 0..12. Corriente: 4..258 mA pares, sin truncar ni ajustar
silenciosamente valores inválidos. SMUX admite 6, 12 o 18. Autozero se convierte a
0 (Disabled), 255 (FirstMeasurement) o 1 (EveryMeasurement) del registro del sensor.
First se refiere al primer ciclo tras habilitar el motor; al hacer medidas
individuales puede ejecutarse de nuevo en cada adquisición. Every se aplica a
cada subciclo, no solo a cada muestra compuesta.

Sample Period es del planificador, no WTIME. Se exige un entero 1..2147483647 y:

```text
cycles = channels / 6
zero_cycles = 0 (Disabled), 1 (First), cycles (Every)
minimum_period_ms = ceil(integration_real_ms × cycles + 20 × zero_cycles + 10)
```

Los 20 ms reservan margen sobre los 15 ms típicos de autozero; no son un máximo
hardware garantizado. Se añaden 10 ms de I2C/planificación. Por defecto el mínimo
resulta 181 ms. Un valor menor se rechaza con SAMPLE_PERIOD_TOO_SHORT.
El timeout de adquisición es minimum_period_ms + 1000 ms.

## Aplicación y rollback

1. Validar todo el candidato antes de escrituras o cambios de adquisición.
2. Verificar la configuración anterior mediante lectura comprobada.
3. Guardar ajustes previos, pausar y cancelar la medida en curso.
4. Escribir todos los ajustes, con LED habilitado al final, y releerlos.
5. Si coinciden, confirmar tiempo cuantizado, ADC full scale y revisión; reanudar
   el planificador si estaba activo.
6. Ante fallo, detener y reescribir los ajustes anteriores. Verificarlos antes de
   mantener ready. La adquisición queda detenida incluso si el rollback fue correcto.
7. Si el rollback no se confirma, ready=false, intentar apagar LED y requerir
   revisión/reinicio. El candidato parcial nunca se publica como activo.

No es una transacción eléctrica indivisible: hay varias operaciones I2C. La
atomicidad de validación y la verificación/recuperación evitan confirmar mezclas.
No hay persistencia ni reintento de Init desde BLE. RefreshConfig no adivina una
configuración distinta: si no coincide con lo confirmado, invalida el estado.

## Adquisición y canales

START respeta LED configurado. STOP normal conserva LED; desconexión/fallo pide
apagado y actualiza la copia activa y su revisión cuando se verifica. Por defecto
LED está apagado incluso al hacer START hasta que el usuario lo habilita con APPLY.

Update inicia medidas sin solaparlas y consulta disponibilidad en llamadas
sucesivas. I2C es síncrono, sin esperar activamente a que termine la integración.
Lee únicamente 6/12/18 resultados según SMUX. El driver obtiene canales, gain y
saturación del mismo frame; se ponen a cero las posiciones no válidas.

Orden del protocolo, truncado al número válido:
FZ, FY, FXL, NIR, VIS_TL_0, VIS_BR_0, F2, F3, F4, F6, VIS_TL_1, VIS_BR_1,
F1, F7, F8, F5, VIS_TL_2, VIS_BR_2. Los counts son crudos, sin normalizar/calibrar.
La saturación es una bandera del frame. Secuencia y timestamp uint32 pueden
desbordarse; timestamp es la consulta que detecta datos, no inicio de exposición.

## Errores, dependencias y pruebas

Todos los accesos al sensor se realizan desde loop, no desde callbacks BLE.
Depende de Arduino, Wire, el driver local y Adafruit AS7343. Los errores de
validación 16..23 se describen en [protocolo](../../docs/HITO_3_BLE.md#servicio-gatt-v1).
Un fallo I2C/timeout intenta apagar y detener; no hay spam de logs en cada loop.

Compilar con `pio run -e esp32dev`. La verificación JS no prueba esta máquina de
estados ni el I2C real: comprobar periodos límite, readback, cambios RUNNING,
SMUX, LED 258 mA y rollback/fallos siguiendo la
[aceptación en hardware](../../docs/HITO_3_BLE.md#pasos-de-aceptación-en-hardware).

## LED AUTO

`TCS3448Config.ledAuto` tiene prioridad sobre `ledEnabled`. En AUTO el LED sigue
`status.acquiring`: START lo enciende y verifica antes de iniciar medidas; STOP
lo apaga, sin borrar el modo AUTO. Desconexion/fallo tambien lo apagan. APPLY
calcula el estado fisico segun la adquisicion anterior y verifica los registros.
Si falla APPLY, rollback restaura AUTO con LED apagado y adquisicion detenida.
Manual OFF/ON conserva el comportamiento anterior. No se persiste tras reinicio.
