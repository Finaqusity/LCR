/* newlib 系统调用桩（§8.4：修掉 nosys 链接警告）。
 * 无 OS、无文件系统、heap=0（禁 malloc）。stdout/stderr 转发到 UART。
 * 这些只为满足 libc 链接；固件正常输出走 uart_printf，不经 stdio。 */

#include <stdint.h>
#include <stddef.h>
#include <errno.h>
#include <sys/stat.h>
#include "uart.h"

int _close(int fd)
{
    (void)fd;
    return -1;
}

int _fstat(int fd, struct stat *st)
{
    (void)fd;
    if (st != 0) { st->st_mode = S_IFCHR; }
    return 0;
}

int _getpid(void)
{
    return 1;
}

int _isatty(int fd)
{
    (void)fd;
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid; (void)sig;
    errno = EINVAL;
    return -1;
}

int _lseek(int fd, int ptr, int dir)
{
    (void)fd; (void)ptr; (void)dir;
    return 0;
}

int _read(int fd, char *ptr, int len)
{
    (void)fd; (void)ptr; (void)len;
    return 0;   /* EOF：命令输入走 uart_read_byte，不经 stdio */
}

int _write(int fd, char *ptr, int len)
{
    if ((fd == 1) || (fd == 2)) {
        if ((ptr != 0) && (len > 0)) {
            uart_write_bytes((const uint8_t *)ptr, (uint32_t)len);
        }
        return len;
    }
    errno = EBADF;
    return -1;
}

/* heap=0：任何 malloc/sbrk 请求都失败（§8.1 禁动态分配）。 */
void *_sbrk(int incr)
{
    (void)incr;
    errno = ENOMEM;
    return (void *)-1;
}

void _exit(int code)
{
    (void)code;
    for (;;) { /* 停机 */ }
}