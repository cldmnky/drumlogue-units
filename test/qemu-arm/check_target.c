#include <stdio.h>
#include <stdint.h>

int main() {
    // Test the different targets
    uint16_t pepege = 0x0405;  // synth
    uint16_t clouds = 0x0403;  // revfx
    uint16_t k_unit_module_synth = 0x01;
    
    printf("pepege-synth target: 0x%04x\n", pepege);
    printf("  Lower byte: 0x%02x\n", pepege & 0xFF);
    printf("  == synth (0x01)? %s\n\n", ((pepege & 0xFF) == k_unit_module_synth) ? "YES" : "NO");
    
    printf("clouds-revfx target: 0x%04x\n", clouds);
    printf("  Lower byte: 0x%02x\n", clouds & 0xFF);
    printf("  == synth (0x01)? %s\n\n", ((clouds & 0xFF) == k_unit_module_synth) ? "YES" : "NO");
    
    return 0;
}
