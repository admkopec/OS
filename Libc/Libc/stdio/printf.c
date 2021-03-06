//
//  printf.c
//  BetaOS
//
//  Created by Adam Kopeć on 11/11/17.
//  Copyright © 2015 Adam Kopeć. All rights reserved.
//

#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <mach/vm_types.h>

#ifdef __cplusplus
extern "C" {
#endif
             extern int  kvsnprintf(char *buf, size_t size, const char *fmt, va_list args);
             extern void panic(const char* fmt, ...);
             extern void itoa(char *buf, unsigned long int n, int base);
             extern bool use_screen_caching;
             extern bool early;
             extern bool experimental;
#ifdef __cplusplus
}
#endif

#define VM_MIN_KERNEL_ADDRESS             ((vm_offset_t) 0xFFFFFF8000000000UL)
#define VM_MIN_KERNEL_PAGE                ((ppnum_t)0)
#define VM_MIN_KERNEL_AND_KEXT_ADDRESS    (VM_MIN_KERNEL_ADDRESS - 0x80000000ULL)
#define VM_MAX_KERNEL_ADDRESS             ((vm_offset_t) 0xFFFFFFFFFFFFEFFFUL)

int  __doprnt(const char *fmt, va_list argp, void (*putc)(int), int radix, int is_log);

int
_doprnt_log(register const char    *fmt, va_list *argp, void (*putc)(int), int radix)/* default radix - for '%r' */ {
    return __doprnt(fmt, *argp, putc, radix, true);
}

int
_doprnt(register const char    *fmt, va_list *argp, void (*putc)(int), int radix)/* default radix - for '%r' */ {
    return __doprnt(fmt, *argp, putc, radix, false);
}

ssize_t
write(int fd, const void *buf, size_t nbyte) {
    //    printf("write(fd=%d, buf=%p nbyte=%lu\n", fd, buf, nbyte);
    
    if (fd == 1 || fd == 2) {
        printf("%s", (const char *)buf);
    } else {
        panic("write() with fd = %d\n", fd);
    }
    return nbyte;
}

int printf(const char *restrict s, ...) {
    va_list     listp;
    int         printedchs;
    va_start(listp, s);
    printedchs = _doprnt(s, &listp, (void(*)(int))putchar, 16);
    va_end(listp);
    return printedchs;
}

int vprintf(const char *restrict s, va_list list) {
    return __doprnt(s, list, (void(*)(int))putchar, 16, false);
}

#define isdigit(d) ((d) >= '0' && (d) <= '9')
#define Ctod(c) ((c) - '0')

#define MAXBUF (sizeof(long long int) * 8)    /* enough for binary */
static char digs[] = "0123456789abcdef";

static int
printnum(unsigned long long int    u, int base, void (*putc)(int)) {
    char       buf[MAXBUF];     /* build number here */
    char *p = &buf[MAXBUF-1];
    int nprinted = 0;
    
    do {
        *p-- = digs[u % base];
        u /= base;
    } while (u != 0);
    
    while (++p != &buf[MAXBUF]) {
        (*putc)(*p);
        nprinted++;
    }
    
    return nprinted;
}

bool    _doprnt_truncates = false;

#if (DEBUG)
bool    doprnt_hide_pointers = false;
#else
bool    doprnt_hide_pointers = true;
#endif

