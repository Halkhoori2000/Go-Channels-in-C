#include "channel.h"

// Helper functions
void notify_waiting_select_recv_sems(channel_t *channel)
{
    for (list_node_t *node = list_head(channel->select_recv_sems); node; node = list_next(node))
    {
        sem_post((sem_t *)list_data(node));
    }
}

void notify_waiting_select_send_sems(channel_t *channel)
{
    for (list_node_t *node = list_head(channel->select_send_sems); node; node = list_next(node))
    {
        sem_post((sem_t *)list_data(node));
    }
}

// Creates a new channel with the provided size and returns it to the caller
// A 0 size indicates an unbuffered channel, whereas a positive size indicates a buffered channel
channel_t *channel_create(size_t size)
{
    // Allocate an instance for the channel pointer
    channel_t *channel = malloc(sizeof(channel_t));

    // Failed to allocate the memory
    if (!channel)
    {
        return NULL;
    }

    // Initialize the members of channel
    channel->open = true;
    channel->buffer = buffer_create(size);
    pthread_mutex_init(&channel->mutex, NULL);
    pthread_cond_init(&channel->receive, NULL);
    pthread_cond_init(&channel->send, NULL);
    channel->select_recv_sems = list_create();
    channel->select_send_sems = list_create();

    return channel;
}

// Writes data to the given channel
// This is a blocking call i.e., the function only returns on a successful completion of send
// In case the channel is full, the function waits till the channel has space to write the new data
// Returns SUCCESS for successfully writing data to the channel,
// CLOSED_ERROR if the channel is closed, and
// GEN_ERROR on encountering any other generic error of any sort
enum channel_status channel_send(channel_t *channel, void *data)
{
    // Lock the mutex
    pthread_mutex_lock(&channel->mutex);

    // Block until there is a buffer is
    while (buffer_current_size(channel->buffer) == buffer_capacity(channel->buffer) && channel->open)
    {
        pthread_cond_wait(&channel->receive, &channel->mutex);
    }

    if (channel->open)
    {
        // Add to the buffer
        buffer_add(channel->buffer, data);

        // Notify waiting threads
        pthread_cond_signal(&channel->send);

        // Notify waiting selects
        notify_waiting_select_recv_sems(channel);

        // Unlock the mutex
        pthread_mutex_unlock(&channel->mutex);

        return SUCCESS;
    }
    else
    {
        // Unlock the mutex
        pthread_mutex_unlock(&channel->mutex);

        // Can't add data to a closed channel
        return CLOSED_ERROR;
    }
}

// Reads data from the given channel and stores it in the function's input parameter, data (Note that it is a double pointer)
// This is a blocking call i.e., the function only returns on a successful completion of receive
// In case the channel is empty, the function waits till the channel has some data to read
// Returns SUCCESS for successful retrieval of data,
// CLOSED_ERROR if the channel is closed, and
// GEN_ERROR on encountering any other generic error of any sort
enum channel_status channel_receive(channel_t *channel, void **data)
{
    // Lock the mutex
    pthread_mutex_lock(&channel->mutex);

    // Block if the channel is empty
    while (buffer_current_size(channel->buffer) == 0 && channel->open)
    {
        pthread_cond_wait(&channel->send, &channel->mutex);
    }

    if (channel->open)
    {
        // Remove the data
        buffer_remove(channel->buffer, data);

        // Notify waiting threads
        pthread_cond_signal(&channel->receive);

        // Notify waiting selects
        notify_waiting_select_send_sems(channel);

        // Unlock the mutex
        pthread_mutex_unlock(&channel->mutex);

        return SUCCESS;
    }
    else
    {
        // Unlock the mutex
        pthread_mutex_unlock(&channel->mutex);

        // Can't add data to a closed channel
        return CLOSED_ERROR;
    }
}

