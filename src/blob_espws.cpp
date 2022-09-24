#include "blob.hpp"
#include <Arduino.h>
extern "C"
{
    #include "blob_core.h"
    
    #include "esp_websocket_client.h"
}



#define MAX_NODENAME_LEN (128)
#define MAX_CHILD_NODES (32)
#define MAX_BLOBS_PER_NODE (32)

#define BLOB_SOCKET_SERVER_ADDRESS "localhost"
#define BLOB_SOCKET_SERVER_PORT (8000)

esp_websocket_client_config_t ws_cfg = {
    .uri = "ws://192.168.50.115",
    .port = 8000,
};


typedef struct blob_node_s
{
    char                 p_name[MAX_NODENAME_LEN];  /* Name of this blob node. */
    int                  blob_idx;
    blob                *p_blob; /* For repeated calls inside a node. All blobs should be identical, except for their data.. */
    int                  n_children;
    struct blob_node_s    *ap_child_nodes[MAX_CHILD_NODES];
    struct blob_node_s    *p_parent_node;
    size_t               blob_size; /* Only valid after complete traverse */
} blob_node;

int _blob_socket_init(const char *addr, int port);

int _blob_socket_start(const char *node_name);
int _blob_socket_float_a(const char *var_name, float *p_var_val, int n);
int _blob_socket_int_a(const char *var_name, int *p_var_val, int n);
int _blob_socket_unsigned_int_a(const char *var_name, unsigned int *p_var_val, int n);
int __blob_aggregate_data(blob_node *p_node, size_t *p_size);
int __blob_assemble_data(blob_node *p_node, unsigned char *p_data, size_t *p_size);
int _blob_node_close(blob_node **pp_blob_node);
int _blob_socket_flush();

int _blob_socket_retrieve_start(const char *node_name);
int _blob_socket_retrieve_float_a(const char *var_name, const float **pp_var_val, int *p_n, int rep);
int _blob_socket_retrieve_int_a(const char *var_name, const int **pp_var_val, int *p_n, int rep);
int _blob_socket_retrieve_unsigned_int_a(const char *var_name, const unsigned int **pp_var_val, int *p_n, int rep);
int __blob_disassemble_data(blob_node **pp_node, unsigned char *p_data, size_t *p_size);
int _blob_socket_retrieve_flush();
extern unsigned int global_time;

struct blob_socket_state_s
{
    blob_node *p_cur_node;
    esp_websocket_client_handle_t p_esp_ws;
};

typedef struct blob_socket_queue_s
{
    unsigned char **pp_queue;
} blob_socket_queue;

typedef struct blob_retrieve_s
{
    blob_node *p_root_node;
    blob_node *p_cur_node;
    unsigned char *p_data; /* Full data */
    size_t n_data;
    blob_socket_queue *p_queue;
    int b_new_data;
};

typedef struct blob_retrieve_s blob_retrieve;
typedef struct blob_socket_state_s blob_socket_state;

blob_socket_state blob_ss = {0};
blob_retrieve blob_sr = {0};

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        printf("WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        printf("WEBSOCKET_EVENT_DISCONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        printf("WEBSOCKET_EVENT_DATA\n");
        printf("Received opcode=%d\n", data->op_code);
        printf("Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);
        /* Only if the event is a data receive */
        if (data->op_code == 2)
        {
            /* Hack */
            if (NULL != blob_sr.p_data)
            {
                free(blob_sr.p_data);
            }
            /* Malloc room for the data but remove the IP address field from the downstream data (server injects)*/
            blob_sr.p_data = (unsigned char*)malloc(data->data_len - 128);
            blob_sr.b_new_data = 1;
            memcpy(blob_sr.p_data, data->data_ptr + 128, data->data_len - 128);
            blob_sr.n_data = data->data_len - 128;
        }

        // xTimerReset(shutdown_signal_timer, portMAX_DELAY);
        break;
    case WEBSOCKET_EVENT_ERROR:
        printf("WEBSOCKET_EVENT_ERROR");
        break;
    }
}

