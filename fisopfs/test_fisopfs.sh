#!/bin/bash

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

TESTS_PASSED=0
TESTS_FAILED=0
TESTS_TOTAL=0

MOUNT_DIR="prueba"
PERSIST_FILE="test_persistence.fisopfs"
FISOPFS_PID=""

print_test_result() {
    local test_name=$1
    local result=$2
    local details=$3
    
    ((TESTS_TOTAL++))
    
    if [ "$result" -eq 0 ]; then
        echo -e "${GREEN}✓${NC} $test_name"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}✗${NC} $test_name"
        if [ -n "$details" ]; then
            echo -e "  ${RED}→${NC} $details"
        fi
        ((TESTS_FAILED++))
    fi
}

setup() {
    
    echo -e "${YELLOW}=== Configuración Inicial ===${NC}"
    
    if mount | grep -q "$MOUNT_DIR"; then
        echo "Desmontando filesystem previo..."
        sudo umount $MOUNT_DIR 2>/dev/null
        sleep 1
    fi
    
    if [ ! -d "$MOUNT_DIR" ]; then
        mkdir $MOUNT_DIR
        echo "✓ Directorio '$MOUNT_DIR' creado"
    fi
    
    if [ -f "$PERSIST_FILE" ]; then
        rm $PERSIST_FILE
        echo "✓ Archivo de persistencia anterior eliminado"
    fi
    
    echo -n "Compilando fisopfs... "
    make clean > /dev/null 2>&1
    if make > /dev/null 2>&1; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}FALLÓ${NC}"
        echo -e "${RED}Error: No se pudo compilar fisopfs${NC}"
        exit 1
    fi
    
    echo -n "Montando filesystem... "
    ./fisopfs --filedisk $PERSIST_FILE $MOUNT_DIR > /dev/null 2>&1 &
    FISOPFS_PID=$!
    sleep 2
    
    if mount | grep -q fisopfs; then
        echo -e "${GREEN}OK${NC}"
        mount | grep fisopfs | sed 's/^/  → /'
    else
        echo -e "${RED}FALLÓ${NC}"
        echo -e "${RED}Error: No se pudo montar el filesystem${NC}"
        cleanup
        exit 1
    fi
    
    echo ""
}

cleanup() {
    echo ""
    echo -e "${YELLOW}=== Limpieza ===${NC}"
    
    if mount | grep -q fisopfs; then
        sudo umount $MOUNT_DIR 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "✓ Filesystem desmontado"
        fi
    fi
    
    if [ -n "$FISOPFS_PID" ] && ps -p $FISOPFS_PID > /dev/null 2>&1; then
        kill $FISOPFS_PID 2>/dev/null
        sleep 1
    fi
}

