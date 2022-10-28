
#include "kernel_list.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/sysmacros.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <stdbool.h>
#include "jpeglib.h"
#include <error.h>
#include <dirent.h>
#include <strings.h>
#include "album.h"

struct node
{
    struct rgbInfo *imageinfo;
    struct list_head list;

};

struct lcdinfo
{
    char *fbmem;
    int width;
    int height;
    int bpp;
};

typedef char (*P)[100];
#define left 0
#define right 1

int lcd;
int num_of_filename;
int e_x, e_y, s_x, s_y;
int num_of_visaul = 0;

struct fb_var_screeninfo varInfo;

int event();                                                    /* 获取左右滑动的信息 */

P dir(const char *filename);                                    /* 将正确格式的文件名称进行存储 */
// char (*    dir(const char *filename)   )[100];                                 /* 将正确格式的文件名称进行存储 */

struct lcdinfo *initLCD();                                      /*  初始化lcd ，获取lcd的信息 */

struct node *allNodeInfo();                                     /*  将所有的rgb数据存到大结构体  */

struct node *__new_code(struct rgbInfo *k);                     /* 对图片的rgb信息进行存储，存储到包含内核链表的节点中 */

void displayPIC(struct rgbInfo *p, struct lcdinfo *linfo);      /* 展示图片 */

int main(int argc, char const *argv[])
{

    if (argc == 1)
    {
        printf("Please select the correct photo album!\n");
        return 0;
    }

    P filenames = dir(argv[1]);

    chdir(argv[1]);
    if (!filenames)
    {
        return 0;
    }

    struct lcdinfo *linfo = initLCD();
    struct node *head = allNodeInfo(filenames, linfo);


    bzero(linfo->fbmem, linfo->height * linfo->width * linfo->bpp / 8 * 2);

    struct node *tmp = list_entry(&head->list, struct node, list);
    
    printf("%d x %d\n", tmp->imageinfo->width, tmp->imageinfo->height);

    while (1)
    {
        displayPIC(tmp->imageinfo, linfo);
        int i = event();
        if (i == 0)
        {
            tmp = list_entry(tmp->list.next, struct node, list);
            printf("%d x %d\n", tmp->imageinfo->width, tmp->imageinfo->height);
        }
        else if (i == 1)
        {
            tmp = list_entry(tmp->list.prev, struct node, list);
            printf("%d x %d\n", tmp->imageinfo->width, tmp->imageinfo->height);
        }
    }
    return 0;
}

int event()
{
    int tp = open("/dev/input/event0", O_RDWR);

    struct input_event ts;
    int x, y;
    while (1)
    {
        read(tp, &ts, sizeof(ts));

        if (ts.type == EV_ABS)
        {
            if (ts.code == ABS_X)
            {
                x = ts.value * 800 / 1024;
            }
            if (ts.code == ABS_Y)
            {
                y = ts.value * 800 / 1024;
            }
        }

        else if (ts.type == EV_KEY && ts.code == BTN_TOUCH && ts.value == 1)
        {
            e_x = x;
            e_y = y;
        }
        else if (ts.type == EV_KEY && ts.code == BTN_TOUCH && ts.value == 0)
        {
            s_x = x;
            s_y = y;
            break;
        }
    }
    int differentx = e_x - s_x; //横向滑动的差值
    int differenty = e_y - s_y;

    if (abs(differentx) > abs(differenty))
    {
        if (differentx > 0) //从右往左
        {
            return left;
        }
    }

    if (abs(differentx) > abs(differenty))
    {
        if (differentx < 0) //从右往左
        {
            return right;
        }
    }
}

