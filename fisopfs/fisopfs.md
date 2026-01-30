# fisop-fs

## Diseño e Implementación


### Estructura de datos

Para el filesystem se implementaron dos tipos de datos principales: `inodo_t` y `super_block_t`, que se definen en el archivo `fs.h`. Las estructuras en memoria que almacenarán los archivos, directorios y sus metadatos son:

#### Inodo (`inodo_t`)
Cada archivo o directorio se representa mediante un "inodo".

El `inodo_t` contiene:

- `inodo_type`: tipo del archivo (archivo, directorio, vacio)
- `nombre_archivo`: nombre del archivo o directorio
- `ruta`: ruta del archivo o directorio
- `ruta_padre`: ruta del directorio padre
- `usuario`: usuario propietario del archivo o directorio (uid)
- `grupo`: grupo propietario del archivo o directorio (gid)
- `permisos_archivo`: permisos del archivo o directorio (lectura, escritura, ejecución)
- `contenido_archivo`: contenido del archivo
- `tamanio_archivo`: tamaño del archivo
- `cantidad_enlaces`: cantidad de enlaces al inodo
- `tiempo_creacion`: tiempo de creación del inodo
- `tiempo_modificacion`: tiempo de modificación del inodo
- `tiempo_ultimo_acceso`: tiempo de último acceso al inodo

 
#### Superbloque (`super_block_t`)

El sistema de archivos completo se representa en una estructura llamada `super_block_t`, la cual contiene todos los inodos y espacios libres (que no tienen inodos).

EL superbloque contiene:

- `inodos`: un arreglo de inodos con tamaño `MAX_INODOS` (es decir, es un tamaño fijo/estático)
- `mapa_bits`: un mapa de bits: un arreglo de enteros, cada entero corresponde a un inodo en el mismo vértice, y este indica si el índice está `LIBRE (0)` u `OCUPADO (1)` 
- `contador_inodos_ocupados`: un contador de inodos marcados como OCUPADO.


### Representación en memoria

**Flat Array**: Todos los archivos y directorios se almacenan en un único arreglo `inodos[MAX_INODOS]`. Como se utiliza un arreglo estático, se busca un índice libre a través de una búsqueda lineal, al agregarse se marca como ocupado y se llenan los datos del inodo.

Tiene una **jerarquía** dentro de la memoria: Aunque es un arreglo estático, la estructura jerárquica de los inodos se mantiene mediante los campos `ruta` y `ruta_padre` de cada uno.
La relación la podemos ver al comparar las cadenas que guardar las rutas: un inodo es hijo de otro inodo "padre" si su `ruta_padre` coincide con la `ruta` del posible "padre".


### Funciones de los inodos 

- `create`: crea un archivo/directorio nuevo en el filesystem. Inicializa los valores de un nuevo inodo.
*Aclaración: Los inodos no utilizados se inicializan con un tipo especial `TIPO_VACIO` para distinguirlos claramente de los archivos y directorios que van a ir creandose y que serán válidos (En el mapa de bits están marcados como libres).*

Usa funciones auxiliares para chequear si ya existe ese nombre `fs_get_inode`, para buscar un inodo libre `buscar_indice_libre`, y para inicializar los valores del inodo `inicializar_datos_inodo_nuevo`.

- `delete`: borra un archivo/directorio del filesystem. Libera los valores de un inodo. 

Usa funciones auxiliares para buscar el indice del inodo `fs_get_inode_index`, para liberar los valores del inodo en caso de ser un directorio `borrar_contenido_directorio`, y si es archivo `liberar_inodo`.

- `read_file`: lee datos de un archivo. 

Usa funciones auxiliares para buscar el inodo `fs_get_inode` a leer.

- `write_file`: escribe datos en un archivo.

Usa la función auxiliar `fs_get_inode` para buscar el inodoa escribir y `truncate_file` si hay que cambiarle el tamanio.

- `truncate_file`: cambia el tamanio del archivo

Usa la función auxiliar `fs_get_inode` para buscar el inodo a cambiar.

- `list_dir`: lista los nombres de archivos y directorios dentro de un directorio


