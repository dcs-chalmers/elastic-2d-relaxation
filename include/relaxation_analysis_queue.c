#include <assert.h>

#include "relaxation_analysis_queue.h"

ptlock_t relaxation_list_lock;
linear_list_t linear_clone;
linear_list_t error_dists;

void add_linear(sval_t val, int end)
{
    // Could use ssmem here as well for consistency
    linear_node_t* count_node = calloc(1, sizeof(*count_node));
    count_node->val = val;

    linear_clone.size++;
    if (linear_clone.head == NULL)
    {
        linear_clone.head = count_node;
        linear_clone.tail = count_node;
    }
    else if (end == 1)
    {
        // Add at head
        count_node->next = linear_clone.head;
        linear_clone.head = count_node;
    }
    else if (end == 0)
    {
        // Add at tail
        linear_clone.tail->next = count_node;
        linear_clone.tail = count_node;
    }
    else
    {
        error("Choose a valid end to add linear node to\n");
    }

}


static void add_relaxed_dist(linear_node_t* dist_node)
{
    error_dists.size++;

    if (error_dists.head == NULL)
    {
        dist_node->next = NULL;
        error_dists.head = dist_node;
        error_dists.tail = dist_node;
    }
    else
    {
        dist_node->next = NULL;
        error_dists.tail->next = dist_node;
        error_dists.tail = dist_node;
    }
}


void remove_linear(sval_t val)
{
    assert(linear_clone.head != NULL);

    linear_clone.size--;

    if (linear_clone.head->val == val)
    {
        // Remove first node

        linear_node_t* found_node = linear_clone.head;

        if (linear_clone.head == linear_clone.tail)
        {
            // First and last are same
            assert(linear_clone.head->next == NULL);
            assert(linear_clone.size == 0);
            linear_clone.tail = NULL;

        }
        linear_clone.head = linear_clone.head->next;

        found_node->val = 0;
        add_relaxed_dist(found_node);

    }
    else {
        // Search list for node

        uint64_t skipped = 1;
        linear_node_t* last_node = linear_clone.head;

        while(last_node->next->val != val)
        {
            skipped++;
            last_node = last_node->next;
            assert(last_node->next != NULL);
        }

        linear_node_t* found_node = last_node->next;
        last_node->next = found_node->next;
        if (found_node == linear_clone.tail)
        {
            // Remove last node
            linear_clone.tail = last_node;
        }

        found_node->val = skipped;
        add_relaxed_dist(found_node);

    }

}


void lock_relaxation_lists()
{
    LOCK(&relaxation_list_lock);
}


void unlock_relaxation_lists()
{
    UNLOCK(&relaxation_list_lock);
}

uint64_t gen_relaxation_count()
{
    // Returns the new count to use. Must hold the lock

    static uint64_t count;

    count++;

    return count;

}

void init_relaxation_analysis()
{
    INIT_LOCK(&relaxation_list_lock);
}

void print_relaxation_measurements()
{

    // Long double used to not run into a max of ints. Maybe gives even worse results?
    sval_t max;
    long double old, sum, mean, variance;

    lock_relaxation_lists();
    assert(error_dists.head != NULL);

    linear_node_t* node = error_dists.head;

    // Find mean and maximum
    sum = 0;
    max = 0;
    while (node != NULL)
    {
        sval_t val = node->val;
        sum += val;

        if (val > max) max = val;

        node = node->next;

        //assert(sum >= old);
        old = sum;
    }

    mean = sum / error_dists.size;
    printf("mean_relaxation , %.4Lf\n", mean);
    printf("max_relaxation , %zu\n", max);

    // Find variance
    node = error_dists.head;
    sum = 0;

    while (node != NULL)
    {
        long double val = (long double) node->val;
        sum += (val-mean) * (val-mean);

        node = node->next;

        //assert(sum >= old);
        old = sum;
    }

    variance = sum / error_dists.size;
    printf("variance_relaxation , %.4Lf\n", variance);

#ifdef SAVE_FULL
    // Print all individually
    node = error_dists.head;
    printf("relaxation_distances , ");
    while (node != NULL)
    {
        sval_t val = node->val;

        printf("%zu", val);
        if (node->next != NULL)
            printf(", ");
        else
            printf("\n");

        node = node->next;
    }
#endif

    unlock_relaxation_lists();
}