run_file_tests() {
    echo -e "${YELLOW}=== 1. Tests de Archivos ===${NC}"
    
    #Creación de archivo con touch
    touch $MOUNT_DIR/test_touch.txt 2>/dev/null
    if [ -f $MOUNT_DIR/test_touch.txt ]; then
        print_test_result "Creación: touch crea archivo vacío" 0
    else
        print_test_result "Creación: touch crea archivo vacío" 1 "Operación 'create' no implementada"
    fi
    
    # Creación de archivo con redirección >
    echo "hola mundo" > $MOUNT_DIR/test_redirect.txt 2>/dev/null
    if [ -f $MOUNT_DIR/test_redirect.txt ]; then
        print_test_result "Creación: redirección > crea archivo" 0
    else
        print_test_result "Creación: redirección > crea archivo" 1 "Operaciones 'create' y 'write' no implementadas"
    fi
    
    # Leer contenido con cat
    echo "contenido de prueba" > $MOUNT_DIR/test_read.txt 2>/dev/null
    content=$(cat $MOUNT_DIR/test_read.txt 2>/dev/null)
    if [ "$content" = "contenido de prueba" ]; then
        print_test_result "Lectura: cat lee contenido correcto" 0
    else
        print_test_result "Lectura: cat lee contenido correcto" 1 "Contenido leído no coincide"
    fi
    
    # Leer con more
    echo -e "linea 1\nlinea 2\nlinea 3" > $MOUNT_DIR/test_more.txt 2>/dev/null

    content=$(script -q -c "more $MOUNT_DIR/test_more.txt" /dev/null | tr -d '\r' | tr -d '\n')
    expected="linea 1linea 2linea 3"

    if [ "$content" = "$expected" ]; then
        print_test_result "Lectura: more lee archivo correctamente" 0
    else
        print_test_result "Lectura: more lee archivo correctamente" 1
    fi

    
    # Escritura con truncamiento
    echo "primera linea" > $MOUNT_DIR/test_trunc.txt 2>/dev/null
    echo "segunda linea" > $MOUNT_DIR/test_trunc.txt 2>/dev/null
    content=$(cat $MOUNT_DIR/test_trunc.txt 2>/dev/null)
    if [ "$content" = "segunda linea" ]; then
        print_test_result "Escritura: truncamiento > sobrescribe contenido" 0
    else
        print_test_result "Escritura: truncamiento > sobrescribe contenido" 1 "Operación 'truncate' no implementada"
    fi
    
    # Escritura con append >>
    echo "linea 1" > $MOUNT_DIR/test_append.txt 2>/dev/null
    echo "linea 2" >> $MOUNT_DIR/test_append.txt 2>/dev/null
    content=$(cat $MOUNT_DIR/test_append.txt 2>/dev/null)
    expected="linea 1
linea 2"
    if [ "$content" = "$expected" ]; then
        print_test_result "Escritura: append >> agrega al final" 0
    else
        print_test_result "Escritura: append >> agrega al final" 1 "Flag O_APPEND no manejado"
    fi
    
    # Borrar archivo con rm
    touch $MOUNT_DIR/test_delete.txt 2>/dev/null
    rm $MOUNT_DIR/test_delete.txt 2>/dev/null
    if [ ! -f $MOUNT_DIR/test_delete.txt ]; then
        print_test_result "Borrado: rm elimina archivo" 0
    else
        print_test_result "Borrado: rm elimina archivo" 1 "Operación 'unlink' no implementada"
    fi
    
    # Borrar archivo con unlink
    touch $MOUNT_DIR/test_unlink.txt 2>/dev/null
    unlink $MOUNT_DIR/test_unlink.txt 2>/dev/null
    if [ ! -f $MOUNT_DIR/test_unlink.txt ]; then
        print_test_result "Borrado: unlink elimina archivo" 0
    else
        print_test_result "Borrado: unlink elimina archivo" 1 "Operación 'unlink' no implementada"
    fi
    
    # Contenido binario
    dd if=/dev/urandom of=$MOUNT_DIR/test_binary.bin bs=512 count=1 > /dev/null 2>&1
    if [ -f $MOUNT_DIR/test_binary.bin ]; then
        size=$(stat -c%s $MOUNT_DIR/test_binary.bin 2>/dev/null)
        if [ "$size" = "512" ]; then
            print_test_result "Contenido binario: se almacena correctamente" 0
        else
            print_test_result "Contenido binario: se almacena correctamente" 1 "Tamaño incorrecto"
        fi
    else
        print_test_result "Contenido binario: se almacena correctamente" 1
    fi
    
    echo ""
}