int
__doprnt(const char    *fmt, va_list argp, void (*putc)(int), int radix, int is_log) {
    int                 length;
    int                 prec;
    bool                ladjust;
    char                padc;
    long long           n;
    unsigned long long    u;
    int                 plus_sign;
    int                 sign_char;
    bool                altfmt, truncate;
    int                 base;
    char                c;
    int                 capitals;
    int                 long_long;
    int                 nprinted = 0;
    
    while ((c = *fmt) != '\0') {
        if ((c != '%') && (c < 0xC0)) {
            (*putc)(c);
            nprinted++;
            fmt++;
            continue;
        }
        
        if (c > 0xC0) {
            if (c > 0xC3) {
                continue;
            }
            char c_ = c;
            fmt++;
            c = *fmt;
            if (c_ == 0xC2) {
                (*putc)(c);
                nprinted++;
                fmt++;
                continue;
            } else if (c_ == 0xC3) {
                c |= 0xC0;
                (*putc)(c);
                nprinted++;
                fmt++;
                continue;
            }
        }
        
        fmt++;
        
        long_long = 0;
        length = 0;
        prec = -1;
        ladjust = false;
        padc = ' ';
        plus_sign = 0;
        sign_char = 0;
        altfmt = false;
        
        while (true) {
            c = *fmt;
            if (c == '#') {
                altfmt = true;
            }
            else if (c == '-') {
                ladjust = true;
            }
            else if (c == '+') {
                plus_sign = '+';
            }
            else if (c == ' ') {
                if (plus_sign == 0)
                    plus_sign = ' ';
            }
            else
                break;
            fmt++;
        }
        
        if (c == '0') {
            padc = '0';
            c = *++fmt;
        }
        
        if (isdigit(c)) {
            while(isdigit(c)) {
                length = 10 * length + Ctod(c);
                c = *++fmt;
            }
        }
        else if (c == '*') {
            length = va_arg(argp, int);
            c = *++fmt;
            if (length < 0) {
                ladjust = !ladjust;
                length = -length;
            }
        }
        
        if (c == '.') {
            c = *++fmt;
            if (isdigit(c)) {
                prec = 0;
                while(isdigit(c)) {
                    prec = 10 * prec + Ctod(c);
                    c = *++fmt;
                }
            }
            else if (c == '*') {
                prec = va_arg(argp, int);
                c = *++fmt;
            }
        }
        
        if (c == 'l') {
            c = *++fmt;    /* need it if sizeof(int) < sizeof(long) */
            if (sizeof(int)<sizeof(long))
                long_long = 1;
            if (c == 'l') {
                long_long = 1;
                c = *++fmt;
            }
        } else if (c == 'q' || c == 'L') {
            long_long = 1;
            c = *++fmt;
        }
        
        truncate = false;
        capitals=0;        /* Assume lower case printing */
        
        switch(c) {
            case 'b':
            case 'B':
            {
                register char *p;
                bool          any;
                register int  i;
                
                if (long_long) {
                    u = va_arg(argp, unsigned long long);
                } else {
                    u = va_arg(argp, unsigned int);
                }
                p = va_arg(argp, char *);
                base = *p++;
                nprinted += printnum(u, base, putc);
                
                if (u == 0)
                    break;
                
                any = false;
                while ((i = *p++) != '\0') {
                    if (*fmt == 'B')
                        i = 33 - i;
                    if (*p <= 32) {
                        /*
                         * Bit field
                         */
                        register int j;
                        if (any)
                            (*putc)(',');
                        else {
                            (*putc)('<');
                            any = true;
                        }
                        nprinted++;
                        j = *p++;
                        if (*fmt == 'B')
                            j = 32 - j;
                        for (; (c = *p) > 32; p++) {
                            (*putc)(c);
                            nprinted++;
                        }
                        nprinted += printnum((unsigned)( (u>>(j-1)) & ((2<<(i-j))-1)), base, putc);
                    }
                    else if (u & (1<<(i-1))) {
                        if (any)
                            (*putc)(',');
                        else {
                            (*putc)('<');
                            any = true;
                        }
                        nprinted++;
                        for (; (c = *p) > 32; p++) {
                            (*putc)(c);
                            nprinted++;
                        }
                    }
                    else {
                        for (; *p > 32; p++)
                            continue;
                    }
                }
                if (any) {
                    (*putc)('>');
                    nprinted++;
                }
                break;
            }
                
            case 'c':
                c = va_arg(argp, int);
                (*putc)(c);
                nprinted++;
                break;
                
            case 's':
            {
                register const char *p;
                register const char *p2;
                
                if (prec == -1)
                    prec = 0x7fffffff;    /* MAXINT */
                
                p = va_arg(argp, char *);
                
                if (p == NULL)
                    p = "";
                
                if (length > 0 && !ladjust) {
                    n = 0;
                    p2 = p;
                    
                    for (; *p != '\0' && n < prec; p++)
                        n++;
                    
                    p = p2;
                    
                    while (n < length) {
                        (*putc)(' ');
                        n++;
                        nprinted++;
                    }
                }
                
                n = 0;
                
                while ((n < prec) && (!(length > 0 && n >= length))) {
                    if (*p == '\0') {
                        break;
                    }
                    if (*p > 0xC0) {
                        if (*p == 0xC2) {
                            p++;
                        } else if (*p == 0xC3) {
                            p++;
                            char c__ = *p;
                            c__ |= 0xC0;
                            (*putc)(c__);
                            nprinted++;
                            n++;
                            continue;
                        }
                    }
                    (*putc)(*p++);
                    nprinted++;
                    n++;
                }
                
                if (n < length && ladjust) {
                    while (n < length) {
                        (*putc)(' ');
                        n++;
                        nprinted++;
                    }
                }
                
                break;
            }
                
            case 'o':
                truncate = _doprnt_truncates;
            case 'O':
                base = 8;
                goto print_unsigned;
                
            case 'D': {
                unsigned char *up;
                char *q, *p;
                
                up = (unsigned char *)va_arg(argp, unsigned char *);
                p  = (char *)va_arg(argp, char *);
                if (length == -1)
                    length = 16;
                while(length--) {
                    (*putc)(digs[(*up >> 4)]);
                    (*putc)(digs[(*up & 0x0f)]);
                    nprinted += 2;
                    up++;
                    if (length) {
                        for (q=p;*q;q++) {
                            (*putc)(*q);
                            nprinted++;
                        }
                    }
                }
                break;
            }
                
            case 'd':
                truncate = _doprnt_truncates;
                base = 10;
                goto print_signed;
                
            case 'u':
                truncate = _doprnt_truncates;
            case 'U':
                base = 10;
                goto print_unsigned;
                
            case 'p':
                altfmt = true;
                if (sizeof(int)<sizeof(void *)) {
                    long_long = 1;
                }
            case 'x':
                truncate = _doprnt_truncates;
                base     = 16;
                goto print_unsigned;
                
            case 'X':
                base     = 16;
                capitals = 16;    /* Print in upper case */
                goto print_unsigned;
                
            case 'z':
                truncate = _doprnt_truncates;
                base     = 16;
                goto print_signed;
                
            case 'Z':
                base     = 16;
                capitals = 16;    /* Print in upper case */
                goto print_signed;
                
            case 'r':
                truncate = _doprnt_truncates;
            case 'R':
                base = radix;
                goto print_signed;
                
            case 'n':
                truncate = _doprnt_truncates;
            case 'N':
                base = radix;
                goto print_unsigned;
                
            print_signed:
                if (long_long) {
                    n = va_arg(argp, long long);
                } else {
                    n = va_arg(argp, int);
                }
                if (n >= 0) {
                    u = n;
                    sign_char = plus_sign;
                }
                else {
                    u = -n;
                    sign_char = '-';
                }
                goto print_num;
                
            print_unsigned:
                if (long_long) {
                    u = va_arg(argp, unsigned long long);
                } else {
                    u = va_arg(argp, unsigned int);
                }
                goto print_num;
                
            print_num:
            {
                char              buf[MAXBUF];     /* build number here */
                register char *p        = &buf[MAXBUF-1];
                static   char digits[]  = "0123456789abcdef0123456789ABCDEF";
                const    char *prefix   = NULL;
                
                if (truncate) u = (long long)((int)(u));
                
                if (doprnt_hide_pointers && is_log) {
                    const char str[] = "<ptr>";
                    const char* strp = str;
                    int strl = sizeof(str) - 1;
                    
                    if (u >= VM_MIN_KERNEL_AND_KEXT_ADDRESS && u <= VM_MAX_KERNEL_ADDRESS) {
                        while(*strp != '\0') {
                            (*putc)(*strp);
                            strp++;
                        }
                        nprinted += strl;
                        break;
                    }
                }
                
                if (u != 0 && altfmt) {
                    if (base == 8)
                        prefix = "0";
                    else if (base == 16)
                        prefix = "0x";
                }
                
                do {
                    /* Print in the correct case */
                    *p-- = digits[(u % base)+capitals];
                    u /= base;
                } while (u != 0);
                
                length -= (int)(&buf[MAXBUF-1] - p);
                if (sign_char)
                    length--;
                if (prefix)
                    length -= (int)strlen(prefix);
                
                if (padc == ' ' && !ladjust) {
                    /* blank padding goes before prefix */
                    while (--length >= 0) {
                        (*putc)(' ');
                        nprinted++;
                    }
                }
                if (sign_char) {
                    (*putc)(sign_char);
                    nprinted++;
                }
                if (prefix) {
                    while (*prefix) {
                        (*putc)(*prefix++);
                        nprinted++;
                    }
                }
                if (padc == '0') {
                    /* zero padding goes after sign and prefix */
                    while (--length >= 0) {
                        (*putc)('0');
                        nprinted++;
                    }
                }
                while (++p != &buf[MAXBUF]) {
                    (*putc)(*p);
                    nprinted++;
                }
                
                if (ladjust) {
                    while (--length >= 0) {
                        (*putc)(' ');
                        nprinted++;
                    }
                }
                break;
            }
                
            case '\0':
                fmt--;
                break;
                
            default:
                (*putc)(c);
                nprinted++;
        }
        fmt++;
    }
    return nprinted;
}
