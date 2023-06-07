/*
 * heapsort.h
 *
 *  Created on: May 27, 2023
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_HEAPSORT_H_
#define MAIN_HEAPSORT_H_

#include <stdint.h>
#include "ml_types.h"

typedef ml_data_type base_t;

void heapsort(base_t* A, size_t n);

#endif /* MAIN_HEAPSORT_H_ */
