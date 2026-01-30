#define FUSE_USE_VERSION 26
#define _GNU_SOURCE

#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/file.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <utime.h>
#include "fs.h" //Incluye las funciones de fs.c

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif


#define DEFAULT_FILE_DISK "persistence_file.fisopfs"

// absolute path for persistence file used in
// `.init` and `.destroy` FUSE operations
static char filedisk_path[2 * PATH_MAX];

//buscar inodo por path en el superblock global
static inodo_t *encontrar_inodo(const char *path)
{
    if (!path) return NULL;

    for (size_t i = 0; i < MAX_INODOS; i++) {
        if (fs.estados_inodos[i] == OCUPADO &&
            strcmp(fs.inodos[i].ruta, path) == 0) {
            return &fs.inodos[i];
        }
    }
    return NULL;
}


//callbacks de FUSE

//chmod/chown/utimens: son operaciones requeridas por FUSE pero no las implementamos.

static int fisopfs_chmod(const char *path, mode_t mode)
{
    printf("[debug] chmod %s -> %o\n", path, mode);
    return 0; // ignoramos, pero no fallamos
}

static int fisopfs_chown(const char *path, uid_t uid, gid_t gid)
{
    printf("[debug] chown %s -> uid=%d gid=%d\n", path, uid, gid);
    return 0; // ignoramos, pero no fallamos
}

static int fisopfs_utimens(const char *path, const struct utimbuf *tv)
{
    printf("[debug] utimens %s\n", path);
    return 0; // ignoramos
}


//llenamos un struct con los metadatos del inodo, lo necesita el FUSE
static int fisopfs_getattr(const char *path, struct stat *st)
{
    printf("[debug] getattr %s\n", path);
    memset(st, 0, sizeof(struct stat));

    inodo_t *ino = encontrar_inodo(path);
    if (!ino) {
        return -ENOENT;
    }

    st->st_uid  = ino->usuario;
    st->st_gid  = ino->grupo;

    if (ino->inodo_type == TIPO_DIRECTORIO) {
        st->st_mode = S_IFDIR | ino->permisos_archivo;
    } else if (ino->inodo_type == TIPO_ARCHIVO) {
        st->st_mode = S_IFREG | ino->permisos_archivo;
    } else {
        //inodo vacio / invalido
        return -ENOENT;
    }

    st->st_nlink = ino->cantidad_enlaces;
    st->st_size  = ino->tamanio_archivo;

    st->st_atime = ino->tiempo_ultimo_acceso;
    st->st_mtime = ino->tiempo_modificacion;
    st->st_ctime = ino->tiempo_creacion;

    return 0;
}

//listamos el contenido de un directorio
static int fisopfs_readdir(const char *path,
                           void *buffer,
                           fuse_fill_dir_t filler,
                           off_t offset,
                           struct fuse_file_info *fi)
{
    (void) offset;
    (void) fi;

    printf("[debug] readdir %s\n", path);

    char nombres[128][MAX_NOMBRE];
    char *ptrs[128];

    for (int i = 0; i < 128; i++) {
        ptrs[i] = nombres[i];
    }

    int contador = list_dir(path, ptrs, 128);
    if (contador < 0) {
        return -ENOENT;
    }

    for (int i = 0; i < contador; i++) {
        filler(buffer, ptrs[i], NULL, 0);
    }

    return 0;
}

//para la lectura de archivos delegamos a read_file y devolvemos la cantidad de bytes leidos
static int fisopfs_read(const char *path,
                        char *buffer,
                        size_t size,
                        off_t offset,
                        struct fuse_file_info *fi)
{
    (void) fi;
    printf("[debug] read %s offset=%ld size=%lu\n", path, (long)offset, (unsigned long)size);

    int n = read_file(path, buffer, size, offset);
    if (n < 0) return -EIO;

    return n;
}

//para la escritura de archivos delegamos a write_file
static int fisopfs_write(const char *path,
                         const char *buf,
                         size_t size,
                         off_t offset,
                         struct fuse_file_info *fi)
{
    (void) fi;
    printf("[debug] write %s offset=%ld size=%lu\n", path, (long)offset, (unsigned long)size);

    int r = write_file(path, buf, size, offset);
    if (r < 0) return -EIO;

    return r;
}

//para la creacion de archivos delegamos a create
static int fisopfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    (void) mode;
    (void) fi;

    printf("[debug] create %s\n", path);

    int res = create(path, TIPO_ARCHIVO);
    if (res != TODO_OK) {
        return -EEXIST;
    }

    return 0;
}

// para la creacion de archivos regulares delegamos a create
static int fisopfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    (void) rdev;
    printf("[debug] mknod %s\n", path);

    if (S_ISREG(mode)) {
        int res = create(path, TIPO_ARCHIVO);
        if (res != TODO_OK) return -EEXIST;
        return 0;
    }

    return -EPERM;
}