int
_blob_socket_init(const char *addr, int port)
{
    esp_err_t err;
    ws_cfg.uri = addr;
    ws_cfg.port = port;
    blob_ss.p_esp_ws = esp_websocket_client_init(&ws_cfg);
    esp_websocket_register_events(blob_ss.p_esp_ws, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void *)(&blob_sr));
    err = esp_websocket_client_start(blob_ss.p_esp_ws);
    if (ESP_OK != err)
    {
        printf("Error establishing connection with server.");
        return -1;
    }
    return 0;
}

int
_blob_socket_terminate()
{
    esp_err_t err;
    err = esp_websocket_client_stop(blob_ss.p_esp_ws);
    esp_websocket_client_destroy(blob_ss.p_esp_ws);
    return 0;
}

int
_blob_node_close(blob_node **pp_blob_node)
{
    blob_node *p_blob_node = *pp_blob_node;
    for (int i=0; i<p_blob_node->n_children; i++)
    {
        /* Must free child nodes before attempting to free this node */
        assert(NULL == p_blob_node->ap_child_nodes[i]);
        return BLOB_ERR;
    }

    blob_close(&p_blob_node->p_blob);
    free(*pp_blob_node);
    *pp_blob_node = NULL;
    return BLOB_OK;
}

int
_blob_socket_start(const char *node_name)
{
    blob_node *p_parent_temp;
    if (NULL == blob_ss.p_cur_node)
    {
        blob_ss.p_cur_node = (blob_node*)calloc(sizeof(blob_node), 1);
        strcpy(blob_ss.p_cur_node->p_name, node_name);
    }
    else
    {   
        int b_found_node = 0;
        if (0 == strcmp(blob_ss.p_cur_node->p_name, node_name))
        {
            b_found_node = 1;
        }
        for (int i=0; i<blob_ss.p_cur_node->n_children; i++)
        {
            if (0 == strcmp(blob_ss.p_cur_node->ap_child_nodes[i]->p_name, node_name))
            {
                p_parent_temp = blob_ss.p_cur_node;
                blob_ss.p_cur_node = blob_ss.p_cur_node->ap_child_nodes[i];
                blob_ss.p_cur_node->p_parent_node = p_parent_temp;
                b_found_node = 1;
            }
        }
        if (!b_found_node)
        {
            p_parent_temp = blob_ss.p_cur_node;
            /* Create the node */
            blob_ss.p_cur_node->ap_child_nodes[blob_ss.p_cur_node->n_children] = (blob_node*)calloc(sizeof(blob_node), 1);
            
            /* Switch into the node */
            blob_ss.p_cur_node = blob_ss.p_cur_node->ap_child_nodes[blob_ss.p_cur_node->n_children];
            blob_ss.p_cur_node->p_parent_node = p_parent_temp;
            blob_ss.p_cur_node->p_parent_node->n_children++;
            strcpy(blob_ss.p_cur_node->p_name, node_name);
        }
    }
    return BLOB_OK;
}

int
_blob_socket_float_a(const char *var_name, float *p_var_val, int n)
{
    if (NULL == blob_ss.p_cur_node->p_blob)
    {
        /* Create a new blob */
        blob_cfg cfg;
        cfg.protocol = BLOB_PROTOCOL_VERSION_FIRST;
        blob_init(&blob_ss.p_cur_node->p_blob, &cfg);
    }
    blob_float_a(blob_ss.p_cur_node->p_blob, var_name, p_var_val, n);
    return BLOB_OK;
}

int
_blob_socket_int_a(const char *var_name, int *p_var_val, int n)
{
    if (NULL == blob_ss.p_cur_node->p_blob)
    {
        /* Create a new blob */
        blob_cfg cfg;
        cfg.protocol = BLOB_PROTOCOL_VERSION_FIRST;
        blob_init(&blob_ss.p_cur_node->p_blob, &cfg);
    }
    blob_int_a(blob_ss.p_cur_node->p_blob, var_name, p_var_val, n);
    return BLOB_OK;
}

