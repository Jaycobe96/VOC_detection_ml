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

typedef struct{
	matrix_data U;
	vector_data W;
	matrix_data V;
}svd_uwv;
matrix_data ml_calc_R(matrix_data Q_data, matrix_data A_data);
matrix_data ml_calc_Q(matrix_data A_data);
matrix_data ml_project_axis(matrix_data A_data, matrix_data V_t);
matrix_data ml_matrix_normalize(matrix_data A_data, vector_data mean_out, vector_data sd_out);
svd_uwv ml_svd(matrix_data A_data);


#endif /* MAIN_ML_TOOLS_H_ */
