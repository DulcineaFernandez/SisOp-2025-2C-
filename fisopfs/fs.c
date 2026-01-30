#include "fs.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <time.h>
#include <libgen.h>
#include <stdbool.h>
#include <unistd.h>


super_block_t fs;

// Devuelve el índice de un inodo libre, o -1 si están todos ocupados
int buscar_indice_libre() {
    for (size_t i = 0; i < MAX_INODOS; i++) {
        if (fs.estados_inodos[i] == LIBRE) {
            return i;
        }
    }
    return -1;
}

// Inicializa todos los datos de un inodo nuevo
//Pre: El indice debe estar libre 
inodo_t* inicializar_datos_inodo_nuevo(size_t indice, const char *path, tipo_inodo_t tipo) {
    //Apunto a la dirección libre y la marco como ocupada y sumo uno al usados
    inodo_t *inodo_nuevo = &fs.inodos[indice];
    fs.estados_inodos[indice] = OCUPADO;
    fs.inodos_usados++;

    //incializo el tipo y la ruta completa del inodo 
    inodo_nuevo->inodo_type = tipo;
    strncpy(inodo_nuevo->ruta, path, MAX_PATH - 1);
    inodo_nuevo->ruta[MAX_PATH - 1] = '\0';

    //incializo el nombre del inodo 
    const char *ultimo_slash = strrchr(path, '/');
    if (ultimo_slash != NULL) {
        // Hay '/', el nombre empieza después de la última '/'
        strncpy(inodo_nuevo->nombre_archivo, ultimo_slash + 1, MAX_NOMBRE - 1);
        inodo_nuevo->nombre_archivo[MAX_NOMBRE - 1] = '\0';
    } else {
        // No hay '/', todo el path es el nombre
        strncpy(inodo_nuevo->nombre_archivo, path, MAX_NOMBRE - 1);
        inodo_nuevo->nombre_archivo[MAX_NOMBRE - 1] = '\0';
    }

    // Extraer ruta del directorio padre
    if (ultimo_slash != NULL) {
        size_t len = ultimo_slash - path; // distancia hasta la última '/'
        if (len == 0) {
            // Padre es la raíz "/"
            strcpy(inodo_nuevo->ruta_padre, "/");
        } else {
            strncpy(inodo_nuevo->ruta_padre, path, len);
            inodo_nuevo->ruta_padre[len] = '\0';
        }
    } else {
        // No hay '/', padre es la raíz
        strcpy(inodo_nuevo->ruta_padre, "/");
    }
    // Permisos y propietario   
    inodo_nuevo->usuario = getuid();
    inodo_nuevo->grupo = getgid();
    
    if (inodo_nuevo->inodo_type == TIPO_ARCHIVO){
        inodo_nuevo->permisos_archivo = 0644;
    }else{
        inodo_nuevo->permisos_archivo = 0755;
    }
    
    // Contenido y tamanio
    inodo_nuevo->tamanio_archivo= 0;
    memset(inodo_nuevo->contenido_archivo, 0, MAX_BYTES);

    // Tiempos
    time_t ahora = time(NULL);
    inodo_nuevo->tiempo_creacion = ahora;
    inodo_nuevo->tiempo_modificacion = ahora;
    inodo_nuevo->tiempo_ultimo_acceso = ahora;

    inodo_nuevo->cantidad_enlaces = 1;

    return inodo_nuevo;
}


int create(const char *path, tipo_inodo_t tipo) {
    // Verificar parámetros
    if (!path || (tipo != TIPO_ARCHIVO && tipo != TIPO_DIRECTORIO) || strlen(path) >= MAX_PATH) {
        return DEVOLVER_ERROR;
    }

    // Verificar si ya existe
    if (fs_get_inode(path) != NULL) {
        return DEVOLVER_ERROR;
    }

    // Buscar un inodo libre
    int indice_libre = buscar_indice_libre();
    if (indice_libre == -1) {
        return DEVOLVER_ERROR; // No hay espacio
    }

    // Inicializar inodo
    inicializar_datos_inodo_nuevo(indice_libre, path, tipo);

    return TODO_OK;
}

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

int fs_get_inode_index(const char *path) {
    if (path == NULL)
    {
        return DEVOLVER_ERROR;
    } 

    for (size_t i = 0; i < MAX_INODOS; i++) {
        if (fs.estados_inodos[i] == OCUPADO && strcmp(fs.inodos[i].ruta, path) == 0) {
            return i;
        }
    }
    return DEVOLVER_ERROR;
}

