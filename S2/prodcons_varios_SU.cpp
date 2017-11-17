// -----------------------------------------------------------------------------
//
// Sistemas concurrentes y Distribuidos.
// Seminario 2. Introducción a los monitores en C++11.
// Antonio Coín Castro.
//
// Archivo: prodcons_varios_SU.cpp
//
// Ejemplo de un monitor en C++11 con semántica SU, para el problema
// de varios productores/consumidores.
// Versión LIFO (stack) y FIFO (queue), controladas mediante opciones de
// precompilación.
//
// -----------------------------------------------------------------------------

#include <iostream>
#include <iomanip>
#include <cassert>
#include <thread>
#include <random>
#include <mutex>
#include "HoareMonitor.hpp"

using namespace std;
using namespace HM;

//#define FIFO                    // comentar esta línea para opción LIFO


//**********************************************************************
// Variables globales
//----------------------------------------------------------------------

constexpr int num_items = 90,         // número de items a producir
              num_prods = 3,          // número de productores (divisor de num_items)
              num_cons = 2,           // número de consumidores (divisor de num_items)
              items_prod = num_items / num_prods,  // número de items por cada productor
              items_cons = num_items / num_cons;   // número de items por cada consumidor


int producidos[num_prods] = {0};      // items producidos por cada hebra

mutex mtx;                            // mutex para escritura en pantalla

unsigned cont_prod[num_items] = {0},  // contadores de verificación: producidos
         cont_cons[num_items] = {0};  // contadores de verificación: consumidos


//**********************************************************************
// Plantilla de función para generar un entero aleatorio uniformemente
// distribuido entre dos valores enteros, ambos incluidos
//----------------------------------------------------------------------

template<int min, int max> int aleatorio()
{
  static default_random_engine generador( (random_device())() );
  static uniform_int_distribution<int> distribucion_uniforme(min, max);
  return distribucion_uniforme(generador);
}


//**********************************************************************
// Funciones comunes a las dos soluciones (FIFO y LIFO)
//----------------------------------------------------------------------

int producir_dato(int h)
{
   int dato = h * (items_prod) + producidos[h];
   producidos[h]++;
   this_thread::sleep_for( chrono::milliseconds( aleatorio<20,100>() ) );
   mtx.lock();
   cout << "Producido (por hebra " << h << "): " << dato << endl << flush;
   mtx.unlock();
   cont_prod[dato]++;
   return dato;
}

//----------------------------------------------------------------------

void consumir_dato(unsigned dato, int h)
{
   if (num_items <= dato) {
     cout << " dato === " << dato << ", num_items == " << num_items << endl;
     assert(dato < num_items);
   }
   cont_cons[dato]++;
   this_thread::sleep_for( chrono::milliseconds( aleatorio<20,100>() ) );
   mtx.lock();
   cout << "\t\tConsumido (por hebra " << h << "): " << dato << endl;
   mtx.unlock();
}

//----------------------------------------------------------------------

bool test_contadores()
{
   bool ok = true;
   cout << "Comprobando contadores ...." << flush ;

   for( unsigned i = 0 ; i < num_items ; i++ )
   {
      if ( cont_prod[i] != 1 )
      {
         cout << "error: valor " << i << " producido " << cont_prod[i] << " veces." << endl ;
         ok = false ;
      }
      if ( cont_cons[i] != 1 )
      {
         cout << "error: valor " << i << " consumido " << cont_cons[i] << " veces" << endl ;
         ok = false ;
      }
   }
   if (ok)
      cout << endl << flush << "Solución (aparentemente) correcta." << endl << flush ;
   return ok;
}


//**********************************************************************
// Monitor para gestionar buffer compartido
//----------------------------------------------------------------------

class MonitorProdCons : public HoareMonitor {
  private:
    static const int TAM_BUFFER = 10;
    int buffer[TAM_BUFFER];
    int primera_libre;
    CondVar ocupadas;
    CondVar libres;

#ifdef FIFO
    int primera_ocupada;
    int n;
#endif

