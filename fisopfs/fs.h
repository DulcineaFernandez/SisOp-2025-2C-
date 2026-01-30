#ifndef FS_H
#define FS_H

#include <sys/types.h>
#include <time.h>


#define MAX_INODOS 100 
#define MAX_BYTES 4096 
#define MAX_NOMBRE 255 
#define MAX_PATH 512 

#define LIBRE 0
#define OCUPADO 1

#define TODO_OK 0
#define DEVOLVER_ERROR -1

typedef enum { TIPO_ARCHIVO, TIPO_DIRECTORIO, TIPO_VACIO } tipo_inodo_t;

typedef struct {
   tipo_inodo_t inodo_type;  
   char nombre_archivo[MAX_NOMBRE];    
   char ruta[MAX_PATH];    
   char ruta_padre[MAX_PATH];  
   uid_t usuario;  
   gid_t grupo;    
   mode_t permisos_archivo;    
   char contenido_archivo[MAX_BYTES];    
   size_t tamanio_archivo;  
   int cantidad_enlaces;   
   time_t tiempo_creacion;                 
   time_t tiempo_modificacion;            
   time_t tiempo_ultimo_acceso;           
} inodo_t;

typedef struct {
   inodo_t inodos[MAX_INODOS]; 
   int estados_inodos[MAX_INODOS]; 
   size_t inodos_usados; 
} super_block_t;


extern super_block_t fs; 

// OPERACIONES PARA ARCHIVOS Y DIRECTORIOS

int create(const char *path,  tipo_inodo_t tipo);

// Borra un archivo del filesystem
/* El archivo debe existir, se libera su inodo y se limpia su contenido, devuelve 0 si tuvo éxito, -1 si hubo error */
int delete(const char *path);

// Lee datos de un archivo
/* El archivo debe existir, copia hasta 'size' bytes desde 'offset' (desde donde empieaza a leer) a 'buf' (hasta donde lee), actualiza tiempo de acceso, devuelve cantidad de bytes leídos, -1 si hubo error */
int read_file(const char *path, char *buf, size_t size, off_t offset);

// Escribe datos en un archivo
/* El archivo debe existir, escribe 'size' bytes desde 'buf' a 'offset', actualiza tamanio y tiempo de modificacion, devuelve
 la cantidad de bytes que escribio si tuvo éxito, -1 si hubo error */
int write_file(const char *path, const char *buf, size_t size, off_t offset);

// Cambia el tamanio del archivo
/* El archivo debe existir, trunca o expande según 'new_size', actualiza tamanio y tiempo de modificacion, devuelve 0 si tuvo éxito, -1 si hubo error */
int truncate_file(const char *path, off_t new_size);

// Lista los nombres de archivos y directorios dentro de un directorio
/* El directorio debe existir, devuelve cantidad de entradas copiadas, -1 si hubo error */
int list_dir(const char *path, char **names, int max_entradas);

//OPERACIONES PARA PERSISTENCIA 
// Inicializa un filesystem vacío con el directorio raíz "/"
/*Se crea un super_block con un único inodo para "/"
   Devuelve 0 si se creó correctamente, -1 si hubo error */
int fs_init_empty(void);

// Carga el filesystem desde un archivo binario
/* El archivo existe y es accesible
   Se llena el super_block en memoria con los datos del archivo
   Devuelve 0 si se cargó correctamente, -1 si hubo error */
int fs_load_from_file(const char *path);

// Guarda el filesystem en un archivo binario
/* El super_block en memoria está inicializado
   Se escribe todo el super_block al archivo
   Devuelve 0 si se guardó correctamente, -1 si hubo error */
int fs_save_to_file(const char *path);

//Devuelve el puntero al inodo con el path dado, o NULL si no existe
static inodo_t *fs_get_inode(const char *path);

// Devuelve el índice del inodo con el path dado, o -1 si no existe
/*  El path debe ser válido*/
int fs_get_inode_index(const char *path);

#endif // FS_H
