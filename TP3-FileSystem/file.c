#include "file.h"
#include <string.h> /* memset */
#include "diskimg.h"
#include "inode.h"

/**
 * Fetches the specified file block from the specified inode.
 *
 * @param fs       Puntero al sistema de archivos abierto.
 * @param inumber  Número de inodo del archivo que queremos leer.
 * @param blockNo  Número (0‑based) del bloque de datos dentro del archivo.
 * @param buf      Buffer donde se copiará el bloque leído (tamaño DISKIMG_SECTOR_SIZE).
 *
 * @return  N º de bytes válidos en el bloque (<= DISKIMG_SECTOR_SIZE),
 *          0 si el bloque solicitado está más allá del EOF,
 *         -1 en caso de error.
 */
int file_getblock(struct unixfilesystem *fs, int inumber, int blockNo, void *buf) {

    if (blockNo < 0) return -1;

    /* Obtener el inodo */
    struct inode in;
    if (inode_iget(fs, inumber, &in) < 0) {
        return -1;
    }

    /* Calcular tamaño total del archivo en bytes */
    int filesize = inode_getsize(&in);

    /* Calcular desplazamiento inicial del bloque dentro del archivo */
    int fileOffset = blockNo * DISKIMG_SECTOR_SIZE;

    /* Si el offset inicial está más allá del EOF, no hay datos válidos */
    if (fileOffset >= filesize) {
        return 0; // bloque fuera del archivo
    }

    /* Averiguar sector real asociado al bloqueNo */
    int diskBlock = inode_indexlookup(fs, &in, blockNo);
    if (diskBlock < 0) {
        return -1; // no se encontró el bloque
    }

    /* Leer sector físico completo */
    if (diskimg_readsector(fs->dfd, diskBlock, buf) < 0) {
        return -1; // error de E/S
    }

    /* Determinar cuántos bytes de este bloque son válidos (último bloque puede ser parcial) */
    int bytesLeft = filesize - fileOffset;
    if (bytesLeft > DISKIMG_SECTOR_SIZE) bytesLeft = DISKIMG_SECTOR_SIZE;

        /* Si el bloque está parcialmente lleno, rellenamos el resto con ceros para
     * que los consumidores (p.ej. checksum) vean bytes determinísticos. */
    if (bytesLeft < DISKIMG_SECTOR_SIZE) {
        memset((char *)buf + bytesLeft, 0, DISKIMG_SECTOR_SIZE - bytesLeft);
    }

    return bytesLeft;
}