//para la creacion de directorios delegamos a create, obvio con tipo directorio
static int fisopfs_mkdir(const char *path, mode_t mode)
{
    (void) mode;
    printf("[debug] mkdir %s\n", path);

    int res = create(path, TIPO_DIRECTORIO);
    if (res != TODO_OK) {
        return -EEXIST;
    }

    return 0;
}

//para borrar archivos delegamos a delete
static int fisopfs_unlink(const char *path)
{
    printf("[debug] unlink %s\n", path);

    int res = delete(path);
    if (res != TODO_OK) {
        return -ENOENT;
    }

    return 0;
}

//para borrar directorios, verificamos que esta vacio y despues delete
static int fisopfs_rmdir(const char *path)
{
    printf("[debug] rmdir %s\n", path);

    char nombres[128][MAX_NOMBRE];
    char *ptrs[128];

    for (int i = 0; i < 128; i++) {
        ptrs[i] = nombres[i];
    }

    int contador = list_dir(path, ptrs, 128);
    if (contador < 0) {
        return -ENOENT;
    }

    int entradas_reales = 0;
    for (int i = 0; i < contador; i++) {
        if (strcmp(ptrs[i], ".") != 0 && strcmp(ptrs[i], "..") != 0) {
            entradas_reales++;
        }
    }

    if (entradas_reales > 0) {
        return -ENOTEMPTY;
    }

    int res = delete(path);
    if (res != TODO_OK) {
        return -ENOENT;
    }

    return 0;
}

//para cambiar el tamaño de un archivo
static int fisopfs_truncate(const char *path, off_t size)
{
    printf("[debug] truncate %s size=%ld\n", path, (long)size);

    int res = truncate_file(path, size);
    if (res != TODO_OK) {
        return -EIO;
    }

    return 0;
}

//aca solo verificamos que exista y no sea un directorio
static int fisopfs_open(const char *path, struct fuse_file_info *fi)
{
    (void) fi;
    printf("[debug] open %s\n", path);

    inodo_t *ino = encontrar_inodo(path);
    if (!ino) return -ENOENT;
    if (ino->inodo_type == TIPO_DIRECTORIO) return -EISDIR;

    return 0;
}

//guardamos el fs en disco cada tanto, porque FUSE pide sincronizacion, para persistencia
static int fisopfs_flush(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    (void) fi;

    printf("[debug] flush\n");
    fs_save_to_file(filedisk_path);
    return 0;
}

//se ejecuta al montar el filesystem, cargamos el superblock desde el archivo de persistencia
//o inicializamos vacio si el archivo no existe
static void *fisopfs_init(struct fuse_conn_info *conn)
{
    (void) conn;

    printf("[debug] fisopfs_init - cargando desde: %s\n", filedisk_path);

    int r = fs_load_from_file(filedisk_path);
    if (r != TODO_OK) {
        printf("[debug] fs_load_from_file fallo, inicializando vacío\n");
        fs_init_empty();
    }

    return NULL;
}

//antes de desmontar, FUSE, guarda automaticamente el superblock en el archivo de persistencia
static void fisopfs_destroy(void *private_data)
{
    (void) private_data;

    printf("[debug] fisopfs_destroy - guardando en: %s\n", filedisk_path);
    fs_save_to_file(filedisk_path);
}

//tabla de operaciones
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


//parseamos argumentos para elegir el archivo de persistencia
//construimos su path absoluto, y despues delegamos todo a fuse_main
//que no vuelve hasta que no se desmonte el fs.
int
main(int argc, char *argv[])
{
	char *filedisk_name = DEFAULT_FILE_DISK;

	for (int i = 1; i < argc - 1; i++) {
		if (strcmp(argv[i], "--filedisk") == 0) {
			filedisk_name = argv[i + 1];

			// we remove the argument so that FUSE doesn't use our
			// argument or name as folder.
			//
			// equivalent to a pop.
			for (int j = i; j < argc - 1; j++) {
				argv[j] = argv[j + 2];
			}

			argc = argc - 2;
			break;
		}
	}
    
    if (filedisk_name[0] == '/') {
        strncpy(filedisk_path, filedisk_name, sizeof(filedisk_path) - 1);
        filedisk_path[sizeof(filedisk_path) - 1] = '\0';
    } else {
        char cwd[PATH_MAX];
        if (!getcwd(cwd, sizeof(cwd))) {
            perror("getcwd");
            return 1;
        }
        snprintf(filedisk_path, sizeof(filedisk_path),
                 "%s/%s", cwd, filedisk_name);
        filedisk_path[sizeof(filedisk_path) - 1] = '\0';
    }

    return fuse_main(argc, argv, &operations, NULL);
}