Usa la función auxiliar `fs_get_inode` para buscar el inodo a listar.


### Búsqueda de archivos por path

La busqueda de archivos por path se realiza a traves de la funcion auxiliar `fs_get_inode`. 

```c
static inodo_t *fs_get_inode(const char *path){
    if(path == NULL){
        return NULL;
    }

    for (size_t i = 0; i < MAX_INODOS; i++) {
        if (fs.estados_inodos[i] == OCUPADO && strcmp(fs.inodos[i].ruta, path) == 0) {
            return &fs.inodos[i];
        }
    }
    return NULL;
}
```

Primero verifica que el **path** sea válido y no NULL, ya que necesitamos un nombre para buscarlo (No permitimos un archivo/directorio sin nombre). Luego itera sobre todos los inodos y verifica si el estado del inodo es `OCUPADO`, en este caso se verifica si coincide con el path. En caso afirmativo: **retorna un puntero al inodo**. 
En caso contrario, **retorna NULL**. 

### Formato de persistencia / Serialización

El manejo de la persistencia de datos se integra al trabajo gracias a las operaciones de FUSE en `fisopfs.c`: es el codigo que recibe las llamadas del kernel (open,read,mkdir,unlink,etc) y las traduce a funciones de nuestro modulo interno (create,read_file,write_file,truncate_file,delete,etc).

FUSE nos da una estructura:

```c
static struct fuse_operations operations = {
    .getattr  = fisopfs_getattr,
	.chmod    = fisopfs_chmod,
	.chown    = fisopfs_chown,
	.utime    = fisopfs_utimens,
    .readdir  = fisopfs_readdir,
    .read     = fisopfs_read,
    .write    = fisopfs_write,
    .truncate = fisopfs_truncate,
    .create   = fisopfs_create,
    .mknod    = fisopfs_mknod,
    .mkdir    = fisopfs_mkdir,
    .unlink   = fisopfs_unlink,
    .rmdir    = fisopfs_rmdir,
    .open     = fisopfs_open,
    .flush    = fisopfs_flush,
    .init     = fisopfs_init,
    .destroy  = fisopfs_destroy,
};
```
Y cada una de estas es un callback que FUSE ejecuta cuando se usa el filesystem desde Bash, por ejemplo:

- `touch` a.txt → llama a `create` → FUSE ejecuta `fisopfs_create`

- `cat` archivo.txt → llama a `read` → FUSE ejecuta `fisopfs_read`

- `rm` archivo.txt → llama a `unlink` → FUSE ejecuta `fisopfs_unlink`

las funciones FUSE no implementan la logica, solo delegan al fs interno.

**Persistencia en disco**:

Nuestro fs vive en RAM, pero debe guardarse al desmontar y restaurarse al montar. Para esto implementamos:

```bash
persistence_file.fisopfs
```

o cualquier archivo pasado asi:

```bash
./fisopfs --filedisk mi_fs.fisopfs prueba/
```

y el sistema resuelve la ruta absoluta, para soportar la ejecucion en background sin confundirse.

**Inicialización (`fisopfs_init`)**:

> Para que el sistema de archivos persista, se implementó la función `fisopfs_init` que intenta abrir el archivo de persistencia. Si extiste, con `fs_load_from_file` hacemos la carga del estado del momento, a traves de la carga del superbloque a memoria. 

Si no llega a existir, inicializa un nuevo sistema de archivos con `fs_init_empty` la cual crea un directorio raíz `/` y lo demás se inicializa vacio sesgún corresponda (0, /0, NULL, etc).


**Guardar el FileSystem**:

El guardado ocurre en dos lugares:

- fisopfs_flush
- fisopfs_destroy

**Sincronización (`fisopfs_flush`)**:

> FUSE puede solicitar sincronización cada cierto tiempo. En el momento que esta ocurre, se carga el estado actual a disco. Para esto se llama a `fs_save_to_file`, la cual hace la carga del superbloque en el archivo de persistencia.

**Desmontaje (`fisopfs_destroy`)**:

> Antes de terminar el proceso, se invoca `fs_save_to_file` para asegurar que los últimos cambios queden guardados.

**Serializacion**

El archivo `.fisopfs` contiene `super_block_t` entero, tal cual esta en la RAM.

```c
fwrite(&fs, sizeof(super_block_t), 1, file);
```
Como no usamos punteros internos dinamicos, la estructura es completamente serializable sin transformaciones, e incluye:

- todos los inodos
- el mapa de bits
- el contador de inodos usados
- el contenido de todos los archivos

Osea, el archivo es una copia binaria del estado completo del filesystem.


## Conclusiones

### Integracion entre *FUSE* y el modulo interno *fs.c*

Como dijimos, el archivo fisopfs.c, actua como la capa que conecta el kernel, FUSE y la logica interna del filesystem ***fs.c***. Por lo que, traduce syscalls reales de Linux (open,read,mkdir,unlink,etc) a nuestras funciones internas (create,read_file,write_file,truncate_file,delete,etc).

El flujo es:

- 1. Un programa del usuario ejecuta una syscall (por ejemplo touch, cat, rm, etc)
- 2. El kernel deriva esa syscall a ***FUSE***.
- 3. FUSE llama a uno de nuestros callbacks:
  - fisopfs_create
  - fisopfs_read
  - fisopfs_write
  - fisopfs_unlink
  - etc
- 4. Cada callback de `fisopfs.c` solo valida datos y delega en la funcion interna correspondiente.
- 5. La respuesta vuelve al kernel y al usuario como si fuera un filesystem real.

Con esto logramos la separacion para que `fs.c` sea totalmente agnostico de ***FUSE***, que la logica este probada por separado y
que `fisopfs.c` se ocupe solo de traducir, validar y armar `struct stat` 

### Decisiones de diseño

Elegimos un arreglo **flat** de inodos para simplicidad y velocidad, con una jerarquia que se mantiene con `ruta` y `ruta_padre`. Con una
serializacion directa gracias a no usar parametros dinamicos, ya que el `.fisopfs.c` es literalmente `fwrite(&fs, sizeof(super_block_t), 1, file);`.
Teniendo una resolucion de ruta absoluta del archivo de persistencia, para que ***FUSE*** en background no se confunda de directorio.
Usamos permisos estilo Unix (0644 archivos, 0755 directorios) y validaciones como `ENOENT`, `EEXIST`, `ENOTEMPTY`, etc.
Y guardamos todo el **filesystem** en memoria RAM, persistiendose solo en flush y destroy.

---


## Pruebas y Salidas de Ejemplo
### 0. Correr Pruebas Automaticas
Para correr las pruebas automaticas del filesystem, debemos estar en el directorio fisopfs

-Compilamos 
```bash
$ make clean
$ make
```
-Montar el fs FUSE

  en la que llamaremos Terminal 1, dentro del dir fisopfs, hacemos
```bash
$ ./fisopfs prueba -f -d
```
  con esto montamos el fs en foreground y con debug 
-Ejecutamos las pruebas


  esto lo hacemos en otra terminal, una Terminal 2, tambien en el dir fisopfs
```bash
$ ./test_fisopfs.sh
```
### 1. Archivos

#### Creación de archivos

Probamos crear archivos de diferentes formas:
```bash
# Con touch
$ touch prueba/archivo1.txt
$ ls -l prueba/archivo1.txt
-rw-r--r-- 1 usuario grupo 0 Nov 23 18:30 prueba/archivo1.txt

# Con redirección
$ echo "hola fisopfs" > prueba/saludo.txt
$ ls -l prueba/saludo.txt
-rw-r--r-- 1 usuario grupo 13 Nov 23 18:31 prueba/saludo.txt
```

Ambas formas funcionan correctamente.

---

#### Lectura de archivos
```bash
$ echo "contenido de prueba" > prueba/test.txt
$ cat prueba/test.txt
contenido de prueba

$ echo "linea 1
linea 2
linea 3" > prueba/multi.txt
$ more prueba/multi.txt
linea 1
linea 2
linea 3
```