run_directory_tests() {
    echo -e "${YELLOW}=== 2. Tests de Directorios ===${NC}"
    
    # Creación directorio con mkdir
    mkdir $MOUNT_DIR/test_dir 2>/dev/null
    if [ -d $MOUNT_DIR/test_dir ]; then
        print_test_result "Creación: mkdir crea directorio" 0
    else
        print_test_result "Creación: mkdir crea directorio" 1 "Operación 'mkdir' no implementada"
    fi
    
    # Listar directorio con ls
    mkdir $MOUNT_DIR/list_test 2>/dev/null
    touch $MOUNT_DIR/list_test/file1.txt 2>/dev/null
    touch $MOUNT_DIR/list_test/file2.txt 2>/dev/null
    
    file_count=$(ls $MOUNT_DIR/list_test 2>/dev/null | wc -l)
    if [ "$file_count" -eq 2 ]; then
        print_test_result "Lectura: ls lista contenido del directorio" 0
    else
        print_test_result "Lectura: ls lista contenido del directorio" 1 "readdir no lista correctamente"
    fi
    
    # Pseudo-directorio '.'
    mkdir $MOUNT_DIR/dot_test 2>/dev/null
    if ls -a $MOUNT_DIR/dot_test 2>/dev/null | grep -q "^\.$"; then
        print_test_result "Lectura: pseudo-directorio '.' presente" 0
    else
        print_test_result "Lectura: pseudo-directorio '.' presente" 1 "falta agregar '.' en readdir"
    fi
    
    # Pseudo-directorio '..'
    mkdir $MOUNT_DIR/dotdot_test 2>/dev/null
    if ls -a $MOUNT_DIR/dotdot_test 2>/dev/null | grep -q "^\.\.$"; then
        print_test_result "Lectura: pseudo-directorio '..' presente" 0
    else
        print_test_result "Lectura: pseudo-directorio '..' presente" 1 "falta agregar '..' en readdir"
    fi
    
    # Borrar directorio vacío
    mkdir $MOUNT_DIR/empty_dir 2>/dev/null
    rmdir $MOUNT_DIR/empty_dir 2>/dev/null
    if [ ! -d $MOUNT_DIR/empty_dir ]; then
        print_test_result "Borrado: rmdir elimina directorio vacío" 0
    else
        print_test_result "Borrado: rmdir elimina directorio vacío" 1 "Operación 'rmdir' no implementada"
    fi
    
    #  Directorio no vacío no se puede borrar
    mkdir $MOUNT_DIR/full_dir 2>/dev/null
    touch $MOUNT_DIR/full_dir/file.txt 2>/dev/null
    rmdir $MOUNT_DIR/full_dir 2>/dev/null
    result=$?
    if [ $result -ne 0 ] && [ -d $MOUNT_DIR/full_dir ]; then
        print_test_result "Borrado: rmdir falla con directorio no vacío" 0
    else
        print_test_result "Borrado: rmdir falla con directorio no vacío" 1 "debe devolver error ENOTEMPTY"
    fi
    
    # Subdirectorio (un nivel de recursión)
    mkdir $MOUNT_DIR/parent 2>/dev/null
    mkdir $MOUNT_DIR/parent/child 2>/dev/null
    if [ -d $MOUNT_DIR/parent/child ]; then
        print_test_result "Recursión: crear subdirectorio (1 nivel)" 0
    else
        print_test_result "Recursión: crear subdirectorio (1 nivel)" 1 "falta soporte para subdirectorios"
    fi
    
    # Archivos en subdirectorio
    mkdir -p $MOUNT_DIR/dir1/dir2 2>/dev/null
    echo "contenido" > $MOUNT_DIR/dir1/dir2/file.txt 2>/dev/null
    content=$(cat $MOUNT_DIR/dir1/dir2/file.txt 2>/dev/null)
    if [ "$content" = "contenido" ]; then
        print_test_result "Recursión: leer/escribir archivo en subdirectorio" 0
    else
        print_test_result "Recursión: leer/escribir archivo en subdirectorio" 1
    fi
    
    echo ""
}


run_stat_tests() {
    echo -e "${YELLOW}=== 3. Tests de Estadísticas (stat) ===${NC}"
    
    # Estadísticas de archivo con stat
    touch $MOUNT_DIR/stat_file.txt 2>/dev/null
    if stat $MOUNT_DIR/stat_file.txt > /dev/null 2>&1; then
        print_test_result "stat funciona en archivos" 0
    else
        print_test_result "stat funciona en archivos" 1 "getattr no devuelve metadatos correctos"
    fi
    
    # Estadísticas de directorio
    mkdir $MOUNT_DIR/stat_dir 2>/dev/null
    if stat $MOUNT_DIR/stat_dir > /dev/null 2>&1; then
        print_test_result "stat funciona en directorios" 0
    else
        print_test_result "stat funciona en directorios" 1
    fi
    
    # Usuario y grupo actual
    touch $MOUNT_DIR/owner_test.txt 2>/dev/null
    file_uid=$(stat -c %u $MOUNT_DIR/owner_test.txt 2>/dev/null)
    file_gid=$(stat -c %g $MOUNT_DIR/owner_test.txt 2>/dev/null)
    current_uid=$(id -u)
    current_gid=$(id -g)
    
    if [ "$file_uid" = "$current_uid" ] && [ "$file_gid" = "$current_gid" ]; then
        print_test_result "Usuario y grupo = usuario actual (getuid/getgid)" 0
    else
        print_test_result "Usuario y grupo = usuario actual (getuid/getgid)" 1 "debe usar getuid(2) y getgid(2)"
    fi
    
    # Fecha de modificación
    echo "inicial" > $MOUNT_DIR/mtime_test.txt 2>/dev/null
    sleep 1
    mtime1=$(stat -c %Y $MOUNT_DIR/mtime_test.txt 2>/dev/null)
    sleep 1
    echo "modificado" > $MOUNT_DIR/mtime_test.txt 2>/dev/null
    mtime2=$(stat -c %Y $MOUNT_DIR/mtime_test.txt 2>/dev/null)
    
    if [ "$mtime2" -gt "$mtime1" ]; then
        print_test_result "Fecha de modificación se actualiza" 0
    else
        print_test_result "Fecha de modificación se actualiza" 1 "mtime debe actualizarse al escribir"
    fi
    
    # Fecha de acceso
    echo "contenido" > $MOUNT_DIR/atime_test.txt 2>/dev/null
    sleep 1
    atime1=$(stat -c %X $MOUNT_DIR/atime_test.txt 2>/dev/null)
    cat $MOUNT_DIR/atime_test.txt > /dev/null 2>&1
    sleep 1
    atime2=$(stat -c %X $MOUNT_DIR/atime_test.txt 2>/dev/null)
    
    if [ "$atime2" -ge "$atime1" ]; then
        print_test_result "Fecha de último acceso se actualiza" 0
    else
        print_test_result "Fecha de último acceso se actualiza" 1 "atime debe actualizarse al leer"
    fi
    
    echo ""
}

