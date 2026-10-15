# SerialCommand

[Aplicación](../../README_APP.md).

Punto de entrada reservado para futuros comandos serie. Actualmente no hay
comandos activos ni se controla el sensor desde este módulo.

`SerialCommand.h` declara `processSerialCommands()` y `SerialCommand.cpp` la
implementa. `main.cpp` la llama una vez por vuelta del bucle. Descarta como máximo
32 bytes disponibles en cada llamada y retorna si no hay datos; no espera una
línea ni responde a la entrada recibida.

Depende únicamente de Arduino/Serial. Los mensajes de diagnóstico de otros
módulos siguen saliendo por Serial a 115200 baudios.

Si se incorporan comandos, mantener un presupuesto de procesamiento acotado para
no retrasar BLE, documentar sintaxis/resultados aquí y delegar las acciones del
sensor en `TCS3448Manager`.