int
_blob_socket_unsigned_int_a(const char *var_name, unsigned int *p_var_val, int n)
{
    if (NULL == blob_ss.p_cur_node->p_blob)
    {
        /* Create a new blob */
        blob_cfg cfg;
        cfg.protocol = BLOB_PROTOCOL_VERSION_FIRST;
        blob_init(&blob_ss.p_cur_node->p_blob, &cfg);
    }
    blob_unsigned_int_a(blob_ss.p_cur_node->p_blob, var_name, p_var_val, n);
    return BLOB_OK;
}

int
_blob_socket_flush()
{
    if (NULL == blob_ss.p_cur_node->p_parent_node)
    {
        FILE *p_file;
        size_t total_size;
        unsigned char *p_full_tree_blob;
        size_t total_size_copied;
        int n_sent;

        /* This is the root node */
        __blob_aggregate_data(blob_ss.p_cur_node, &total_size);
        p_full_tree_blob = (unsigned char*)calloc(sizeof(unsigned char), total_size);
        memset(p_full_tree_blob, 0, total_size);
    
        total_size_copied = total_size;
        /* Now, serialise the data */
        __blob_assemble_data(blob_ss.p_cur_node, p_full_tree_blob, &total_size_copied);

        /* All data be filled */
        assert(total_size_copied == 0);

        /* For now, write to file */
        global_time = micros();
        n_sent = esp_websocket_client_send_bin(blob_ss.p_esp_ws, (char *)p_full_tree_blob, total_size, 1000);
        global_time = micros() - global_time;
        if (n_sent != total_size)
        {
            printf("Full amount of data not transmitted.\n");
        }
        if (n_sent == -1)
        {
            printf("Error calling esp_websocket_client_send.\n");
            return -1;
        }
       
        free(p_full_tree_blob);
        _blob_node_close(&blob_ss.p_cur_node);
    }
    else
    {
        size_t this_blob_size = 0;
        unsigned char *p_data;
        if (NULL != blob_ss.p_cur_node->p_blob)
        {
            blob_get_data(blob_ss.p_cur_node->p_blob, &p_data, &this_blob_size);
        }
        else
        {
            blob_ss.p_cur_node->blob_size = 0;
        }
        
        
        blob_ss.p_cur_node->blob_size = this_blob_size;
        blob_ss.p_cur_node = blob_ss.p_cur_node->p_parent_node;
    }
    
    return BLOB_OK;
}

int
__blob_header_get_size(blob *p_blob, size_t *p_size)
{
    int *p_var_len;
    int *p_var_types;
    char *p_var_names;
    int n_vars;
    int n_repetitions;
    size_t total_size = 0;

    /* All blobs are the same, just with different data */
    blob_get_info(p_blob,
                  &p_var_len,
                  &p_var_types,
                  &p_var_names,
                  &n_vars,
                  &n_repetitions);

    total_size += n_vars * sizeof(int); /* Store the length of each variable */
    total_size += n_vars * sizeof(int); /* Store the types of each variable */
    total_size += n_vars * sizeof(char) * BLOB_MAX_VAR_NAME_LEN; /* Store the variable names */
    total_size += sizeof(int); /* Store the number of variables in the blob */
    total_size += sizeof(int); /* Store the number of repetititions of the variables in the blob */
    *p_size = total_size;
    return 0;
}

int
__blob_aggregate_data(blob_node *p_node, size_t *p_size)
{
    size_t total_size = 0;

    size_t tmp_size;
    unsigned char *p_data;
    if (NULL != p_node->p_blob)
    {
        blob_get_data(p_node->p_blob, &p_data, &tmp_size);
        total_size += tmp_size;
    }
    
    for (int i=0; i<p_node->n_children; i++)
    {
        size_t tmp_size = 0;
        __blob_aggregate_data(p_node->ap_child_nodes[i], &tmp_size);
        total_size += tmp_size;
    }

    if (NULL != p_node->p_blob)
    {
        __blob_header_get_size(p_node->p_blob, &tmp_size);
        total_size += tmp_size;
    }
    
    total_size += sizeof(int); /* node has blob ? */
    total_size += sizeof(unsigned int); /* number of children */
    total_size += sizeof(char) * MAX_NODENAME_LEN; /* name of the node */

    p_node->blob_size = total_size;
    *p_size = total_size;
    return 0;
}

