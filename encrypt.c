#include <stdio.h>
#include <stdlib.h>

/**
 * encrypt.c
 * 
 * 纯 C 语言实现的资源区 Payload 滚动密钥加密工具 (与 loader.c 算法 100% 对齐)
 */

void EncryptPayload(unsigned char* buffer, unsigned int size, unsigned int key) {
    unsigned int ebx = key;
    for (unsigned int ecx = size; ecx > 0; ecx--) {
        unsigned char plain = *buffer;
        unsigned char cipher = plain ^ (unsigned char)(ebx & 0xFF);
        *buffer = cipher;
        buffer++;

        // 32 位循环右移 1 位
        ebx = ((ebx >> 1) | (ebx << 31)) & 0xFFFFFFFF;

        // 异或更新低位密钥 (使用密文字节 cipher)
        ebx = (ebx & ~0xFF) | (((ebx & 0xFF) ^ cipher) & 0xFF);

        // 密钥累加剩余计数
        ebx = (ebx + ecx) & 0xFFFFFFFF;
    }
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "用法: %s <输入DLL文件> <输出加密BIN文件>\n", argv[0]);
        return 1;
    }

    FILE* fin = fopen(argv[1], "rb");
    if (!fin) {
        perror("[-] 无法打开输入文件");
        return 1;
    }

    fseek(fin, 0, SEEK_END);
    long size = ftell(fin);
    fseek(fin, 0, SEEK_SET);

    if (size <= 0) {
        fprintf(stderr, "[-] 输入文件大小无效: %ld\n", size);
        fclose(fin);
        return 1;
    }

    unsigned char* buffer = (unsigned char*)malloc(size);
    if (!buffer) {
        fprintf(stderr, "[-] 内存分配失败: %ld 字节\n", size);
        fclose(fin);
        return 1;
    }

    if (fread(buffer, 1, size, fin) != (size_t)size) {
        fprintf(stderr, "[-] 读取输入文件失败\n");
        free(buffer);
        fclose(fin);
        return 1;
    }
    fclose(fin);

    // 执行 0xDEADBEEF 滚动密钥加密
    EncryptPayload(buffer, (unsigned int)size, 0xDEADBEEF);

    FILE* fout = fopen(argv[2], "wb");
    if (!fout) {
        perror("[-] 无法创建输出文件");
        free(buffer);
        return 1;
    }

    if (fwrite(buffer, 1, size, fout) != (size_t)size) {
        fprintf(stderr, "[-] 写入加密数据失败\n");
        free(buffer);
        fclose(fout);
        return 1;
    }

    fclose(fout);
    free(buffer);
    printf("[+] 成功加密 %ld 字节 -> %s\n", size, argv[2]);
    return 0;
}
