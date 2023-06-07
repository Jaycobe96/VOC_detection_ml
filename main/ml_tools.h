/*
 * ml_tools.h
 *
 *  Created on: May 20, 2023
 *      Author: Jakub Tomczak
 */

#ifndef MAIN_ML_TOOLS_H_
#define MAIN_ML_TOOLS_H_

#include <stdint.h>
#include <stdlib.h>
#include "matrix.h"
#include "ml_vector.h"


typedef struct {
	matrix_data D;
	matrix_data V_t;
}DVt;

matrix_data ml_matrix_normalize(matrix_data A_data);
matrix_data ml_calc_R(matrix_data Q_data, matrix_data A_data);
matrix_data ml_calc_Q(matrix_data A_data);
matrix_data ml_tridiagonalization(matrix_data A_data);
ml_data_type ml_eigenvalue(matrix_data A_data, vector_data v_data);
void ml_svd(matrix_data A_data);


#endif /* MAIN_ML_TOOLS_H_ */