// Libera un inodo dado su índice
//Pre: El indice debe estar ocupado y ser válido
void liberar_inodo(size_t indice, inodo_t *inodo) {
    fs.estados_inodos[indice] = LIBRE;
    fs.inodos_usados--;

    inodo->inodo_type = 0;
    inodo->tamanio_archivo = 0;
    inodo->cantidad_enlaces = 0;
    inodo->usuario = 0;
    inodo->grupo = 0;
    inodo->permisos_archivo = 0;
    inodo->nombre_archivo[0] = '\0';
    inodo->ruta[0] = '\0';
    inodo->ruta_padre[0] = '\0';
    inodo->tiempo_creacion = 0;
    inodo->tiempo_modificacion = 0;
    inodo->tiempo_ultimo_acceso = 0;

    for (size_t i = 0; i < MAX_BYTES; i++) {
        inodo->contenido_archivo[i] = 0;
    }
}

// Borra recursivamente el contenido de un directorio
//Pre: El path debe existir
void borrar_contenido_directorio(const char *path) {
    for (size_t indice = 0; indice < MAX_INODOS; indice++) {
        if (fs.estados_inodos[indice] == OCUPADO && strcmp(fs.inodos[indice].ruta_padre, path) == 0) {
            if (fs.inodos[indice].inodo_type == TIPO_DIRECTORIO) {
                borrar_contenido_directorio(fs.inodos[indice].ruta);
            }
            liberar_inodo(indice, &fs.inodos[indice]);
        }
    }
}

int delete(const char *path) {
    int indice = fs_get_inode_index(path);
    if (indice == -1){
        return DEVOLVER_ERROR;
    }

    inodo_t *inodo_a_borrar = fs_get_inode(path);
    if (inodo_a_borrar->inodo_type == TIPO_DIRECTORIO) {
        borrar_contenido_directorio(inodo_a_borrar->ruta);
    } 

    liberar_inodo(indice, inodo_a_borrar);

    return TODO_OK;
}

int read_file(const char *path, char *buf, size_t size, off_t offset) {
    if (buf == NULL) {
        return DEVOLVER_ERROR;
    }

    inodo_t *inode = fs_get_inode(path);
    if (inode == NULL) {
        return DEVOLVER_ERROR;
    }

    if (inode->inodo_type != TIPO_ARCHIVO) {
        return DEVOLVER_ERROR;
    }

    if (offset >= inode->tamanio_archivo) {
        return 0; 
    }

    size_t bytes_disponibles = inode->tamanio_archivo - offset;
    size_t bytes_a_leer;
    if (size < bytes_disponibles) {
        bytes_a_leer = size;
    } else {
        bytes_a_leer = bytes_disponibles;
    }

    for (size_t i = 0; i < bytes_a_leer; i++) {
        buf[i] = inode->contenido_archivo[offset + i];
    }

    inode->tiempo_ultimo_acceso = time(NULL);
    return bytes_a_leer;
}

int write_file(const char *path, const char *buf, size_t size, off_t offset) {
    if (buf == NULL) {
        return DEVOLVER_ERROR;
    }

    inodo_t *inode = fs_get_inode(path);
    if (inode == NULL) {
        return DEVOLVER_ERROR;
    }

    if (inode->inodo_type != TIPO_ARCHIVO) {
        return DEVOLVER_ERROR;
    }

    if ((size_t)offset > inode->tamanio_archivo) {
        return DEVOLVER_ERROR;
    }

    size_t bytes_disponibles = MAX_BYTES - offset;
    size_t bytes_a_escribir;

    if (size < bytes_disponibles) {
        bytes_a_escribir = size;
    } else {
        bytes_a_escribir = bytes_disponibles;
    }

    size_t nuevo_tamanio = offset + bytes_a_escribir;

    if (nuevo_tamanio > inode->tamanio_archivo) {
        truncate_file(path, nuevo_tamanio);
    }

    for (size_t i = 0; i < bytes_a_escribir; i++) {
        inode->contenido_archivo[offset + i] = buf[i];
    }

    inode->tiempo_modificacion = time(NULL);

    return bytes_a_escribir;
}

int truncate_file(const char *path, off_t new_size) {
    inodo_t *inode = fs_get_inode(path);
    if (inode == NULL){
        return DEVOLVER_ERROR;
    }

    if (inode->inodo_type != TIPO_ARCHIVO)
    {
        return DEVOLVER_ERROR; 
    } 

    if (new_size > MAX_BYTES) {
        return DEVOLVER_ERROR; 
    }

    if ((size_t)new_size > inode->tamanio_archivo) {
        for (size_t i = inode->tamanio_archivo; i < new_size; i++) {
            inode->contenido_archivo[i] = 0;
        }
    } else if ((size_t)new_size < inode->tamanio_archivo) {
        for (size_t i = new_size; i < inode->tamanio_archivo; i++) {
            inode->contenido_archivo[i] = 0;
        }
    }

    inode->tamanio_archivo = new_size;
    inode->tiempo_modificacion = time(NULL);

    return TODO_OK;
}

