#include "ODRTest.h"

int main() {
    // Run the complete ODR compliance test
    bool result = ODRTest::runCompleteTest();
    
    return result ? 0 : 1;
}
