/* hppoa_mem.exe - read/write/scan the running hppoa.exe guest memory (RE helper).
 *   hppoa_mem r  <guest> <len>            hex dump
 *   hppoa_mem d  <guest> <len> <file>     dump to file
 *   hppoa_mem w  <guest> <hexbytes>       write bytes
 *   hppoa_mem f32 <guest> <len> <value> <tol>   find floats
 * Guest address -> host: + offset (HPPOA_MEM_OFFSET, default 0x10000). */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static HANDLE open_game(void)
{
    PROCESSENTRY32 pe = { sizeof pe };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    HANDLE h = NULL;
    const char *name = getenv("HPPOA_EXE") ? getenv("HPPOA_EXE") : "hppoa.exe";
    if (Process32First(snap, &pe)) do {
        if (!_stricmp(pe.szExeFile, name)) {
            h = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
                            PROCESS_QUERY_INFORMATION, FALSE, pe.th32ProcessID);
            break;
        }
    } while (Process32Next(snap, &pe));
    CloseHandle(snap);
    return h;
}

static unsigned long long off(void)
{
    const char *e = getenv("HPPOA_MEM_OFFSET");
    return e ? strtoull(e, NULL, 0) : 0x10000;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: see source\n"); return 2; }
    HANDLE h = open_game();
    if (!h) { fprintf(stderr, "hppoa.exe not found / no access (%lu)\n", GetLastError()); return 1; }
    unsigned guest = (unsigned)strtoul(argv[2], NULL, 0);
    void *host = (void *)(uintptr_t)(guest + off());
    if (!strcmp(argv[1], "r") || !strcmp(argv[1], "d") || !strcmp(argv[1], "f32")) {
        size_t len = strtoul(argv[3], NULL, 0);
        unsigned char *buf = calloc(1, len);
        SIZE_T got = 0;
        /* page by page so holes don't fail the whole read */
        for (size_t o = 0; o < len; o += 4096) {
            SIZE_T n = len - o < 4096 ? len - o : 4096, g = 0;
            if (ReadProcessMemory(h, (char *)host + o, buf + o, n, &g)) got += g;
        }
        if (!strcmp(argv[1], "r")) {
            for (size_t o = 0; o < len; o += 16) {
                printf("%08X:", (unsigned)(guest + o));
                for (size_t i = 0; i < 16 && o + i < len; i++) printf(" %02X", buf[o + i]);
                printf("  |");
                for (size_t i = 0; i < 16 && o + i < len; i += 4) {
                    float f; memcpy(&f, buf + o + i, 4);
                    printf(" %g", f);
                }
                printf("\n");
            }
        } else if (!strcmp(argv[1], "d")) {
            FILE *fp = fopen(argv[4], "wb");
            fwrite(buf, 1, len, fp);
            fclose(fp);
        } else {
            float v = strtof(argv[4], NULL), tol = strtof(argv[5], NULL);
            for (size_t o = 0; o + 4 <= len; o += 4) {
                float f; memcpy(&f, buf + o, 4);
                if (fabsf(f - v) <= tol) printf("%08X %g\n", (unsigned)(guest + o), f);
            }
        }
        fprintf(stderr, "read %zu/%zu\n", (size_t)got, len);
    } else if (!strcmp(argv[1], "harry")) {
        /* harry <seconds>: player [0x3565B0] -> object +0x28 -> translation +0x70, every 250 ms */
        int n = atoi(argv[2]) * 4;
        for (int i = 0; i < n; i++, Sleep(250)) {
            unsigned pl = 0, ob = 0; float t[3] = {0};
            ReadProcessMemory(h, (void *)(uintptr_t)(0x3565B0 + off()), &pl, 4, NULL);
            if (pl) ReadProcessMemory(h, (void *)(uintptr_t)(pl + 0x28 + off()), &ob, 4, NULL);
            if (ob) ReadProcessMemory(h, (void *)(uintptr_t)(ob + 0x70 + off()), t, 12, NULL);
            printf("%6.2f pl %08X obj %08X pos %9.2f %9.2f %9.2f\n", i / 4.0, pl, ob, t[0], t[1], t[2]);
            fflush(stdout);
        }
    } else if (!strcmp(argv[1], "w")) {
        const char *s = argv[3];
        unsigned char b[256]; size_t n = 0;
        while (s[0] && s[1] && n < sizeof b) { unsigned v; sscanf(s, "%2x", &v); b[n++] = (unsigned char)v; s += 2; }
        SIZE_T put = 0;
        BOOL ok = WriteProcessMemory(h, host, b, n, &put);
        fprintf(stderr, "write %s %zu\n", ok ? "ok" : "FAILED", (size_t)put);
    }
    return 0;
}