P dir(const char *filename)
{
    char buf[5];
    P name = (P)calloc(20, 100);
    DIR *dp = opendir(filename);
    if (dp == NULL)
    {
        printf("Please select the correct photo album!\n");
        return NULL;
    }

    struct dirent *ep = (struct dirent *)calloc(1, sizeof(struct dirent));

    chdir(filename);
    char pwd[100];
    getcwd(pwd, sizeof(pwd));

    while ((ep = readdir(dp)) != NULL)
    {
        if (strlen(ep->d_name) <= 4)
            continue;
        bzero(buf, 5);
        memcpy(buf, &(ep->d_name[strlen(ep->d_name) - 4]), 4);
        buf[4] = '\0';
        if (strcmp(buf, ".jpg") == 0)
        {
            strcpy(name[num_of_filename], pwd);
            strcat(name[num_of_filename], "/");
            strcat(name[num_of_filename], ep->d_name);
            num_of_filename++;
        }
        else if (strcmp(buf, ".bmp") == 0)
        {
            strcpy(name[num_of_filename], pwd);
            strcat(name[num_of_filename], "/");
            strcat(name[num_of_filename], ep->d_name);
            num_of_filename++;
        }
    }
    if (num_of_filename == 0)
    {
        printf("have no picture!\n");
        free(name);
        return NULL;
    }

    return name;
}

struct lcdinfo *initLCD()
{
    // 1,打开屏幕设备文件
    lcd = open("/dev/fb0", O_RDWR);
    if (lcd == -1)
    {
        perror("Failed to open the device!");
        exit(0);
    }

    // 2,获取屏幕设备信息
    struct fb_fix_screeninfo fixInfo;

    bzero(&varInfo, sizeof(varInfo));
    bzero(&fixInfo, sizeof(fixInfo));

    ioctl(lcd, FBIOGET_FSCREENINFO, &fixInfo);
    ioctl(lcd, FBIOGET_VSCREENINFO, &varInfo);

    static struct lcdinfo linfo;
    linfo.width = varInfo.xres;
    linfo.height = varInfo.yres;
    linfo.bpp = varInfo.bits_per_pixel;

    // 3，准备好映射内存（显存）
    linfo.fbmem = mmap(NULL, linfo.width * linfo.height * linfo.bpp / 8 * 2,
                       PROT_WRITE | PROT_READ, MAP_SHARED, lcd, 0);
    if (linfo.fbmem == MAP_FAILED)
    {
        perror("Failed to mapping!");
        exit(0);
    }

    varInfo.xoffset = 0;
    varInfo.yoffset = 480;
    ioctl(lcd, FBIOPAN_DISPLAY, &varInfo);

    return &linfo;
}

