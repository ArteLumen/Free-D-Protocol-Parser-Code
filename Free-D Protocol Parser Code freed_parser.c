#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 5555
#define FREED_PACKET_SIZE 29

typedef struct {
    uint8_t camera_id;
    float pan;
    float tilt;
    float roll;
    float x;
    float y;
    float z;
    float zoom;
    float focus;
} CameraTrackingData;

float decode_24bit_to_float(const uint8_t *buffer, float scale) {
    int32_t val = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
    
    if (val & 0x800000) {
        val |= 0xFF000000;
    }
    return ((float)val) / scale;
}

int parse_freed_packet(const uint8_t *packet, CameraTrackingData *data) {
    
  if (packet[0] != 0xD1) {
        return -1; 
  }

    data->camera_id = packet[1];

    data->pan = decode_24bit_to_float(&packet[2], 32768.0f);
    data->tilt = decode_24bit_to_float(&packet[5], 32768.0f);
    data->roll = decode_24bit_to_float(&packet[8], 32768.0f);

    data->x = decode_24bit_to_float(&packet[11], 64.0f);
    data->y = decode_24bit_to_float(&packet[14], 64.0f);
    data->z = decode_24bit_to_float(&packet[17], 64.0f);

    data->zoom = decode_24bit_to_float(&packet[20], 256.0f);
    data->focus = decode_24bit_to_float(&packet[23], 256.0f);

    return 0;
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    uint8_t buffer[FREED_PACKET_SIZE];
    socklen_t len;

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

    while (1) {
        len = sizeof(cliaddr);
        ssize_t n = recvfrom(sockfd, buffer, FREED_PACKET_SIZE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
        
        if (n == FREED_PACKET_SIZE) {
            CameraTrackingData cam_data;
            if (parse_freed_packet(buffer, &cam_data) == 0) {
                printf("[Camera ID: %d] Position(X: %.2f, Y: %.2f, Z: %.2f) | Rotation(Pan: %.2f, Tilt: %.2f, Roll: %.2f)\n",
                       cam_data.camera_id, 
                       cam_data.x, cam_data.y, cam_data.z, 
                       cam_data.pan, cam_data.tilt, cam_data.roll);
            } else {
                printf("Invalid Free-D sync byte received!\n");
            }
        }
    }

    close(sockfd);
    return 0;
}
