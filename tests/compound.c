// Test compound assignment operators: +=, -=, *=, /=, %=, &=, |=, ^=, <<=, >>=

int main() {
    int result = 0;
    {
        int x = 5;
        x += 3;
        if (x != 8) { result = 1; }
    }
    {
        int x = 10;
        x -= 4;
        if (x != 6) { result = 2; }
    }
    {
        int x = 3;
        x *= 7;
        if (x != 21) { result = 3; }
    }
    {
        int x = 20;
        x /= 4;
        if (x != 5) { result = 4; }
    }
    {
        int x = 17;
        x %= 5;
        if (x != 2) { result = 5; }
    }
    {
        int x = 240;  // 0b11110000
        x &= 255;    // 0b11111111
        if (x != 240) { result = 6; }
    }
    {
        int x = 240;  // 0b11110000
        x |= 15;     // 0b00001111
        if (x != 255) { result = 7; }
    }
    {
        int x = 240;  // 0b11110000
        x ^= 15;     // 0b00001111
        if (x != 255) { result = 8; }
    }
    {
        int x = 1;
        x <<= 3;
        if (x != 8) { result = 9; }
    }
    {
        int x = 16;
        x >>= 2;
        if (x != 4) { result = 10; }
    }
    {
        int x = 1;
        x += 1;  // 2
        x *= 2;  // 4
        x -= 1;  // 3
        if (x != 3) { result = 11; }
    }
    return result;
}