int
__blob_assemble_data(blob_node *p_node, unsigned char *p_data, size_t *p_size)
{
    size_t size_left = *p_size;
    int n_initial_children = p_node->n_children;
    while (p_node->n_children > 0)
    {
        __blob_assemble_data(p_node->ap_child_nodes[p_node->n_children-1], p_data, &size_left);
        _blob_node_close(&p_node->ap_child_nodes[p_node->n_children-1]);
        p_node->n_children--;
    }

    {
        int  *p_var_len;
        int  *p_var_types;
        char *p_var_names;
        int   n_vars;
        int   n_repetitions;
        unsigned char *p_blob_data;
        size_t blob_size;
        int b_node_has_blob;

        /* This node has no children. Copy blob contents over. */
        if (NULL != p_node->p_blob)
        {
            b_node_has_blob = 1;
            blob_get_data(p_node->p_blob, &p_blob_data, &blob_size);
            memcpy(p_data + size_left - blob_size, p_blob_data, blob_size);
            size_left -= blob_size;
            /* write the blob header */
            blob_get_info(p_node->p_blob,
                            &p_var_len,
                            &p_var_types,
                            &p_var_names,
                            &n_vars,
                            &n_repetitions);
            /* Copy the variable lengths into the buffer */
            memcpy(p_data + size_left - (n_vars * sizeof(int)), p_var_len, n_vars * sizeof(int));
            size_left -= (n_vars * sizeof(int));
            /* Copy the variable of types into the buffer */
            memcpy(p_data + size_left - (n_vars * sizeof(int)), p_var_types, n_vars * sizeof(int));
            size_left -= (n_vars * sizeof(int));
            /* Copy the variable names into the buffer */
            memcpy(p_data + size_left - (n_vars * sizeof(char) * BLOB_MAX_VAR_NAME_LEN), p_var_names, n_vars * sizeof(char) * BLOB_MAX_VAR_NAME_LEN);
            size_left -= (n_vars * sizeof(char) * BLOB_MAX_VAR_NAME_LEN);
            /* Copy the number of variables per blob */
            memcpy(p_data + size_left - sizeof(int), &n_vars, sizeof(int));
            size_left -= sizeof(int);
            /* Copy the number of repetitions of the variables in each blob */
            memcpy(p_data + size_left - sizeof(int), &n_repetitions, sizeof(int));
            size_left -= sizeof(int);
        }
        else
        {
            b_node_has_blob = 0;
            blob_size = 0;
        }
        
        /* Write if this node has a blob */
        memcpy(p_data + size_left - sizeof(int), &b_node_has_blob, sizeof(int));
        size_left -= sizeof(int);

        /* Write the number of children */
        memcpy(p_data + size_left - sizeof(int), &n_initial_children, sizeof(int));
        size_left -= sizeof(int);
        /* Write the name of the node */
        memcpy(p_data + size_left - (sizeof(char) * MAX_NODENAME_LEN), p_node->p_name, sizeof(char) * MAX_NODENAME_LEN);
        size_left -= (sizeof(char) * MAX_NODENAME_LEN);
    }
    *p_size = size_left;
    return 0;
}   

