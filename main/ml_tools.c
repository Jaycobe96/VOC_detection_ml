/*
 * ml_tools.c
 *
 *  Created on: May 20, 2023
 *      Author: Jakub Tomczak
 */
#include "ml_tools.h"

#include <string.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_pthread.h"
#include "pthread.h"
#include <math.h>

static float sqrarg;
#define SQR(a) ((sqrarg=(a)) == 0.0 ? 0.0 : sqrarg*sqrarg)

static float maxarg1,maxarg2;
#define FMAX(a,b) (maxarg1=(a),maxarg2=(b),(maxarg1) > (maxarg2) ?\
        (maxarg1) : (maxarg2))

static int iminarg1,iminarg2;
#define IMIN(a,b) (iminarg1=(a),iminarg2=(b),(iminarg1) < (iminarg2) ?\
        (iminarg1) : (iminarg2))

#define SIGN(a,b) ((b) >= 0.0 ? fabs(a) : -fabs(a))

static void svdcmp(float **a, int m, int n, float w[], float **v);

static float pythag(float a, float b) {
	float absa, absb;
	absa = fabs(a);
	absb = fabs(b);
	if (absa > absb)
		return absa * sqrt(1.0 + SQR(absb / absa));
	else
		return (absb == 0.0 ? 0.0 : absb * sqrt(1.0 + SQR(absa / absb)));
}

matrix_data ml_matrix_normalize(matrix_data A_data) {

	matrix_t A = A_data.data;
	for (size_t i = 0; i < A_data.n_len; i++) {
		ml_data_type sd = 0.0, mean = 0.0, sum = 0.0;

		for (size_t j = 0; j < A_data.m_len; j++) {
			sum += A[j][i];
		}
		mean = sum / A_data.m_len;

		sum = 0.0;
		for (size_t j = 0; j < A_data.m_len; j++) {
			sum += (A[j][i] - mean) * (A[j][i] - mean);
		}
		sd = sqrt(sum / A_data.m_len);

		for (size_t j = 0; j < A_data.m_len; j++) {
			A[j][i] = (A[j][i] - mean) / sd;
		}

	}
	return A_data;
}

void ml_svd(matrix_data A_data, matrix_data *U_out, vector_data *W_out, matrix_data *V_out) {
	vector_data w_data = vector_data_mem_init(A_data.n_len, 0);
	matrix_data V_data = matrix_data_mem_init(A_data.n_len, A_data.n_len, 0);
	matrix_print(A_data, "A");

	svdcmp((float**)A_data.data, A_data.m_len, A_data.n_len, w_data.data, V_data.data);
	vector_print(w_data, "W");
	matrix_print(V_data, "V");
}

