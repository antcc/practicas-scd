/**
 * This program paints the Mandelbrot set usign MPI and the escape time algorithm.
 *
 * The library png++ is used to save the mandelbrot final image to a file. Said library
 * is only a C++ wrapper for libpng.
 *
 *
 * Antonio Coín Castro.
 */

#include <iostream>
#include <complex>
#include <cmath>
#include <mpi.h>
#include <png++/png.hpp>

//************************************************************************************
// Customizable program parameters
//------------------------------------------------------------------------------------

//#define LINEAR_COLORING   // Coloring strategy

const float
  precision     = 0.001;    // Precision when sampling complex numbers

const int
  limit         = 1000,     // Max iterations for escape time algorithm
  radius        = 2,        // Complex subset is {z : -radius < Re z, Im z < radius}
  img_W         = 1024,     // Width of final image
  img_H         = 1024;     // Height of final image

const int
  num_slaves    = 3,
  num_processes = num_slaves + 1,
  id_master     = 0,
  tag_send      = 0,
  tag_end       = 1;


//*********************************************************************
// Data structures
//---------------------------------------------------------------------

// A complex number
typedef std::complex<float> Complex;

// A square in the complex plane, sampled from pixels
class SampledPlane {
  private:
    float x_limit;
    float y_limit;
    float jump;

  public:
    SampledPlane(float x_limit, float y_limit, float jump)
      : x_limit(x_limit), y_limit(y_limit), jump(jump) { };

    // Getters
    float x_min() const { return -x_limit; }
    float x_max() const { return x_limit; }
    float y_min() const { return -y_limit; }
    float y_max() const { return y_limit; }
    float get_jump() const { return jump; }

    const int width() const { return (2 * x_limit / jump) + 1; }
    const int height() const { return (2 * y_limit / jump) + 1; }

    Complex pixelToComplex(int i, int j) {
      int W = width();
      int H = height();
      return Complex(-x_limit + (2 * x_limit) / (W - 1) * i,
                     -y_limit + (2 * y_limit) / (H - 1) * j);
    }
};


//***************************************************************************
// Helper functions
//---------------------------------------------------------------------------

// Construct a PNG image and print it to a file (using png++)
void visualize(SampledPlane plane, float ** img) {
  png::image<png::rgb_pixel> img_png(img_W, img_H);
  int W = plane.width();
  int H = plane.height();
  float scale_W = (float) (W-1) / (img_W-1);
  float scale_H = (float) (H-1) / (img_H-1);

  for (int i = 0; i < img_W; i++) {
    for (int j = 0; j < img_H; j++) {
      int x = i * scale_W;
      int y = j * scale_H;
      int color = img[x][y];
      img_png[j][i] = png::rgb_pixel(color, color, color);
    }
  }

  img_png.write("mandelbrot.png");
}

// Compute one iteration of the mandelbrot sequence
Complex function_mandelbrot(Complex z, Complex c) {
  return z * z + c;
}

// Apply the escape time algorithm to calculate the colors of a given row
void calculate_colors(SampledPlane plane, float * img, int i) {
  int n_iterations;
  const int RGB_MAX = 1 << 8;

  for (int j = 0; j < plane.height(); j++) {
    Complex z(0);

    // Scale pixel (i,j) to a complex number in our plane
    Complex c = plane.pixelToComplex(i,j);

    n_iterations = 0;
    while (std::abs(z) <= radius && n_iterations < limit) {
      z = function_mandelbrot(z, c);
      n_iterations++;
    }

#ifdef LINEAR_COLORING
    // Compute color of the pixel from 0 to 255 (linear map)
    float scale = (float) (RGB_MAX-1) / (limit-1);
    img[j] = (limit - n_iterations) * scale;
#else
    // A couple of extra iterations to improve the coloring algorithm
    const int EXTRA_ITER = 3;
    for (int k = 0; k < EXTRA_ITER; k++) {
      z = function_mandelbrot(z,c);
      n_iterations++;
    }

    // Compute color of the pixel from 0 to 255 (continuous coloring)
    if (n_iterations == limit + EXTRA_ITER)
      img[j] = 0.0;
    else
      img[j] = (n_iterations - log(log(abs(z)))/log(radius)) / n_iterations * RGB_MAX-1;
#endif
  }
}


