// -----------------------------------------------------------------------------
// Sistemas Concurrentes y Distribuidos. Seminario 1.
// Implementación concurrente del cálculo numérico de una integral
// Antonio Coín Castro.
//
// Compilación:
//   $ g++ -std=c++11 -o integral integral.cpp -lpthread
// -----------------------------------------------------------------------------

#include <iostream>
#include <iomanip>
#include <chrono>
#include <future>
#include <vector>
#include <cmath>

using namespace std;
using namespace std::chrono;

const long m = 1024l*1024l*1024l;
const long n = 4;

// -----------------------------------------------------------------------------
// evalua la función $f$ a integrar ($f(x)=4/(1+x^2)$)
double f(double x)
{
  return 4.0/(1.0+x*x) ;
}

// -----------------------------------------------------------------------------
// calcula la integral de forma secuencial, devuelve resultado:
double calcular_integral_secuencial()
{
   double suma = 0.0;                       // inicializar suma
   for (long i = 0; i < m; i++)             // para cada $i$ entre $0$ y $m-1$:
     suma += f((i + double(0.5)) / m);      // $~$ añadir $f(x_i)$ a la suma actual
   return suma/m;                           // devolver valor promedio de $f$
}

// -----------------------------------------------------------------------------
// función que ejecuta cada hebra de forma contigua.
// Recibe $i$ ==índice de la hebra, ($0\leq i<n$)
// El valor $f(x_i)$ es calculado por la hebra $i/n$
double funcion_hebra_contigua(long i)
{
  double suma = 0.0;
  for (int j = (i-1)*(m/n); j < i*(m/n); j++)
    suma += f((j + double(0.5)) / m);
  return suma;
}

// -----------------------------------------------------------------------------
// función que ejecuta cada hebra de forma entrelazada.
// Recibe $i$ ==índice de la hebra, ($0\leq i<n$)
// El valor f(x_i) es calculado por la hebra $i mod n$
double funcion_hebra_entrelazada(long i)
{
  double suma = 0.0;
  for (int j = i; j < m; j += n)
    suma += f((j + double(0.5)) / m);
  return suma;
}

// -----------------------------------------------------------------------------
// calculo de la integral de forma concurrente
double calcular_integral_concurrente(bool contigua)
{
  future<double> futuros[n];
  double suma = 0;
  int i;

  if (contigua)
    for (i = 0; i < n; i++)
      futuros[i] = async( launch::async, funcion_hebra_contigua, i+1);
  else
    for (i = 0; i < n; i++)
      futuros[i] = async( launch::async, funcion_hebra_entrelazada, i+1);

  for (i = 0; i < n; i++)
    suma += futuros[i].get();

  return suma/m;
}
// -----------------------------------------------------------------------------

int main()
{
  time_point<steady_clock> inicio_sec       = steady_clock::now() ;
  const double             result_sec       = calcular_integral_secuencial();
  time_point<steady_clock> fin_sec          = steady_clock::now();
  double                   x                = sin(0.4567);
  time_point<steady_clock> inicio_conc_cont = steady_clock::now();
  double                   result_conc_cont = calcular_integral_concurrente(true);
  time_point<steady_clock> fin_conc_cont    = steady_clock::now();
  double                   y                = sin(0.6747);
  time_point<steady_clock> inicio_conc_entr = steady_clock::now();
  double                   result_conc_entr = calcular_integral_concurrente(false);
  time_point<steady_clock> fin_conc_entr    = steady_clock::now();
  duration<float,milli>    tiempo_sec       = fin_sec - inicio_sec;
  duration<float,milli>    tiempo_conc_cont = fin_conc_cont - inicio_conc_cont;
  duration<float,milli>    tiempo_conc_entr = fin_conc_entr - inicio_conc_entr;
  const float              porc_cont        = 100.0 * tiempo_conc_cont.count() / tiempo_sec.count();
  const float              porc_entr        = 100.0 * tiempo_conc_entr.count() / tiempo_sec.count();

  constexpr double pi = 3.14159265358979323846l;

  cout << "Número de muestras (m)              : " << m << endl
       << "Número de hebras (n)                : " << n << endl
       << setprecision(18)
       << "Valor de PI                         : " << pi << endl
       << "Resultado secuencial                : " << result_sec  << endl
       << "Resultado concurrente (contigua)    : " << result_conc_cont << endl
       << "Resultado concurrente (entrelazada) : " << result_conc_entr << endl
       << setprecision(5)
       << "Tiempo secuencial                   : " << tiempo_sec.count()  << " milisegundos. " << endl
       << "Tiempo concurrente (contigua)       : " << tiempo_conc_cont.count() << " milisegundos. " << endl
       << "Tiempo concurrente (entrelazada)    : " << tiempo_conc_entr.count() << " milisegundos. " << endl
       << setprecision(4)
       << "% t.conc/t.sec. (contigua)          : " << porc_cont << "%" << endl
       << "% t.conc/t.sec. (entrelazada)       : " << porc_entr << "%" << endl;
}
