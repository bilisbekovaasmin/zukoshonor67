/*test_custom.c
Tests the custom BFV policy*/

#include "custom_policy.h" //Provides the function prototypes for the custom policy

#include <assert.h> //Provides the assert macro for runtime assertions
#include <stdio.h>  //Provides the printf function for outputting information

#define KIB 1024U //Defines a kibibyte as 1024 bytes

int main(void)
{
    /*Create space for three cached resources*/
    PolicyEntry size_entries[2];
    /*Insert A, B, and C at different request ticks
    Each entry begins with frequency 1*/
    custom_init_entry(
        &size_entries[0],
         "large-cheap", 
         80 * KIB, 
         40, 
         "student1", 1
        );
    custom_init_entry(
        &size_entries[1], 
        "small-expensive", 
        8 * KIB, 
        400, 
        "student1", 
        1
    );
    double large_score = custom_calculate_score(&size_entries[0], 2);
    double small_score = custom_calculate_score(&size_entries[1], 2);

    /* The small, expensive resource should have a much higher retention score*/
    assert(small_score > large_score);

    /*The custom policy should evict the large, 
    inexpensive resource at index 0*/
    assert(custom_choose_victim(size_entries, 2, 2) == 0);

    PolicyEntry client_entries[2];
    
    /*Create 2 otherwise identical resources*/
    custom_init_entry(
        &client_entries[0], 
        "shared-resource", 
        16 * KIB, 
        100, 
        "student1", 
        3
    );

    custom_init_entry(
        &client_entries[1], 
        "single client-resource", 
        16 * KIB, 
        100, 
        "student1", 
        3
    );
    /*Both resources receive another hit, so their frequency and recency remain equal
    The first is used by a different client,
    while the second is reused by the same client 
    */
   custom_record_hit(
    &client_entries[0],
     "student2", 
     4
);

    custom_record_hit(
     &client_entries[1], 
     "student1", 
     4
);
    
     /*The shared resource should have a higher retention score*/
     double shared_score = 
        custom_calculate_score(&client_entries[0], 4);
     double single_score = 
     custom_calculate_score(&client_entries[1], 4);
     
     assert(shared_score > single_score);
    
     /*The custom policy should evict the single-client resource at index 1*/
    assert(custom_choose_victim(client_entries, 2, 4) == 1);
     assert(custom_choose_victim(client_entries, 0, 4) == SIZE_MAX);

     printf("Large cheap score: %.2f\n", large_score);
    printf("Small expensive score: %.2f\n", small_score);
    printf("Shared resource score: %.2f\n", shared_score);
    printf("Single client resource score: %.2f\n", single_score);

    puts("All tests passed successfully.");
     return 0;
    }