void displayPIC(struct rgbInfo *p, struct lcdinfo *linfo)
{
    int bpp = linfo->bpp / 8;
    int x, y;
    char *p1 = linfo->fbmem;
    int scree_size = linfo->width * linfo->height * linfo->bpp / 8;

    if (p->sign == 1)
    {

        bzero(linfo->fbmem + 480 * (num_of_visaul % 2) * linfo->width * linfo->bpp / 8, linfo->width * linfo->height * linfo->bpp / 8);
        bool tooLarge = false;

        if (p->width > 2 * linfo->width || p->height > 2 * linfo->height)
        {
            printf("图片大于屏幕两倍,暂不支持,再见!\n");
            exit(0);
        }
        else if (p->width > linfo->width || p->height > linfo->height)
        {
            tooLarge = true;
            x = (linfo->width - p->width / 2) / 2;
            y = (linfo->height - p->height / 2) / 2;
        }
        else
        {
            x = (linfo->width - p->width) / 2;
            y = (linfo->height - p->height) / 2;
        }

        p1 += (y * linfo->width + x) * bpp;

        if (tooLarge)
        {
            for (int i = 0, k1 = 0; i < linfo->height - y && k1 < p->height; i++, k1 += 2)
            {
                int lcdoffset = linfo->width * bpp * i;
                int rgboffset = p->width * 3 * k1;

                for (int j = 0, k2 = 0; j < linfo->width - x && k2 < p->width; j++, k2 += 2)
                {
                    memcpy(p1 + scree_size * (num_of_visaul % 2) + lcdoffset + j * bpp + 2, p->rgbdata + rgboffset + k2 * 3 + 0, 1);
                    memcpy(p1 + scree_size * (num_of_visaul % 2) + lcdoffset + j * bpp + 1, p->rgbdata + rgboffset + k2 * 3 + 1, 1);
                    memcpy(p1 + scree_size * (num_of_visaul % 2) + lcdoffset + j * bpp + 0, p->rgbdata + rgboffset + k2 * 3 + 2, 1);
                }
            }
        }
        else
        {
            for (int i = 0; i < linfo->height && i < p->height; i++)
            {
                int lcdoffset = linfo->width * bpp * i;
                int rgboffset = p->width * 3 * i;
                for (int j = 0, k = 0; j < linfo->width && j < p->width; j++, k++)
                {
                    memcpy(p1 + scree_size * (num_of_visaul % 2) + lcdoffset + j * bpp + 2, p->rgbdata + rgboffset + k * 3 + 0, 1);
                    memcpy(p1 + scree_size * (num_of_visaul % 2) + lcdoffset + j * bpp + 1, p->rgbdata + rgboffset + k * 3 + 1, 1);
                    memcpy(p1 + scree_size * (num_of_visaul % 2) + lcdoffset + j * bpp + 0, p->rgbdata + rgboffset + k * 3 + 2, 1);
                }
            }
        }
    }

    else if (p->sign == 0)
    {

        bzero(linfo->fbmem + 480 * (num_of_visaul % 2) * linfo->width * linfo->bpp / 8, linfo->width * linfo->height * linfo->bpp / 8);
        bool tooLarge = false;

        x = abs(linfo->width - p->width) / 2;
        y = abs(linfo->height - p->height) / 2;

        int offset = (y * 800 + x) * 4;
        int pad = (4 - (p->width * p->bpp / 8) % 4) % 4;

        // p指向RGB的最末一行的行首
        char *p1 = p->rgbdata + (p->width * p->bpp / 8 + pad) * (p->height - 1);

        for (int i = 0; i < p->height && i < linfo->height - y; i++)
        {
            for (int j = 0; j < p->width && j < linfo->width - x; j++)
            {
                memcpy(linfo->fbmem + scree_size * (num_of_visaul % 2) + offset + 4 * j + 800 * 4 * i, p1 + 3 * j, 3);
            }

            p1 -= (p->width * p->bpp / 8 + pad);
        }
    }

    varInfo.xoffset = 0;
    varInfo.yoffset = 480 * (num_of_visaul % 2);
    ioctl(lcd, FBIOPAN_DISPLAY, &varInfo);
    num_of_visaul++;
}

struct node *__new_code(struct rgbInfo *k)
{
    struct node *new = calloc(1, sizeof(struct node));
    if (new == NULL)
    {
        printf("fail to initail \n");
        return NULL;
    }

    INIT_LIST_HEAD(&new->list);
    new->imageinfo = k;

    return new;
}

struct node *allNodeInfo(P filenames, struct lcdinfo *linfo)
{
    bool initNode = false;
    struct node *head = NULL;
    struct rgbInfo *k = NULL;
    char buf[5];

    for (int j = 0; j < num_of_filename; j++)
    {
        if (!initNode)
        {
            bzero(buf, 5);
            memcpy(buf, &(filenames[j][strlen(filenames[j]) - 4]), 4);
            buf[4] = '\0';
            if (strcmp(buf, ".jpg") == 0)
            {
                printf("%s\n", filenames[j]);
                k = jpg2rgb(filenames[j]);

                head = __new_code(k);
                initNode = true;
            }
            else if (strcmp(buf, ".bmp") == 0)
            {
                printf("%s\n", filenames[j]);
                k = loadBmp(filenames[j]);
                if (NULL != k)
                {
                    head = __new_code(k);
                    initNode = true;
                }
            }
        }
        else
        {
            bzero(buf, 5);
            memcpy(buf, &(filenames[j][strlen(filenames[j]) - 4]), 4);
            buf[4] = '\0';
            if (strcmp(buf, ".jpg") == 0)
            {
                printf("%s\n", filenames[j]);
                k = jpg2rgb(filenames[j]);
                struct node *new = __new_code(k);
                list_add_tail(&new->list, &head->list);
            }
            else if (strcmp(buf, ".bmp") == 0)
            {
                printf("%s\n", filenames[j]);
                k = loadBmp(filenames[j]);
                if (NULL != k)
                {
                    struct node *new = __new_code(k);
                    list_add_tail(&new->list, &head->list);
                }
            }
        }
    }
    return head;
}