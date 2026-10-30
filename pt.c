#include "os.h"

#define PAGE_SIZE_BITS 12


void page_table_update(uint64_t pt, uint64_t vpn, uint64_t ppn) {
    uint64_t index;
    uint64_t* pt_entry_virtual_address;
    int level = 0;
    uint64_t pt_entry = pt;

    for (level = 0; level < 4; level++) {
        index = (vpn >> ((4 - level) * 9)) & 0x1FF;
        pt_entry_virtual_address = (uint64_t*)phys_to_virt(pt_entry << PAGE_SIZE_BITS);
        if (!(pt_entry_virtual_address[index]&1)) {
            if (ppn == NO_MAPPING) {
                return; // No need to create a new page table entry if we are unmapping
            }
            pt_entry_virtual_address[index] = (alloc_page_frame() << PAGE_SIZE_BITS) | 1;
        }
        pt_entry = pt_entry_virtual_address[index] >> PAGE_SIZE_BITS;
    }

    index = vpn & 0x1FF;
    pt_entry_virtual_address = (uint64_t*)phys_to_virt(pt_entry << PAGE_SIZE_BITS);
    if (ppn == NO_MAPPING) {
        pt_entry_virtual_address[index] = 0;
    } else {
        pt_entry_virtual_address[index] = (ppn << PAGE_SIZE_BITS) | 1;
    }

}


uint64_t page_table_query(uint64_t pt, uint64_t vpn) {
    uint64_t index;
    uint64_t* pt_entry_virtual_address;
    int level;
    uint64_t pt_entry = pt;

    for (level = 0; level < 5; level++) {
        index = (vpn >> ((4 - level) * 9)) & 0x1FF;
        pt_entry_virtual_address = (uint64_t*)phys_to_virt(pt_entry << PAGE_SIZE_BITS);
        pt_entry = pt_entry_virtual_address[index];

        if (!(pt_entry&1)) {
            return NO_MAPPING; 
        }
        pt_entry = pt_entry >> PAGE_SIZE_BITS;

    }
    return pt_entry; 
}