  public:
    MonitorProdCons();
    void leer(int & dato);
    void escribir(int dato);
};


//**********************************************************************
// Implementación métodos MonitorProdCons
//----------------------------------------------------------------------

MonitorProdCons::MonitorProdCons()
{
  primera_libre = 0;
  ocupadas = newCondVar();
  libres = newCondVar();

#ifdef FIFO
  primera_ocupada = 0;
  n = 0;
#endif
}

void MonitorProdCons::leer(int & dato)
{
#ifdef FIFO
  // Esperar bloqueado hasta que 0 < n
  if (n == 0)
    ocupadas.wait();

  // Hacer la operación de lectura, actualizando estado del monitor
  assert(0 < n);
  dato = buffer[primera_ocupada];
  primera_ocupada = (primera_ocupada+1) % TAM_BUFFER;
  n--;
#else
  // Esperar bloqueado hasta que 0 < n
  if (primera_libre == 0)
    ocupadas.wait();

  // Hacer la operación de lectura, actualizando estado del monitor
  assert(0 < primera_libre);
  dato = buffer[--primera_libre];
#endif

  // Señalar al productor que hay un hueco libre, por si está esperando
  libres.signal();
}

//----------------------------------------------------------------------

void MonitorProdCons::escribir(int dato)
{
#ifdef FIFO
  if (n == TAM_BUFFER)
    libres.wait();

  assert(n < TAM_BUFFER);
  buffer[primera_libre] = dato;
  primera_libre = (primera_libre+1) % TAM_BUFFER;
  n++;
#else
  if (primera_libre == TAM_BUFFER)
    libres.wait();

  assert(primera_libre < TAM_BUFFER);
  buffer[primera_libre++] = dato;
#endif

  // señalar al consumidor que ya hay una celda ocupada (por si esta esperando)
  ocupadas.signal();
}


// *****************************************************************************
// Funciones de hebras
// -----------------------------------------------------------------------------

void funcion_hebra_productora(MRef<MonitorProdCons> monitor, int h)
{
   for(unsigned i = 0 ; i < items_prod ; i++) {
      int valor = producir_dato(h);
      monitor->escribir(valor);
   }
}

// -----------------------------------------------------------------------------

void funcion_hebra_consumidora(MRef<MonitorProdCons> monitor, int h)
{
  int valor;
  for(unsigned i = 0 ; i < items_cons ; i++) {
    monitor->leer(valor);
    consumir_dato(valor, h);
   }
}


// *****************************************************************************
// Programa principal
// -----------------------------------------------------------------------------

int main()
{
  int i;
  string version;
#ifdef FIFO
  version = "FIFO";
#else
  version = "LIFO";
#endif

  cout << "--------------------------------------------------------" << endl
       << "Problema de los productores-consumidores (solución "
       << version << ")" << endl
       << "--------------------------------------------------------" << endl
       << endl << flush ;

  auto monitor = Create<MonitorProdCons>();

  thread hebras_productoras[num_prods],
         hebras_consumidoras[num_cons];

  for(i = 0; i < num_prods; i++)
    hebras_productoras[i] = thread(funcion_hebra_productora, monitor, i);
  for(i = 0; i < num_cons; i++)
    hebras_consumidoras[i] = thread(funcion_hebra_consumidora, monitor, i+num_prods);

  for(i = 0; i < num_prods; i++)
    hebras_productoras[i].join();
  for(i = 0; i < num_cons; i++)
    hebras_consumidoras[i].join();

  // Comprobar que cada item se ha producido y consumido exactamente una vez
  cout << endl;
  if (test_contadores())
    cout << endl << "------------- FIN -------------" << endl;
  else {
    cout << endl << "¡Hay algún error!" << endl;
    return 1;
  }
}
