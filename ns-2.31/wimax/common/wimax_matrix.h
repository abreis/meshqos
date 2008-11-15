/* 
 *  Copyright (C) 2007 Dip. Ing. dell'Informazione, University of Pisa, Italy
 *  http://info.iet.unipi.it/~cng/ns2mesh80216/
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA, USA
 */

#ifndef __NS2_WIMAX_MATRIX_H
#define __NS2_WIMAX_MATRIX_H

/*!
  Generic matrix data structure.
  */
template<typename T>
class Matrix {

	//! Matrix data structure.
	T* matrix;
	//! Number of rows.
	unsigned int rows;
	//! Number of columns.
	unsigned int cols;

public:
	//! Create an empty matrix.
	Matrix () : matrix(0), rows(0), cols(0) { }
	//! Copy contructor.
	Matrix (const Matrix<T>&);
	//! Free memory.
	~Matrix ();
	//! Return a reference to a (i, j) element. If it does not exist, resize.
	/*!
	  The first argument (i) is the row number, while j is the column number.
	  */
	T& at (unsigned int i, unsigned int j) {
		if ( i >= rows || j >= cols ) resize (i, j);
		return matrix[index(i, j)]; }
	//! Return the number of rows.
	unsigned int getRows () const { return rows; }
	//! Return the number of columns.
	unsigned int getCols () const { return cols; }
	//! Deletes the matrix.
	void reset ();

protected:
	//! Resize a matrix so that (i, j) exists.
	void resize (unsigned int i, unsigned int j);
	//! Get the (linear) index of two-dimension coordinates.
	unsigned int index (unsigned int i, unsigned int j) {
		return j + cols * i; }
	//! Get the (linear) index of two-dimension coordinates, with n columns.
	unsigned int index (unsigned int i, unsigned int j, unsigned int n) {
		return j + n * i; }
	
private:
	void operator= (const Matrix<T>&);
};

template<typename T>
Matrix<T>::Matrix (const Matrix<T>& C)
{
	rows = C.rows;
	cols = C.cols;
	matrix = new T[rows * cols];
	for ( unsigned int i = 0; i < rows * cols; i++ )
		matrix[i] = C.matrix[i];
}

template<typename T>
void Matrix<T>::resize (unsigned int i, unsigned int j)
{
	unsigned int new_rows = ( i < rows ) ? rows : ( i + 1 );
	unsigned int new_cols = ( j < cols ) ? cols : ( j + 1 );

	T* new_matrix = new T[new_rows * new_cols];
	for ( unsigned int it = 0 ; it < new_rows ; it++ )
		for ( unsigned int jt = 0 ; jt < new_cols ; jt++ )
			if ( it < rows && jt < cols )
				new_matrix[index(it, jt, new_cols)] = matrix[index(it, jt)];
			else
				new_matrix[index(it, jt, new_cols)] = 0;

	if ( matrix ) delete[] matrix;
	matrix = new_matrix;
	rows = new_rows;
	cols = new_cols;
}

template<typename T>
void Matrix<T>::reset ()
{
	if ( matrix ) delete[] matrix;
	matrix = 0;
	cols = rows = 0;
}

template<typename T>
Matrix<T>::~Matrix ()
{
	if ( matrix ) delete[] matrix;
}

#endif // __NS2_WIMAX_MATRIX_H