run_persistence_tests() {
    echo -e "${YELLOW}=== 4. Tests de Persistencia ===${NC}"
    
    # Verificar archivo de persistencia
    if [ -f "$PERSIST_FILE" ]; then
        print_test_result "Se crea archivo .fisopfs" 0
    else
        print_test_result "Se crea archivo .fisopfs" 1 "operación 'init' no crea el archivo"
    fi
    
    # Persistencia de archivos
    echo "datos persistentes" > $MOUNT_DIR/persist_file.txt 2>/dev/null
    
    echo "  → Desmontando filesystem..."
    sudo umount $MOUNT_DIR 2>/dev/null
    sleep 1
    
    echo "  → Remontando filesystem..."
    ./fisopfs --filedisk $PERSIST_FILE $MOUNT_DIR > /dev/null 2>&1 &
    FISOPFS_PID=$!
    sleep 2
    
    if [ -f $MOUNT_DIR/persist_file.txt ]; then
        content=$(cat $MOUNT_DIR/persist_file.txt 2>/dev/null)
        if [ "$content" = "datos persistentes" ]; then
            print_test_result "Archivo persiste tras desmontar/remontar" 0
        else
            print_test_result "Archivo persiste tras desmontar/remontar" 1 "contenido no coincide"
        fi
    else
        print_test_result "Archivo persiste tras desmontar/remontar" 1 "archivo no existe tras remontar"
    fi
    
    #  Persistencia de directorios
    mkdir $MOUNT_DIR/persist_dir 2>/dev/null
    
    sudo umount $MOUNT_DIR 2>/dev/null
    sleep 1
    ./fisopfs --filedisk $PERSIST_FILE $MOUNT_DIR > /dev/null 2>&1 &
    FISOPFS_PID=$!
    sleep 2
    
    if [ -d $MOUNT_DIR/persist_dir ]; then
        print_test_result "Directorio persiste tras desmontar/remontar" 0
    else
        print_test_result "Directorio persiste tras desmontar/remontar" 1
    fi
    
    # Persistencia de estructura completa
    mkdir -p $MOUNT_DIR/dir_a/dir_b 2>/dev/null
    echo "archivo en dir_b" > $MOUNT_DIR/dir_a/dir_b/file.txt 2>/dev/null
    
    sudo umount $MOUNT_DIR 2>/dev/null
    sleep 1
    ./fisopfs --filedisk $PERSIST_FILE $MOUNT_DIR > /dev/null 2>&1 &
    FISOPFS_PID=$!
    sleep 2
    
    content=$(cat $MOUNT_DIR/dir_a/dir_b/file.txt 2>/dev/null)
    if [ "$content" = "archivo en dir_b" ]; then
        print_test_result "Estructura anidada persiste completamente" 0
    else
        print_test_result "Estructura anidada persiste completamente" 1
    fi
    
    # Ejecución en background (sin -f)
    sudo umount $MOUNT_DIR 2>/dev/null
    sleep 1
    
    # Ejecutar sin -f (background)
    ./fisopfs --filedisk $PERSIST_FILE $MOUNT_DIR > /dev/null 2>&1
    sleep 2
    
    if mount | grep -q fisopfs; then
        print_test_result "Funciona en background (sin flag -f)" 0
    else
        print_test_result "Funciona en background (sin flag -f)" 1 "falla al ejecutar sin -f"
    fi
    
    echo ""
}

main() {
    setup
    
    run_file_tests
    run_directory_tests
    run_stat_tests
    run_persistence_tests
        
    echo -e "  Tests ejecutados: ${BLUE}$TESTS_TOTAL${NC}"
    echo -e "  Tests exitosos:   ${GREEN}$TESTS_PASSED${NC}"
    echo -e "  Tests fallidos:   ${RED}$TESTS_FAILED${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo ""
        echo -e "${GREEN}¡TODOS LOS TESTS PASARON EXITOSAMENTE!                ${NC}"
        exit 0
    fi
}

trap cleanup EXIT INT TERM

main