int
__blob_disassemble_data(blob_node **pp_node, unsigned char *p_data, size_t *p_size)
{
    size_t total_size = 0;
    blob_node *p_node = *pp_node;
    int b_has_blob;
    int child;
    if (p_node == NULL)
    {
        /* Only allocate a new node if there is not an existing one to repopulate */
        p_node = (blob_node*)calloc(sizeof(blob_node), 1);
        *pp_node = p_node;
        /* Assume blob structure unchanging across each blob tree - this does entire process does not
           need be re-executed for each new blob */
        memcpy(p_node->p_name, p_data + total_size, sizeof(char)*MAX_NODENAME_LEN);
        total_size += sizeof(char)*MAX_NODENAME_LEN;
        memcpy(&p_node->n_children, p_data + total_size, sizeof(int));
        total_size += sizeof(int);
        memcpy(&b_has_blob, p_data + total_size, sizeof(int));
        total_size += sizeof(int);
        if ((b_has_blob) && (NULL == p_node->p_blob))
        {
            blob_cfg cfg = {0};
            blob_init(&p_node->p_blob, &cfg);
        }
        
        if (b_has_blob)
        {
            int i, rep;
            unsigned int offset=0;
            size_t blob_size = 0;            
            blob_set_from_data(p_node->p_blob, p_data + total_size, &blob_size);
            total_size += blob_size;
        }
        for (child=0; child<p_node->n_children; child++)
        {
            size_t size_child;
            __blob_disassemble_data(&p_node->ap_child_nodes[child], p_data + total_size, &size_child);
            p_node->ap_child_nodes[child]->p_parent_node = p_node;
            total_size += size_child;
        }
        p_node->blob_size = total_size;
    }
    else
    {
        /* Fast method for updating blob data w.r.t a new packet. Assumes consistent packet sizes */
        size_t blob_size = blob_get_serialized_data_size(p_node->p_blob);
        size_t node_header_size = sizeof(char)*MAX_NODENAME_LEN + 2 * sizeof(int);
        if (NULL != p_node->p_blob)
        {
            blob_update_root_data(p_node->p_blob, p_data + node_header_size);
        }
        
        for (child=0; child<p_node->n_children; child++)
        {
            size_t size_child;
            __blob_disassemble_data(&p_node->ap_child_nodes[child], p_data + node_header_size + blob_size, &size_child);
        }
    }

    *p_size = p_node->blob_size;
    return 0;
}   

void
__blob_socket_retrieve_queue_push(blob_socket_queue *p_queue, unsigned char *p_new_data, size_t size)
{
    (void)p_queue;
    (void)p_new_data;
    (void)size;
}

void
__blob_socket_retrieve_queue_pull(blob_socket_queue *p_queue, unsigned char **pp_queue_pop, size_t *p_queue_pop_size)
{
    (void)p_queue;
    (void)pp_queue_pop;
    (void)p_queue_pop_size;
}

int
_blob_socket_retrieve_start(const char *p_name)
{
    int child;
    size_t recv_total_size = blob_sr.n_data;
    unsigned char *p_recv_data = blob_sr.p_data;

    if (  (NULL == blob_sr.p_root_node) || (blob_sr.p_cur_node == blob_sr.p_root_node))
    {
        /* Root node */
        /* Read into the queue. Override the queue if it already exists */
        __blob_socket_retrieve_queue_push(blob_sr.p_queue, p_recv_data, recv_total_size);
        __blob_socket_retrieve_queue_pull(blob_sr.p_queue, &blob_sr.p_data, &blob_sr.n_data);
        /* For now */
        blob_sr.p_data = p_recv_data;
        blob_sr.n_data = recv_total_size;
        // if (blob_sr.n_data > 0)
        // {
        //     printf("node name: %s\n", (char*)blob_sr.p_data);
        // }
    }
    
    /* Pretty sure I don't need to maintain a variable called p_root_node here */
    if (NULL != blob_sr.p_data)
    {
        if (  (NULL == blob_sr.p_root_node))
        {
            size_t total_size;
            /* Disassemble the data and create the node-tree */
            __blob_disassemble_data(&blob_sr.p_root_node, blob_sr.p_data, &total_size);

            if (total_size != blob_sr.n_data)
            {
                printf("Error decoding packet; size mismatch. Decode size %u, data size %u.\n", total_size, blob_sr.n_data);
            }
            /* Could both be NULL, so update p_cur_node to the root node since disassemble will do the allocate */
            blob_sr.p_cur_node = blob_sr.p_root_node;
        }
        else if (  (blob_sr.p_cur_node == blob_sr.p_root_node)
            &&(0 == strcmp(p_name, blob_sr.p_root_node->p_name))
            )
        {
            size_t total_size;
            
            /* Disassemble the data and create the node-tree */
            __blob_disassemble_data(&blob_sr.p_root_node, blob_sr.p_data, &total_size);
            
            if (total_size != blob_sr.n_data)
            {
                printf("Error decoding packet; size mismatch. Decode size %u, data size %u.\n", total_size, blob_sr.n_data);
            }

            /* Could both be NULL, so update p_cur_node to the root node since disassemble will do the allocate */
            blob_sr.p_cur_node = blob_sr.p_root_node;
        }
        else
        {
            int next = -1;
            for (child=0; child<blob_sr.p_root_node->n_children; child++)
            {
                if (0 == strcmp(p_name, blob_sr.p_cur_node->p_name))
                {
                    next = child;   
                }
            }
            if (next == -1)
            {
                printf("Error, invalid node name\n");
                return -1;
            }
            blob_sr.p_cur_node = blob_sr.p_cur_node->ap_child_nodes[next];
        }
    }
    blob_sr.p_data = p_recv_data;
    blob_sr.n_data = recv_total_size;
    return 0;
}