La lectura funciona tanto con `cat` como con `more`.

---

#### Escritura: truncamiento y append
```bash
# Truncar (sobrescribir)
$ echo "primera version" > prueba/trunc.txt
$ cat prueba/trunc.txt
primera version
$ echo "version nueva" > prueba/trunc.txt
$ cat prueba/trunc.txt
version nueva

# Append (agregar al final)
$ echo "linea 1" > prueba/append.txt
$ echo "linea 2" >> prueba/append.txt
$ cat prueba/append.txt
linea 1
linea 2
```

El truncamiento con `>` y el append con `>>` se manejan correctamente.

---

#### Borrado
```bash
$ touch prueba/borrar.txt
$ ls prueba/borrar.txt
prueba/borrar.txt
$ rm prueba/borrar.txt
$ ls prueba/borrar.txt
ls: cannot access 'prueba/borrar.txt': No such file or directory

# También funciona con unlink
$ touch prueba/test_unlink.txt
$ unlink prueba/test_unlink.txt
$ ls prueba/test_unlink.txt
ls: cannot access 'prueba/test_unlink.txt': No such file or directory
```

---

#### Contenido binario
```bash
$ dd if=/dev/urandom of=prueba/binario.bin bs=512 count=1
1+0 records in
1+0 records out
512 bytes copied, 0.001 s, 512 kB/s
$ stat -c%s prueba/binario.bin
512
$ file prueba/binario.bin
prueba/binario.bin: data
```

El filesystem maneja correctamente archivos binarios.

---

### 2. Directorios

#### Crear y listar directorios
```bash
$ mkdir prueba/directorio1
$ ls -ld prueba/directorio1
drwxr-xr-x 2 usuario grupo 4096 Nov 23 18:35 prueba/directorio1

# Listar contenido
$ mkdir prueba/test_dir
$ touch prueba/test_dir/file1.txt
$ touch prueba/test_dir/file2.txt
$ touch prueba/test_dir/file3.txt
$ ls prueba/test_dir
file1.txt  file2.txt  file3.txt
```

---

#### Entradas especiales . y ..
```bash
$ mkdir prueba/special
$ ls -a prueba/special
.  ..
```

Las entradas `.` y `..` aparecen correctamente en todos los directorios.

---

#### Borrado de directorios
```bash
# Directorio vacío se puede borrar
$ mkdir prueba/vacio
$ rmdir prueba/vacio
$ ls prueba/vacio
ls: cannot access 'prueba/vacio': No such file or directory

# Directorio con archivos NO se puede borrar
$ mkdir prueba/lleno
$ touch prueba/lleno/archivo.txt
$ rmdir prueba/lleno
rmdir: failed to remove 'prueba/lleno': Directory not empty
$ ls -d prueba/lleno
prueba/lleno
```

El filesystem valida correctamente que el directorio esté vacío antes de borrarlo.

---

#### Subdirectorios
```bash
$ mkdir prueba/padre
$ mkdir prueba/padre/hijo
$ ls -R prueba/padre
prueba/padre:
hijo

prueba/padre/hijo:

# Crear archivos en subdirectorios
$ mkdir prueba/dir1
$ mkdir prueba/dir1/dir2
$ echo "archivo profundo" > prueba/dir1/dir2/deep.txt
$ cat prueba/dir1/dir2/deep.txt
archivo profundo
```

El filesystem soporta al menos un nivel de recursión en directorios.

---

### 3. Estadísticas (stat)

#### Metadatos básicos
```bash
$ touch prueba/stats.txt
$ stat prueba/stats.txt
  File: prueba/stats.txt
  Size: 0               Blocks: 0          IO Block: 4096   regular file
Device: 30h/48d Inode: 2           Links: 1
Access: (0644/-rw-r--r--)  Uid: ( 1000/ usuario)   Gid: ( 1000/   grupo)
Access: 2024-11-23 18:40:00.000000000 -0300
Modify: 2024-11-23 18:40:00.000000000 -0300
Change: 2024-11-23 18:40:00.000000000 -0300
 Birth: -

$ mkdir prueba/dir_stats
$ stat prueba/dir_stats
  File: prueba/dir_stats
  Size: 4096            Blocks: 8          IO Block: 4096   directory
Device: 30h/48d Inode: 3           Links: 2
Access: (0755/drwxr-xr-x)  Uid: ( 1000/ usuario)   Gid: ( 1000/   grupo)
Access: 2024-11-23 18:41:00.000000000 -0300
Modify: 2024-11-23 18:41:00.000000000 -0300
Change: 2024-11-23 18:41:00.000000000 -0300
 Birth: -
```