// Writes data to the given channel
// This is a non-blocking call i.e., the function simply returns if the channel is full
// Returns SUCCESS for successfully writing data to the channel,
// CHANNEL_FULL if the channel is full and the data was not added to the buffer,
// CLOSED_ERROR if the channel is closed, and
// GEN_ERROR on encountering any other generic error of any sort
enum channel_status channel_non_blocking_send(channel_t *channel, void *data)
{
    // Lock the mutex
    pthread_mutex_lock(&channel->mutex);

    // Check if channel is open
    if (channel->open)
    {
        // Check if channel is full
        if (buffer_current_size(channel->buffer) == buffer_capacity(channel->buffer))
        {
            // Unlock the mutex
            pthread_mutex_unlock(&channel->mutex);

            return CHANNEL_FULL;
        }
        else
        {
            // Add to the buffer
            buffer_add(channel->buffer, data);

            // Notify waiting threads
            pthread_cond_signal(&channel->send);

            // Notify waiting selects
            notify_waiting_select_recv_sems(channel);

            // Unlock the mutex
            pthread_mutex_unlock(&channel->mutex);

            return SUCCESS;
        }
    }
    else
    {
        // Unlock the mutex
        pthread_mutex_unlock(&channel->mutex);

        // Can't add data to a closed channel
        return CLOSED_ERROR;
    }
}

// Reads data from the given channel and stores it in the function's input parameter data (Note that it is a double pointer)
// This is a non-blocking call i.e., the function simply returns if the channel is empty
// Returns SUCCESS for successful retrieval of data,
// CHANNEL_EMPTY if the channel is empty and nothing was stored in data,
// CLOSED_ERROR if the channel is closed, and
// GEN_ERROR on encountering any other generic error of any sort
enum channel_status channel_non_blocking_receive(channel_t *channel, void **data)
{
    // Lock the mutex
    pthread_mutex_lock(&channel->mutex);

    if (channel->open)
    {
        if (buffer_current_size(channel->buffer) == 0)
        {
            // Unlock the mutex
            pthread_mutex_unlock(&channel->mutex);

            return CHANNEL_EMPTY;
        }
        else
        {
            // Remove the data
            buffer_remove(channel->buffer, data);

            // Notify waiting threads
            pthread_cond_signal(&channel->receive);

            // Notify waiting selects
            notify_waiting_select_send_sems(channel);

            // Unlock the mutex
            pthread_mutex_unlock(&channel->mutex);

            return SUCCESS;
        }
    }
    else
    {
        // Unlock the mutex
        pthread_mutex_unlock(&channel->mutex);

        // Can't add data to a closed channel
        return CLOSED_ERROR;
    }
}

// Closes the channel and informs all the blocking send/receive/select calls to return with CLOSED_ERROR
// Once the channel is closed, send/receive/select operations will cease to function and just return CLOSED_ERROR
// Returns SUCCESS if close is successful,
// CLOSED_ERROR if the channel is already closed, and
// GEN_ERROR in any other error case
enum channel_status channel_close(channel_t *channel)
{
    // Lock the mutex
    pthread_mutex_lock(&channel->mutex);

    // Check if open
    if (channel->open)
    {
        // Close the channel
        channel->open = false;

        // Notify all waiting threads on this channel
        pthread_cond_broadcast(&channel->receive);
        pthread_cond_broadcast(&channel->send);

        // Notify waiting selects
        notify_waiting_select_send_sems(channel);
        notify_waiting_select_recv_sems(channel);

        // Unlock the mutex
        pthread_mutex_unlock(&channel->mutex);

        return SUCCESS;
    }
    else
    {
        // Channel is already closed
        // Unlock the mutex
        pthread_mutex_unlock(&channel->mutex);

        return CLOSED_ERROR;
    }
}

// Frees all the memory allocated to the channel
// The caller is responsible for calling channel_close and waiting for all threads to finish their tasks before calling channel_destroy
// Returns SUCCESS if destroy is successful,
// DESTROY_ERROR if channel_destroy is called on an open channel, and
// GEN_ERROR in any other error case
enum channel_status channel_destroy(channel_t *channel)
{
    // Lock the mutex
    pthread_mutex_lock(&channel->mutex);