int _blob_socket_retrieve_float_a(const char *var_name, const float **pp_var_val, int *p_n, int rep)
{
    *p_n = 0;
    *pp_var_val = NULL;
    if (NULL != blob_sr.p_data)
    {
        if (NULL != blob_sr.p_cur_node)
        {
            blob_retrieve_float_a(blob_sr.p_cur_node->p_blob, var_name, pp_var_val, p_n, rep);
        }
    }
    return 0;
}

int _blob_socket_retrieve_int_a(const char *var_name, const int **pp_var_val, int *p_n, int rep)
{
    *p_n = 0;
    *pp_var_val = NULL;
    if (NULL != blob_sr.p_data)
    {
        if (NULL != blob_sr.p_cur_node)
        {
            blob_retrieve_int_a(blob_sr.p_cur_node->p_blob, var_name, pp_var_val, p_n, rep);
        }
    }
    return 0;
}

int
_blob_socket_retrieve_unsigned_int_a(const char *var_name, const unsigned int **pp_var_val, int *p_n, int rep)
{
    *p_n = 0;
    *pp_var_val = NULL;
    if (NULL != blob_sr.p_data)
    {
        if (NULL != blob_sr.p_cur_node)
        {
            blob_retrieve_unsigned_int_a(blob_sr.p_cur_node->p_blob, var_name, pp_var_val, p_n, rep);
        }
    }
    return 0;
}

int
_blob_socket_retrieve_float_a_default(const char *var_name, const float **pp_var_val, int *p_n, float *p_default)
{
    *p_n = 0;
    *pp_var_val = NULL;
    if ((NULL != blob_sr.p_data) && (NULL != blob_sr.p_cur_node))
    {

        blob_retrieve_float_a(blob_sr.p_cur_node->p_blob, var_name, pp_var_val, p_n, 0);
    }
    else
    {
        *pp_var_val = p_default;
    }
    return 0;
}

int
_blob_socket_retrieve_int_a_default(const char *var_name, const int **pp_var_val, int *p_n, int *p_default)
{
    *p_n = 0;
    *pp_var_val = NULL;
    if ((NULL != blob_sr.p_data) && (NULL != blob_sr.p_cur_node))
    {
        blob_retrieve_int_a(blob_sr.p_cur_node->p_blob, var_name, pp_var_val, p_n, 0);
    }
    else
    {
        *pp_var_val = p_default;
    }
    return 0;
}

int
_blob_socket_retrieve_unsigned_int_a_default(const char *var_name, const unsigned int **pp_var_val, int *p_n, unsigned int *p_default)
{
    *p_n = 0;
    *pp_var_val = NULL;
    if ((NULL != blob_sr.p_data) && (NULL != blob_sr.p_cur_node))
    {
        blob_retrieve_unsigned_int_a(blob_sr.p_cur_node->p_blob, var_name, pp_var_val, p_n, 0);
    }
    else
    {
        *pp_var_val = p_default;
    }
    return 0;
}


int
_blob_socket_retrieve_flush()
{
    if (NULL != blob_sr.p_cur_node)
    {
        if (NULL != blob_sr.p_cur_node->p_parent_node)
        {
            blob_sr.p_cur_node = blob_sr.p_cur_node->p_parent_node;
        }
        else
        {
            blob_sr.b_new_data = 0;
        }
    }
    return 0;
}
