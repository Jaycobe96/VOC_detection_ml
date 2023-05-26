/*
 * ml_sampler.c
 *
 *  Created on: May 18, 2023
 *      Author: Jakub Tomczak
 */

#include "ml_sampler.h"

#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_pthread.h"
#include "pthread.h"

typedef struct d_list d_list_t;

struct d_list{
	ml_data_type* data;
    d_list_t* prev;
    d_list_t* next;
};

pthread_mutex_t mtx_ml_sampler_queue;

static d_list_t *ml_data_head = NULL;
static d_list_t *ml_data_tail = NULL;
static size_t M_size_bytes_max = 0;
static size_t M_size_bytes = 0;
static size_t N_len = 0;

static d_list_t* queue_push(d_list_t* origin, ml_data_type* data, size_t len);
static d_list_t* queue_pop(d_list_t* target, ml_data_type*, size_t len);

// Queue
static d_list_t* queue_push(d_list_t* origin, ml_data_type* data, size_t len) {
   if(origin == NULL) {
       origin = (d_list_t*) malloc(sizeof(d_list_t));
       origin->data = (ml_data_type*) malloc(sizeof(ml_data_type) * len);
       origin->next = NULL;
       origin->prev = NULL;
       memcpy(origin->data, data, sizeof(ml_data_type) * len);
       return origin;
   }
   while(origin->next != NULL) {
       origin = origin->next;
   }
   d_list_t* node_new = (d_list_t*) malloc(sizeof(d_list_t));
   if(node_new == NULL) {
	   return NULL;
   }
   node_new->data = (ml_data_type*) malloc(sizeof(ml_data_type) * len);
   memcpy(node_new->data, data, sizeof(ml_data_type) * len);
   node_new->prev = origin;
   node_new->next = NULL;
   origin->next = node_new;
   return node_new;
}

static d_list_t* queue_pop(d_list_t *target, ml_data_type *data, size_t len) {
	d_list_t* new_edge = NULL;
	if(target->prev != NULL)  {
		new_edge = target->prev;
		new_edge->next = NULL;
	} else if(target->next != NULL) {
		new_edge = target->next;
		new_edge->prev = NULL;
	}

	if(target != NULL) {
		if(target->data != NULL) {
			if(data != NULL) {
				memcpy(data, target->data, sizeof(ml_data_type) * len);
			}
			free(target->data);
		}
		free(target);
	}
	return new_edge;
}

size_t ml_sampler_get_M_len(void){
	return N_len;
}

void ml_sampler_data_clear_all(void) {

	 if(pthread_mutex_lock(&mtx_ml_sampler_queue)){
		 printf("mtx lock error\n");
	 }

	while(ml_data_head != NULL) {
		if(ml_data_head->data != NULL) {
			free(ml_data_head->data);
		}
		d_list_t *tmp = ml_data_head;
		ml_data_head = ml_data_head->next;
		free(tmp);
	}
	ml_data_tail = NULL;
	M_size_bytes = 0;

	 if(pthread_mutex_unlock(&mtx_ml_sampler_queue)){
		 printf("mtx unlock error\n");
	 }
}

void ml_sampler_init(size_t row_attribute_count, size_t buff_max_size_bytes) {
	N_len = row_attribute_count;
	M_size_bytes_max = buff_max_size_bytes;
	ml_data_head = NULL;
	ml_data_tail = NULL;

	if(pthread_mutex_init(&mtx_ml_sampler_queue, NULL)) {
		printf("mtx init error");
	}
}

matrix_data ml_sampler_data_collect(void) {
	size_t M_len = M_size_bytes / ((sizeof(ml_data_type) * N_len) + sizeof(d_list_t));

	ml_data_type** new_A = (ml_data_type**) malloc(M_len * sizeof(ml_data_type*));

	if(new_A == NULL) {
		printf("Could not allocate matrix\n");
	}

	for(size_t i = 0; i < M_len; i++) {
		new_A[i] = (ml_data_type*) malloc(N_len * sizeof(ml_data_type));
	}

	size_t ite = 0;
	while(ml_sampler_data_pop(new_A[ite++]) == 1 && ite < M_len);

	matrix_data out;
	out.n_len = N_len;
	out.m_len = M_len;
	out.data = (matrix_t) new_A;

	return out;
}

uint8_t ml_sampler_data_push(ml_data_type *data) {

	assert(N_len > 0);

	const size_t size_node_bytes = (sizeof(ml_data_type) * N_len) + sizeof(d_list_t);

	if(pthread_mutex_lock(&mtx_ml_sampler_queue)){
		printf("mtx lock error\n");
	}

	// pop last node if buffer size would exceed max. buffer size allowed
	if(M_size_bytes_max <= M_size_bytes + size_node_bytes && M_size_bytes >= size_node_bytes) {
		ml_data_head = queue_pop(ml_data_head, NULL, 0);
		M_size_bytes = 0;
	}

	if(ml_data_head == NULL) {
		ml_data_head = queue_push(ml_data_head, data, N_len);
		ml_data_tail = ml_data_head;
	} else {
		ml_data_tail = queue_push(ml_data_tail, data, N_len);
	}

	M_size_bytes += size_node_bytes;
	printf("bytes:%u\n", M_size_bytes);

	 if(pthread_mutex_unlock(&mtx_ml_sampler_queue)){
		 printf("mtx unlock error\n");
	 }

	if(ml_data_tail == NULL || ml_data_head == NULL) {
		return 0;
	}
	return 1;
}

uint8_t ml_sampler_data_pop(ml_data_type *data) {

	assert(N_len > 0);

	const size_t size_node_bytes = (sizeof(ml_data_type) * N_len) + sizeof(d_list_t);

	if(pthread_mutex_lock(&mtx_ml_sampler_queue)) {
		 printf("mtx lock error\n");
	}

	if(M_size_bytes >= size_node_bytes && ml_data_head != NULL) {
		ml_data_head = queue_pop(ml_data_head, data, N_len);
		M_size_bytes -= size_node_bytes;
	} else {
		 if(pthread_mutex_unlock(&mtx_ml_sampler_queue)){
			 printf("mtx unlock error\n");
		 }
		return 0;

	}
	 if(pthread_mutex_unlock(&mtx_ml_sampler_queue)){
		 printf("mtx unlock error\n");
	 }
	return 1;
}

// todo: ml_sampler_data_extend()


