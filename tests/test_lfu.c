#include "lfu_policy.h" //Provides the function prototypes for the LFU policy

#include <assert.h> //Provides the assert macro for runtime assertions
#include <stdio.h>  //Provides the printf function for outputting information

int main(void){
    /*Create space for three cached resources*/
    PolicyEntry entries[3];
    /*Insert A, B, and C at different request ticks
    Each entry begins with frequency 1*/
    lfu_init_entry(&entries[0], "A", 32 *1024,125,1);
    lfu_init_entry(&entries[1], "B", 42*1024, 90, 2);
    lfu_init_entry(&entries[2], "C", 14*1024, 80, 3);
    /*A receives a hit at tick 4
    A's frequency becomes 2, so LFU should protect it
    */
    lfu_record_hit(&entries[0], 4);
    /*Verify that the hit increases A's frequency*/
    assert(entries[0].frequency == 2);
/*B and C both have frequency 1
B was last accessed at tick 2
C was last accessed at tick 3
LFu uses the LRU tie-break and chooses B
*/
    size_t victim = lfu_choose_victim(entries, 3);
    assert(victim == 1);
    /*An empty array must return the special */
    assert(lfu_choose_victim(entries, 0) == SIZE_MAX);
    printf("Selected victim: %s\n", entries[victim].key);
    puts("All tests passed successfully.");
    return 0;
}