    // Check if channel is open
    if (channel->open)
    {
        // Unlock the mutex
        pthread_mutex_unlock(&channel->mutex);

        // Throw an error
        return DESTROY_ERROR;
    }
    else
    {
        // Release all mmemory used by the channel
        list_destroy(channel->select_recv_sems);
        list_destroy(channel->select_send_sems);
        pthread_cond_destroy(&channel->receive);
        pthread_cond_destroy(&channel->send);
        buffer_free(channel->buffer);

        // Unlock the mutex
        pthread_mutex_unlock(&channel->mutex);

        // Final clean-up
        pthread_mutex_destroy(&channel->mutex);
        free(channel);

        return SUCCESS;
    }
}

// Helper function to unregister sempahore
void unregister_semaphore(select_t *channel_list, size_t channel_count, sem_t *select)
{
    for (size_t i = 0; i < channel_count; i++)
    {
        // Lock the channel
        pthread_mutex_lock(&channel_list[i].channel->mutex);

        // Remove the semaphore from the channel's list
        list_remove(channel_list[i].channel->select_recv_sems, list_find(channel_list[i].channel->select_recv_sems, select));
        list_remove(channel_list[i].channel->select_send_sems, list_find(channel_list[i].channel->select_send_sems, select));

        // Unock the channel
        pthread_mutex_unlock(&channel_list[i].channel->mutex);
    }
}

// Takes an array of channels (channel_list) of type select_t and the array length (channel_count) as inputs
// This API iterates over the provided list and finds the set of possible channels which can be used to invoke the required operation (send or receive) specified in select_t
// If multiple options are available, it selects the first option and performs its corresponding action
// If no channel is available, the call is blocked and waits till it finds a channel which supports its required operation
// Once an operation has been successfully performed, select should set selected_index to the index of the channel that performed the operation and then return SUCCESS
// In the event that a channel is closed or encounters any error, the error should be propagated and returned through select
// Additionally, selected_index is set to the index of the channel that generated the error
enum channel_status channel_select(select_t *channel_list, size_t channel_count, size_t *selected_index)
{
    enum channel_status stat;
    sem_t select;

    // Create a semaphore for this select call
    sem_init(&select, false, 0);

    // Register semaphores to each channel
    for (size_t i = 0; i < channel_count; i++)
    {
        channel_t *channel = channel_list[i].channel;

        pthread_mutex_lock(&channel->mutex);

        // Add the pointer to the semaphore to the list of sems to monitor
        if (channel_list[i].dir == SEND)
        {
            list_insert(channel->select_send_sems, &select);
        }
        else
        {
            list_insert(channel->select_recv_sems, &select);
        }

        pthread_mutex_unlock(&channel->mutex);
    }

    // Sentinel loop
    for (;;)
    {
        // Loop through each channel
        for (size_t i = 0; i < channel_count; i++)
        {
            // Send a data
            if (channel_list[i].dir == SEND)
            {
                // Use non-blocking call
                stat = channel_non_blocking_send(channel_list[i].channel, channel_list[i].data);

                // Continue if the result is full
                if (stat == CHANNEL_FULL)
                {
                    continue;
                }
                // Propagate error/success
                else
                {
                    *selected_index = i;
                    unregister_semaphore(channel_list, channel_count, &select);
                    sem_destroy(&select);
                    return stat;
                }
            }
            // Receive a data
            else
            {
                // Use non-blocking call
                stat = channel_non_blocking_receive(channel_list[i].channel, &channel_list[i].data);

                // Continue if the result is empty
                if (stat == CHANNEL_EMPTY)
                {
                    continue;
                }
                // Propagate error/success
                else
                {
                    *selected_index = i;
                    unregister_semaphore(channel_list, channel_count, &select);
                    sem_destroy(&select);
                    return stat;
                }
            }
        }

        // Wait for the select semahpore
        sem_wait(&select);
    }
}
