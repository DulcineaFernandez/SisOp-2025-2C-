# Sistemas Operativos - FIUBA (2025 2C)

Este repositorio contiene las soluciones a los trabajos prácticos de la materia **Sistemas Operativos** (FIUBA) 2do cuatrimestre 2025.

## Contenido del Repositorio

### 1. [Shell]
Implementación de un intérprete de comandos propio (shell de Linux).
> Ejecución de comandos, manejo de *pipes* (`|`), redirecciones (`>`, `<`), ejecución en segundo plano (`&`) y variables de entorno.
Interacción básica con el sistema operativo y la gestión de procesos desde el espacio de usuario.

### 2. [Scheduler]
Implementación y análisis de planificadores de procesos dentro de un kernel educativo (JOS/xv6 environment).
> Implementación de algoritmos de planificación como *Round Robin* y colas de prioridad.
Entender el funcionamiento del *context switch* y las políticas de planificación de CPU.

### 3. [Filesystem]
Implementación de un sistema de archivos en espacio de usuario utilizando **FUSE** (*Filesystem in Userspace*).
> Creación, lectura, escritura y eliminación de archivos y directorios. Persistencia de datos en un archivo binario en disco.
> Estructura interna de un sistema de archivos (inodos, bloques de datos, serialización).

## Ejecución

Cada directorio cuenta con su propio archivo `README.md` con instrucciones detalladas sobre:
- Cómo compilar el proyecto (`make`).
- Cómo ejecutar las pruebas automatizadas.
- Requerimientos del sistema operativo.
- Detalles de implementación y respuestas a preguntas teóricas.