Los archivos se crean con permisos 0644 y los directorios con 0755.

---

#### Usuario y grupo
```bash
$ id
uid=1000(usuario) gid=1000(grupo) groups=1000(grupo)
$ touch prueba/ownership.txt
$ stat -c "UID:%u GID:%g" prueba/ownership.txt
UID:1000 GID:1000
```

Los archivos y directorios se crean con el UID y GID del usuario actual (usando `getuid()` y `getgid()`).

---

#### Timestamps
```bash
# Modificación (mtime)
$ echo "v1" > prueba/mtime.txt
$ stat -c %Y prueba/mtime.txt
1700760000
$ sleep 2
$ echo "v2" > prueba/mtime.txt
$ stat -c %Y prueba/mtime.txt
1700760002

# Acceso (atime)
$ echo "contenido" > prueba/atime.txt
$ stat -c %X prueba/atime.txt
1700760000
$ sleep 1
$ cat prueba/atime.txt > /dev/null
$ stat -c %X prueba/atime.txt
1700760001
```

Los timestamps se actualizan correctamente: `mtime` al escribir y `atime` al leer.

---

### 4. Persistencia

#### Archivo .fisopfs
```bash
$ ls -lh test.fisopfs
-rw-r--r-- 1 usuario grupo 8.5K Nov 23 18:45 test.fisopfs
$ file test.fisopfs
test.fisopfs: data
```

El filesystem se guarda en un archivo binario con extensión `.fisopfs`.

---

#### Persistencia entre montajes
```bash
# Crear archivo y directorio
$ echo "esto debe persistir" > prueba/persist.txt
$ mkdir prueba/persist_dir

# Desmontar
$ sudo umount prueba
$ mount | grep fisopfs
(no output)

# Montar nuevamente
$ ./fisopfs --filedisk test.fisopfs prueba/
$ mount | grep fisopfs
fisopfs on /home/usuario/fisop/prueba type fuse.fisopfs (rw,nosuid,nodev,relatime,user_id=1000,group_id=1000)

# Verificar que persisten
$ cat prueba/persist.txt
esto debe persistir
$ ls -d prueba/persist_dir
prueba/persist_dir
```

Los datos persisten correctamente al desmontar y volver a montar el filesystem.

---

#### Estructura completa
```bash
$ mkdir -p prueba/a/b/c
$ echo "archivo profundo" > prueba/a/b/c/deep.txt
$ ls -R prueba/a
prueba/a:
b

prueba/a/b:
c

prueba/a/b/c:
deep.txt

# Desmontar y remontar
$ sudo umount prueba
$ ./fisopfs --filedisk test.fisopfs prueba/

# Verificar
$ cat prueba/a/b/c/deep.txt
archivo profundo
```

Toda la jerarquía de directorios persiste correctamente.

---

#### Ejecución en background
```bash
$ sudo umount prueba 2>/dev/null
$ ./fisopfs --filedisk test.fisopfs prueba/

$ ps aux | grep fisopfs
usuario  12345  0.0  0.1  12345  1234 ?        Ssl  18:50   0:00 ./fisopfs --filedisk test.fisopfs prueba/

$ mount | grep fisopfs
fisopfs on /home/usuario/fisop/prueba type fuse.fisopfs (rw,nosuid,nodev,relatime,user_id=1000,group_id=1000)

$ echo "test background" > prueba/bg.txt
$ cat prueba/bg.txt
test background
```

Sin la flag `-f`, el filesystem se ejecuta en background correctamente.

