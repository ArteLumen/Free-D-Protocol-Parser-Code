#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define PORT 5555
#define FREED_PACKET_SIZE 29
#define FREED_MSG_TYPE_D1 0xD1
#define RECV_BUFFER_SIZE 64   // Larger than one packet, so oversized datagrams are detected

typedef struct {
    uint8_t camera_id;
    float pan;       // degrees
    float tilt;      // degrees
    float roll;      // degrees
    float x;         // millimeters
    float y;         // millimeters
    float z;         // millimeters
    uint32_t zoom;   // raw lens encoder counts (unsigned)
    uint32_t focus;  // raw lens encoder counts (unsigned)
} CameraTrackingData;

typedef enum {
    PARSE_OK = 0,
    PARSE_BAD_TYPE,
    PARSE_BAD_CHECKSUM
} ParseResult;

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

// Reads a 24-bit big-endian unsigned value
static uint32_t read_u24(const uint8_t *b) {
    return ((uint32_t)b[0] << 16) | ((uint32_t)b[1] << 8) | (uint32_t)b[2];
}

// Reads a 24-bit big-endian two's complement value
static int32_t read_s24(const uint8_t *b) {
    int32_t val = (int32_t)read_u24(b);
    if (val & 0x800000) {
        val -= 0x1000000;   // sign extension
    }
    return val;
}

static float decode_24bit_to_float(const uint8_t *b, float scale) {
    return (float)read_s24(b) / scale;
}

// Free-D checksum: 0x40 minus the sum of all bytes before the checksum byte (mod 256)
static uint8_t freed_checksum(const uint8_t *packet) {
    unsigned sum = 0;
    for (int i = 0; i < FREED_PACKET_SIZE - 1; i++) {
        sum += packet[i];
    }
    return (uint8_t)((0x40u - sum) & 0xFFu);
}

static ParseResult parse_freed_packet(const uint8_t *packet, CameraTrackingData *data) {
    if (packet[0] != FREED_MSG_TYPE_D1) {
        return PARSE_BAD_TYPE;
    }

    if (packet[FREED_PACKET_SIZE - 1] != freed_checksum(packet)) {
        return PARSE_BAD_CHECKSUM;
    }

    data->camera_id = packet[1];

    data->pan  = decode_24bit_to_float(&packet[2], 32768.0f);
    data->tilt = decode_24bit_to_float(&packet[5], 32768.0f);
    data->roll = decode_24bit_to_float(&packet[8], 32768.0f);

    data->x = decode_24bit_to_float(&packet[11], 64.0f);
    data->y = decode_24bit_to_float(&packet[14], 64.0f);
    data->z = decode_24bit_to_float(&packet[17], 64.0f);

    data->zoom  = read_u24(&packet[20]);
    data->focus = read_u24(&packet[23]);

    return PARSE_OK;
}

int main(void) {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    uint8_t buffer[RECV_BUFFER_SIZE];
    socklen_t len;

    // Stop cleanly on Ctrl+C / kill (no SA_RESTART, so recvfrom wakes up)
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Virtual Cinematography Free-D Server Listening on Port %d...\n", PORT);

    while (g_running) {
        len = sizeof(cliaddr);
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                             (struct sockaddr *)&cliaddr, &len);

        if (n < 0) {
            if (errno == EINTR) {
                continue;   // Interrupted by a signal; the loop checks g_running
            }
            perror("recvfrom failed");
            break;
        }

        char sender[INET_ADDRSTRLEN] = "unknown";
        inet_ntop(AF_INET, &cliaddr.sin_addr, sender, sizeof(sender));

        if (n != FREED_PACKET_SIZE) {
            printf("Ignored packet from %s: expected %d bytes, got %zd\n",
                   sender, FREED_PACKET_SIZE, n);
            continue;
        }

        CameraTrackingData cam_data;
        ParseResult result = parse_freed_packet(buffer, &cam_data);

        if (result == PARSE_BAD_TYPE) {
            printf("Ignored packet from %s: unexpected message type 0x%02X (expected 0x%02X)\n",
                   sender, buffer[0], FREED_MSG_TYPE_D1);
        } else if (result == PARSE_BAD_CHECKSUM) {
            printf("Ignored packet from %s: checksum mismatch\n", sender);
        } else {
            printf("[Camera ID: %d] Position(mm) X: %.2f, Y: %.2f, Z: %.2f | "
                   "Rotation(deg) Pan: %.2f, Tilt: %.2f, Roll: %.2f | "
                   "Zoom: %u, Focus: %u (raw)\n",
                   cam_data.camera_id,
                   cam_data.x, cam_data.y, cam_data.z,
                   cam_data.pan, cam_data.tilt, cam_data.roll,
                   cam_data.zoom, cam_data.focus);
        }
    }

    printf("Shutting down.\n");
    close(sockfd);
    return 0;
}
