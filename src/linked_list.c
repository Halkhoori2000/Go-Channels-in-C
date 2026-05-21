#include <stdlib.h>
#include "linked_list.h"

// Creates and returns a new list
list_t *list_create()
{
    // Create a pointer to list and initialize to zero
    return calloc(1, sizeof(list_t));
}

// Destroys a list
void list_destroy(list_t *list)
{
    // Delete all nodes
    list_node_t *node;
    while (node = list_head(list), node)
    {
        list_remove(list, node);
    }

    // Delete list
    free(list);
}

// Returns head of the list
list_node_t *list_head(list_t *list)
{
    return list->head;
}

// Returns tail of the list
list_node_t *list_tail(list_t *list)
{
    return list->tail;
}

// Returns next element in the list
list_node_t *list_next(list_node_t *node)
{
    return node->next;
}

// Returns prev element in the list
list_node_t *list_prev(list_node_t *node)
{
    return node->prev;
}

// Returns end of the list marker
list_node_t *list_end(list_t *list)
{
    return list->tail;
}

// Returns data in the given list node
void *list_data(list_node_t *node)
{
    return node->data;
}

// Returns the number of elements in the list
size_t list_count(list_t *list)
{
    return list->count;
}

// Finds the first node in the list with the given data
// Returns NULL if data could not be found
list_node_t *list_find(list_t *list, void *data)
{
    for (list_node_t *node = list->head; node; node = list_next(node))
    {
        if (node->data == data)
        {
            return node;
        }
    }

    return NULL;
}

// Inserts a new node in the list with the given data
// Returns new node inserted
list_node_t *list_insert(list_t *list, void *data)
{
    // Insert the node at the end of the list
    list_node_t *node = calloc(1, sizeof(list_node_t));
    if (!node)
    {
        return NULL;
    }

    // Fill up node
    node->data = data;

    // Insert to the linked list
    if (list->count == 0)
    {
        list->head = node;
        list->tail = node;
    }
    else
    {
        node->prev = list->tail;
        list->tail->next = node;
        list->tail = node;
    }

    // Increment number of elements
    list->count++;

    // Return link
    return node;
}

// Removes a node from the list and frees the node resources
void list_remove(list_t *list, list_node_t *node)
{
    // Do nothing if node is NULL
    if (!node)
    {
        return;
    }

    // We assume that the node is in the list
    if (node->prev)
    {
        node->prev->next = node->next;
    }
    else 
    {
        // Update head
        list->head = node->next;
    }

    if (node->next)
    {
        node->next->prev = node->prev;
    }
    else 
    {
        // Update tail
        list->tail = node->prev;
    }

    // Update count
    list->count--;

    // Free memory of node
    free(node);
}
