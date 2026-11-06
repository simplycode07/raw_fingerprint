#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#define ELAN_VID 0x04f3
#define ELAN_PID 0x0903

#pragma pack(push, 1)
typedef struct {
    unsigned short bfType;
    unsigned int   bfSize;
    unsigned short bfReserved1;
    unsigned short bfReserved2;
    unsigned int   bfOffBits;
} BITMAPFILEHEADER;

typedef struct {
    unsigned int   biSize;
    int            biWidth;
    int            biHeight;
    unsigned short biPlanes;
    unsigned short biBitCount;
    unsigned int   biCompression;
    unsigned int   biSizeImage;
    int            biXPelsPerMeter;
    int            biYPelsPerMeter;
    unsigned int   biClrUsed;
    unsigned int   biClrImportant;
} BITMAPINFOHEADER;
#pragma pack(pop)

void save_bmp(const char *filename, unsigned char *data, int width, int height) {
    FILE *f = fopen(filename, "wb");
    int row_size = (width + 3) & ~3;
    int image_size = row_size * height;
    BITMAPFILEHEADER bfh = {0x4D42, sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+256*4+image_size,0,0,sizeof(BITMAPFILEHEADER)+sizeof(BITMAPINFOHEADER)+256*4};
    BITMAPINFOHEADER bih = {sizeof(BITMAPINFOHEADER), width, -height, 1, 8, 0, image_size, 2834, 2834, 256, 256};
    fwrite(&bfh, sizeof(bfh), 1, f);
    fwrite(&bih, sizeof(bih), 1, f);
    for (int i = 0; i < 256; i++) {
        unsigned char pal[4] = {i, i, i, 0};
        fwrite(pal, 4, 1, f);
    }
    for (int y = 0; y < height; y++) {
        fwrite(data + y * width, width, 1, f);
        for (int p = width; p < row_size; p++) fputc(0, f);
    }
    fclose(f);
    printf("Saved %s\n", filename);
}

int main() {
    libusb_device_handle *dev = NULL;
    libusb_context *ctx = NULL;
    libusb_init(&ctx);
    dev = libusb_open_device_with_vid_pid(ctx, ELAN_VID, ELAN_PID);
    if (!dev) { printf("no dev\n"); return 1; }
    
    if (libusb_kernel_driver_active(dev, 0) == 1) libusb_detach_kernel_driver(dev, 0);
    libusb_claim_interface(dev, 0);
    
    unsigned char resp[512];
    int transferred;
    unsigned char cmd[2];
    
    // dims
    cmd[0]=0x00; cmd[1]=0x0c;
    libusb_bulk_transfer(dev, 0x01, cmd, 2, &transferred, 10000);
    libusb_bulk_transfer(dev, 0x83, resp, 4, &transferred, 10000);
    int width = resp[2], raw_height = resp[0];
    printf("w=%d h=%d\n", width, raw_height);
    
    // led on
    cmd[0]=0x40; cmd[1]=0x31;
    libusb_bulk_transfer(dev, 0x01, cmd, 2, &transferred, 10000);
    usleep(100000);
    
    printf("swipe...\n");
    // try different sequences
    // sequence 1: pre_scan on ep83
    cmd[0]=0x40; cmd[1]=0x3f;
    libusb_bulk_transfer(dev, 0x01, cmd, 2, &transferred, 10000);
    transferred = 0;
    int r = libusb_bulk_transfer(dev, 0x83, resp, 1, &transferred, 60000);
    printf("s1 pre: r=%d t=%d val=%02x\n", r, transferred, resp[0]);
    
    if (resp[0] == 0x55) {
        // try get image
        cmd[0]=0x00; cmd[1]=0x09;
        libusb_bulk_transfer(dev, 0x01, cmd, 2, &transferred, 10000);
        int img_size = raw_height * width * 2;
        unsigned short *raw = malloc(img_size + 1024);
        memset(raw, 0, img_size + 1024);
        // read from ep 0x82
        transferred = 0;
        r = libusb_bulk_transfer(dev, 0x82, (unsigned char*)raw, img_size + 512, &transferred, 10000);
        printf("s1 img ep82: r=%d t=%d\n", r, transferred);
        if (transferred > width*raw_height) {
            // normalize
            unsigned char *img = malloc(width * raw_height);
            unsigned short min = 0xffff, max = 0;
            for (int i = 0; i < width*raw_height; i++) {
                if (raw[i] < min) min = raw[i];
                if (raw[i] > max) max = raw[i];
            }
            printf("range %d-%d\n", min, max);
            for (int i = 0; i < width*raw_height; i++) {
                if (max==min) img[i]=0;
                else img[i] = (unsigned char)((raw[i]-min)*255/(max-min));
            }
            save_bmp("raw_s1.bmp", img, width, raw_height);
            free(img);
        }
        free(raw);
    }
    
    // stop
    cmd[0]=0x00; cmd[1]=0x0b;
    libusb_bulk_transfer(dev, 0x01, cmd, 2, &transferred, 10000);
    
    libusb_release_interface(dev, 0);
    libusb_attach_kernel_driver(dev, 0);
    libusb_close(dev);
    libusb_exit(ctx);
    return 0;
}
