#include <3ds.h>

#include <stdio.h>

int main(int argc, char **argv) {
    gfxInitDefault();

    consoleInit(GFX_TOP, NULL);

    printf("--- APT::SetAppCpuTimeLimit ---\n\n");

    // Get initial percentage
    u32 percentage;
    APT_GetAppCpuTimeLimit(&percentage);

    printf("Initial percentage: %lu\n\n", percentage);

    // Try all percentages from 0-100%, print failed calls
    for (int i = 0; i <= 100; i++) {
        const Result res = APT_SetAppCpuTimeLimit(i);

        if (R_FAILED(res)) {
            APT_GetAppCpuTimeLimit(&percentage);

            printf("[%d:%lu:%lX]\n", i, percentage, res);
        }
    }

    // Send command with invalid fixed value
    u32 aptcmdbuf[16];
    aptcmdbuf[0] = 0x004F0080;
    aptcmdbuf[1] = 0;
    aptcmdbuf[2] = 20;

    aptSendCommand(aptcmdbuf);

    printf("\nWith fixed = 0: [%08lX:%08lX]\n", aptcmdbuf[0], aptcmdbuf[1]);

    while (aptMainLoop()) {
        hidScanInput();

        if ((hidKeysDown() & KEY_START) != 0) {
            break;
        }

        gfxFlushBuffers();
        gfxSwapBuffers();

        gspWaitForVBlank();
    }

    gfxExit();

    return 0;
}