//***************************************************************************************
// Master: send rows to slaves for processing, and ultimately print the resulting image
//---------------------------------------------------------------------------------------
void master(SampledPlane plane) {
  int W = plane.width();
  int H = plane.height();
  int colored_rows = 0;
  int tag;
  int id_slave;
  int row_id;
  int i;
  float** img;
  MPI_Status status;

  // Allocate memory for img
  img = new float*[W];
  for (i = 0; i < W; i++)
    img[i] = new float[H];

  // Send initial rows
  for (i = 0; i < num_slaves; i++) {
    MPI_Send(&i, 1, MPI_INT, i+1, tag_send, MPI_COMM_WORLD);
    MPI_Send(img[i], H, MPI_FLOAT, i+1, tag_send, MPI_COMM_WORLD);
  }

  i--;

  // Receive colored rows and send new ones dynamically
  while (colored_rows < W) {
    MPI_Recv(&row_id, 1, MPI_INT, MPI_ANY_SOURCE, tag_send, MPI_COMM_WORLD, &status);
    id_slave = status.MPI_SOURCE;
    MPI_Recv(img[row_id], H, MPI_FLOAT, id_slave, tag_send, MPI_COMM_WORLD, &status);
    colored_rows++;

    // Decide whether to send more rows or to terminate slave
    if (i < W - 1) {
      tag = tag_send;
      i++;
    }
    else {
      tag = tag_end;
    }

    MPI_Send(&i, 1, MPI_INT, id_slave, tag_send, MPI_COMM_WORLD);
    MPI_Send(img[i], H, MPI_FLOAT, id_slave, tag, MPI_COMM_WORLD);
  }

  // Print resulting image
  visualize(plane, img);

  // Free memory
  for (i = 0; i < W; i++)
    delete[] img[i];
  delete[] img;
}


//************************************************************************************
// Slave: receive rows, compute their colors, and send them back to master
//------------------------------------------------------------------------------------
void slave(SampledPlane plane, int id) {
  float* row;
  int H;
  int row_id;
  MPI_Status status;

  // Allocate memory for each row
  H = plane.height();
  row = new float[H];

  // Receive row_id and row
  MPI_Recv(&row_id, 1, MPI_INT, id_master, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
  MPI_Recv(row, H, MPI_FLOAT, id_master, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

  while (status.MPI_TAG != tag_end) {
    calculate_colors(plane, row, row_id);

    // Send row_id and colored row
    MPI_Send(&row_id, 1, MPI_INT, id_master, tag_send, MPI_COMM_WORLD);
    MPI_Send(row, H, MPI_FLOAT, id_master, tag_send, MPI_COMM_WORLD);

    // Receive more rows
    MPI_Recv(&row_id, 1, MPI_INT, id_master, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    MPI_Recv(row, H, MPI_FLOAT, id_master, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
  }

  // Free memory
  delete[] row;
}


//**************************************************************************************
// Main: initialise MPI environment and run master-slaves
//--------------------------------------------------------------------------------------
int main(int argc, char** argv) {
  int id_self, current_num_processes;
  SampledPlane plane(radius, radius, precision);

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &id_self);
  MPI_Comm_size(MPI_COMM_WORLD, &current_num_processes);

  if (num_processes == current_num_processes) {
    if (id_self == id_master)
      master(plane);
    else
      slave(plane, id_self);
  }

  else {
    if (id_self == id_master) {
      std::cout << "error: expected number of processes is " << num_processes
                << ", but current number of processes is : " << current_num_processes
                << std::endl;
    }
  }

  MPI_Finalize();
  return 0;
}
