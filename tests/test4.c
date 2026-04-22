// Test 4: Recursion
void recursive(int n) {
    if (n > 0) {
        recursive(n - 1);
    }
}

int main() {
    recursive(5);
    return 0;
}