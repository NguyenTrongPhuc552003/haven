/**
 * haven_tool — userspace CLI for the Haven hypervisor guest interface.
 *
 * Communicates with the Haven Linux kernel module via /dev/haven.
 * When not running inside a Haven partition the device will be absent;
 * the tool reports this gracefully and exits with a non-zero status.
 *
 * Usage: haven_tool <command>
 *
 * Commands:
 *   version         Print the Haven hypervisor version number.
 *   partition-info  Print the current partition ID.
 *   test-isolation  Trigger the hypervisor's built-in isolation self-test.
 *   help            Show this usage message.
 *
 * @file drivers/guest-tools/haven_tool.c
 */

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "haven_ioctl.h"

#define HAVEN_DEV "/dev/haven"

/* -----------------------------------------------------------------------
 * usage
 * ----------------------------------------------------------------------- */
static void usage(const char *argv0)
{
    fprintf(stderr,
            "Usage: %s <command>\n"
            "\n"
            "Commands:\n"
            "  version         Print Haven hypervisor version.\n"
            "  partition-info  Print current partition ID.\n"
            "  test-isolation  Trigger isolation self-test.\n"
            "  help            Show this message.\n",
            argv0);
}

/* -----------------------------------------------------------------------
 * open_haven_dev — open /dev/haven, or report and return -1.
 * ----------------------------------------------------------------------- */
static int open_haven_dev(void)
{
    int fd = open(HAVEN_DEV, O_RDWR);
    if (fd < 0) {
        if (errno == ENOENT)
            fprintf(stderr,
                    "haven_tool: %s not found — "
                    "not running inside a Haven partition?\n",
                    HAVEN_DEV);
        else
            fprintf(stderr, "haven_tool: cannot open %s: %s\n",
                    HAVEN_DEV, strerror(errno));
    }
    return fd;
}

/* -----------------------------------------------------------------------
 * cmd_version
 * ----------------------------------------------------------------------- */
static int cmd_version(void)
{
    int fd = open_haven_dev();
    if (fd < 0)
        return 1;

    uint32_t ver = 0;
    if (ioctl(fd, HAVEN_IOCTL_GET_VERSION, &ver) < 0) {
        fprintf(stderr, "haven_tool: GET_VERSION ioctl failed: %s\n",
                strerror(errno));
        close(fd);
        return 1;
    }
    close(fd);

    printf("Haven hypervisor version: %u.%u.%u\n",
           (ver >> 16) & 0xFFU,
           (ver >>  8) & 0xFFU,
           (ver >>  0) & 0xFFU);
    return 0;
}

/* -----------------------------------------------------------------------
 * cmd_partition_info
 * ----------------------------------------------------------------------- */
static int cmd_partition_info(void)
{
    int fd = open_haven_dev();
    if (fd < 0)
        return 1;

    uint32_t partition_id = 0;
    if (ioctl(fd, HAVEN_IOCTL_GET_PARTITION, &partition_id) < 0) {
        fprintf(stderr, "haven_tool: GET_PARTITION ioctl failed: %s\n",
                strerror(errno));
        close(fd);
        return 1;
    }
    close(fd);

    printf("Current partition ID: %u\n", partition_id);
    return 0;
}

/* -----------------------------------------------------------------------
 * cmd_test_isolation
 * ----------------------------------------------------------------------- */
static int cmd_test_isolation(void)
{
    int fd = open_haven_dev();
    if (fd < 0)
        return 1;

    uint32_t result = 0;
    if (ioctl(fd, HAVEN_IOCTL_TRIGGER_TEST, &result) < 0) {
        fprintf(stderr, "haven_tool: TRIGGER_TEST ioctl failed: %s\n",
                strerror(errno));
        close(fd);
        return 1;
    }
    close(fd);

    if (result == 0) {
        printf("Isolation self-test: PASS\n");
        return 0;
    }
    fprintf(stderr, "Isolation self-test: FAIL (code %u)\n", result);
    return 1;
}

/* ===================================================================== */
/* main                                                                    */
/* ===================================================================== */
int main(int argc, char *argv[])
{
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "version") == 0)
        return cmd_version();

    if (strcmp(cmd, "partition-info") == 0)
        return cmd_partition_info();

    if (strcmp(cmd, "test-isolation") == 0)
        return cmd_test_isolation();

    if (strcmp(cmd, "help") == 0) {
        usage(argv[0]);
        return 0;
    }

    fprintf(stderr, "haven_tool: unknown command '%s'\n", cmd);
    usage(argv[0]);
    return 1;
}