static void svdcmp(float **a, int m, int n, float w[], float **v) {
	int flag, i, its, j, jj, k, l, nm;
	float anorm, c, f, g, h, s, scale, x, y, z, *rv1;
	rv1 = vector_mem_init(n, 0);
	g = scale = anorm = 0.0;
	for (i = 1; i <= n; i++) {
		l = i + 1;
		rv1[i-1] = scale * g;
		g = s = scale = 0.0;
		if (i <= m) {
			for (k = i; k <= m; k++)
				scale += fabs(a[k-1][i-1]);
			if (scale) {
				for (k = i; k <= m; k++) {
					a[k-1][i-1] /= scale;
					s += a[k-1][i-1] * a[k-1][i-1];
				}
				f = a[i-1][i-1];
				g = -SIGN(sqrt(s), f);
				h = f * g - s;
				a[i-1][i-1] = f - g;
				for (j = l; j <= n; j++) {
					for (s = 0.0, k = i; k <= m; k++)
						s += a[k-1][i-1] * a[k-1][j-1];
					f = s / h;
					for (k = i; k <= m; k++)
						a[k-1][j-1] += f * a[k-1][i-1];
				}
				for (k = i; k <= m; k++)
					a[k-1][i-1] *= scale;
			}
		}
		w[i-1] = scale * g;
		g = s = scale = 0.0;
		if (i <= m && i != n) {
			for (k = l; k <= n; k++)
				scale += fabs(a[i-1][k-1]);
			if (scale) {
				for (k = l; k <= n; k++) {
					a[i-1][k-1] /= scale;
					s += a[i-1][k-1] * a[i-1][k-1];
				}
				f = a[i-1][l-1];
				g = -SIGN(sqrt(s), f);
				h = f * g - s;
				a[i-1][l-1] = f - g;
				for (k = l; k <= n; k++)
					rv1[k-1] = a[i-1][k-1] / h;
				for (j = l; j <= m; j++) {
					for (s = 0.0, k = l; k <= n; k++)
						s += a[j-1][k-1] * a[i-1][k-1];
					for (k = l; k <= n; k++)
						a[j-1][k-1] += s * rv1[k-1];
				}
				for (k = l; k <= n; k++)
					a[i-1][k-1] *= scale;
			}
		}
		anorm = FMAX(anorm, (fabs(w[i-1]) + fabs(rv1[i-1])));
	}
	for (i = n; i >= 1; i--) {
		if (i < n) {
			if (g) {
				for (j = l; j <= n; j++)
					v[j-1][i-1] = (a[i-1][j-1] / a[i-1][l-1]) / g;
				for (j = l; j <= n; j++) {
					for (s = 0.0, k = l; k <= n; k++)
						s += a[i-1][k-1] * v[k-1][j-1];
					for (k = l; k <= n; k++)
						v[k-1][j-1] += s * v[k-1][i-1];
				}
			}
			for (j = l; j <= n; j++)
				v[i-1][j-1] = v[j-1][i-1] = 0.0;
		}
		v[i-1][i-1] = 1.0;
		g = rv1[i-1];
		l = i;
	}
	for (i = IMIN(m, n); i >= 1; i--) {
		l = i + 1;
		g = w[i-1];
		for (j = l; j <= n; j++)
			a[i-1][j-1] = 0.0;
		if (g) {
			g = 1.0 / g;
			for (j = l; j <= n; j++) {
				for (s = 0.0, k = l; k <= m; k++)
					s += a[k-1][i-1] * a[k-1][j-1];
				f = (s / a[i-1][i-1]) * g;
				for (k = i; k <= m; k++)
					a[k-1][j-1] += f * a[k-1][i-1];
			}
			for (j = i; j <= m; j++)
				a[j-1][i-1] *= g;
		} else
			for (j = i; j <= m; j++)
				a[j-1][i-1] = 0.0;
		++a[i-1][i-1];
	}
	for (k = n; k >= 1; k--) {
		for (its = 1; its <= 30; its++) {
			flag = 1;
			for (l = k; l >= 1; l--) {
				nm = l - 1;
				if ((float) (fabs(rv1[l-1]) + anorm) == anorm) {
					flag = 0;
					break;
				}
				if ((float) (fabs(w[nm-1]) + anorm) == anorm)
					break;
			}
			if (flag) {
				c = 0.0;
				s = 1.0;
				for (i = l; i <= k; i++) {
					f = s * rv1[i-1];
					rv1[i-1] = c * rv1[i-1];
					if ((float) (fabs(f) + anorm) == anorm)
						break;
					g = w[i-1];
					h = pythag(f, g);
					w[i-1] = h;
					h = 1.0 / h;
					c = g * h;
					s = -f * h;
					for (j = 1; j <= m; j++) {
						y = a[j-1][nm-1];
						z = a[j-1][i-1];
						a[j-1][nm-1] = y * c + z * s;
						a[j-1][i-1] = z * c - y * s;
					}
				}
			}
			z = w[k-1];
			if (l == k) {
				if (z < 0.0) {
					w[k-1] = -z;
					for (j = 1; j <= n; j++)
						v[j-1][k-1] = -v[j-1][k-1];
				}
				break;
			}
			if (its == 30)
				printf("no convergence in 30 svdcmp iterations");
			x = w[l-1];
			nm = k - 1;
			y = w[nm-1];
			g = rv1[nm-1];
			h = rv1[k-1];
			f = ((y - z) * (y + z) + (g - h) * (g + h)) / (2.0 * h * y);
			g = pythag(f, 1.0);
			f = ((x - z) * (x + z) + h * ((y / (f + SIGN(g, f))) - h)) / x;
			c = s = 1.0;
			for (j = l; j <= nm; j++) {
				i = j + 1;
				g = rv1[i-1];
				y = w[i-1];
				h = s * g;
				g = c * g;
				z = pythag(f, h);
				rv1[j-1] = z;
				c = f / z;
				s = h / z;
				f = x * c + g * s;
				g = g * c - x * s;
				h = y * s;
				y *= c;
				for (jj = 1; jj <= n; jj++) {
					x = v[jj-1][j-1];
					z = v[jj-1][i-1];
					v[jj-1][j-1] = x * c + z * s;
					v[jj-1][i-1] = z * c - x * s;
				}
				z = pythag(f, h);
				w[j-1] = z;
				if (z) {
					z = 1.0 / z;
					c = f * z;
					s = h * z;
				}
				f = c * g + s * y;
				x = c * y - s * g;
				for (jj = 1; jj <= m; jj++) {
					y = a[jj-1][j-1];
					z = a[jj-1][i-1];
					a[jj-1][j-1] = y * c + z * s;
					a[jj-1][i-1] = z * c - y * s;
				}
			}
			rv1[l-1] = 0.0;
			rv1[k-1] = f;
			w[k-1] = x;
		}
	}
	free(rv1);
}
