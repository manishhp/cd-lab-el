// Test 5: Loop-heavy program
void loop_func() {
    for (int i = 0; i < 10; i++) {
        // Do nothing
    }
}

int main() {
    for (int i = 0; i < 5; i++) {
        loop_func();
    }
    return 0;
}