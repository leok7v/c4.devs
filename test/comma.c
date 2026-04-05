// Test comma operator

int test_return();

int main() {
    int result = 0;
    
    // Basic comma: (1, 2, 3) should be 3
    {
        int x = (1, 2, 3);
        if (x != 3) { result = 1; }
    }
    
    // Comma with assignments
    {
        int a = 0;
        int b = 0;
        (a = 5, b = 10);
        if (a != 5 || b != 10) { result = 2; }
    }
    
    // Comma in if condition
    {
        int x = 0;
        int y = 0;
        if ((x = 1, y = 2, 0)) {
            result = 3;  // shouldn't reach here
        }
        if (x != 1 || y != 2) { result = 4; }
    }
    
    // Comma in while condition
    {
        int i = 0;
        int sum = 0;
        while ((i < 3, sum += i, i++, i < 3)) {
            // loop 3 times
        }
        if (sum != 3) { result = 5; }  // 0 + 1 + 2 = 3
    }
    
    // Comma in return
    {
        if (test_return() != 3) { result = 6; }
    }
    
    // Nested commas
    {
        int x = ((1, 2), (3, 4));
        if (x != 4) { result = 7; }
    }
    
    return result;
}

int test_return() {
    return (1, 2, 3);
}
