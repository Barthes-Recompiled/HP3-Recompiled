/**
 * XAPI start-up checks that only make sense on a console with the real disc.
 *
 * sub_00155251, XAPI 5659's disc authentication (from XapiInitProcess at
 * 0x155482): when the certificate asks for DVD-X2 media and carries the
 * newer security flag (cert +0x1D8 bit 0, which HP: Chamber of Secrets'
 * XDK 5028 certificate lacks), it reads the drive's authentication page
 * (MODE SENSE page 0x3E) and then the disc's Xbox security sector (SCSI READ
 * DVD STRUCTURE 0xAD, 0x664 bytes) and has sub_0015519A verify it. The
 * runtime's synthetic DVD has no security sector, so the check failed five
 * times and XAPI booted to the dashboard (XapiBootToDash 0x155135, then
 * XLaunchNewImage(NULL)) before the title's main ran. Here the disc is
 * authentic: STATUS_SUCCESS.
 *
 * RECOMP_HLE_XAPI=0 runs the XAPI body (sub_X_gen) again.
 * Built into the recompile with --exclude-manual naming this file.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* the register names (eax, esp, ...) the runtime macros expand to */
#define RECOMP_GENERATED_CODE
#include "recomp/gen/recomp_types.h"

static int hle_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RECOMP_HLE_XAPI");
        on = !(e && *e == '0');
    }
    return on;
}

/* NTSTATUS XapiAuthenticateDisc(void) (no stack arguments) */
/* PoA's XapiInitProcess (0x000117AB, XDK 5849) has no disc authentication: after the call whose result it stores
 * at 0x3CB6D4 (hp1's call to 0x00155251 follows the same store) it maps \Device\Harddisk0\partition1\
 * (sub_0001179A) and checks the cache and utility drives and the kernel version; none of hp1's auth chain
 * (0x0015519A, 0x00155251) has a PoA match. Nothing to bind; kept for the other HP titles. */
#if 0  /* PoA: not linked, see above */
void sub_00155251(void)
{
    if (!hle_on()) {
        sub_00155251_gen();
        return;
    }
    fprintf(stderr, "[XAPI] disc authentication: skipped, the disc counts as authentic\n");
    g_eax = 0;
    g_esp += 4;
}
#endif