int list_dir(const char *path, char **names, int max_entradas) {
    if (names == NULL || max_entradas <= 0) {
        return DEVOLVER_ERROR;
    }

    inodo_t *dir_inode = fs_get_inode(path);
    if (dir_inode == NULL) {
        return DEVOLVER_ERROR; 
    }

    if (dir_inode->inodo_type != TIPO_DIRECTORIO) {
        return DEVOLVER_ERROR;
    }

    int contador = 0;

    if (contador < max_entradas) {
        strncpy(names[contador], ".", MAX_NOMBRE - 1);
        names[contador][MAX_NOMBRE - 1] = '\0';
        contador++;
    }

    if (contador < max_entradas) {
        strncpy(names[contador], "..", MAX_NOMBRE - 1);
        names[contador][MAX_NOMBRE - 1] = '\0';
        contador++;
    }

    for (size_t i = 0; i < MAX_INODOS && contador < max_entradas; i++) {
        if (fs.estados_inodos[i] == OCUPADO) {
            inodo_t *inode = fs_get_inode(fs.inodos[i].ruta);
            if (strcmp(inode->ruta_padre, path) == 0) {
                strncpy(names[contador], inode->nombre_archivo, MAX_NOMBRE - 1);
                names[contador][MAX_NOMBRE - 1] = '\0';
                contador++;
            }
        }
    }

    return contador;
}

int fs_save_to_file(const char *path){
    if (path == NULL) {
        return DEVOLVER_ERROR;
    }

    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        perror("Error al abrir archivo para guardar");
        return DEVOLVER_ERROR;
    }

    size_t escritos = fwrite(&fs, sizeof(super_block_t), 1, file);
    if (escritos != 1) {
        perror("Error al escribir el filesystem");
        fclose(file);
        return DEVOLVER_ERROR;
    }

    fclose(file);
    return TODO_OK;
}

int fs_load_from_file(const char *path) {
    if (path == NULL) {
        return DEVOLVER_ERROR;
    }

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return fs_init_empty();
    }

    size_t leidos = fread(&fs, sizeof(super_block_t), 1, file);

    if (leidos != 1) {
        perror("Error al leer el archivo de filesystem");
        fclose(file);
        return DEVOLVER_ERROR;
    }

    fclose(file);
    return TODO_OK;
}

int fs_init_empty(void){
    fs.inodos_usados = 0;

    for (int i = 0; i < MAX_INODOS; i++) {
        fs.estados_inodos[i] = LIBRE;
    }

    for (int i = 0; i < MAX_INODOS; i++) {
        inodo_t * inodo_inicializado = &fs.inodos[i];

        inodo_inicializado->inodo_type = TIPO_VACIO;
        inodo_inicializado->nombre_archivo[0] = '\0';
        inodo_inicializado->ruta[0] = '\0';
        inodo_inicializado->ruta_padre[0] = '\0';

        inodo_inicializado->usuario = 0;
        inodo_inicializado->grupo = 0;
        inodo_inicializado->permisos_archivo = 0;

        inodo_inicializado->tiempo_creacion = 0;
        inodo_inicializado->tiempo_modificacion = 0;
        inodo_inicializado->tiempo_ultimo_acceso = 0;

        inodo_inicializado->cantidad_enlaces = 0;
        inodo_inicializado->tamanio_archivo = 0;
    }

    fs.estados_inodos[0] = OCUPADO;
    fs.inodos_usados = 1;

    inodo_t *raiz = &fs.inodos[0];
    raiz->inodo_type = TIPO_DIRECTORIO;
    strcpy(raiz->nombre_archivo, "/");
    strcpy(raiz->ruta, "/");
    strcpy(raiz->ruta_padre, "/");
    raiz->tamanio_archivo = 0;
    raiz->usuario = getuid();
    raiz->grupo = getgid();
    raiz->permisos_archivo = 0755;
    time_t ahora = time(NULL);
    raiz->tiempo_creacion = ahora;
    raiz->tiempo_modificacion = ahora;
    raiz->tiempo_ultimo_acceso = ahora;
    raiz->cantidad_enlaces = 1;

    return TODO_OK;
}
