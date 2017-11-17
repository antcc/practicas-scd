/**
 * Sistemas concurrentes y distribuidos.
 * Práctica 1: el problema productor-consumidor
 *
 * Antonio Coín Castro.
 */

#include <iostream>
#include <cassert>
#include <thread>
#include <mutex>
#include <random>
#include <atomic>
#include "Semaphore.h"

using namespace std ;
using namespace SEM ;

#define LIFO 0                 // Solución LIFO (1) o FIFO (0)

//**********************************************************************
// variables compartidas
//----------------------------------------------------------------------

const int num_items   = 40,            // número de items por cada productor
	        tam_vec     = 10,             // tamaño del buffer
          num_prods   = 3,
          num_cons    = 2,
          total_items = num_items * num_prods;


unsigned contador_prods = 0,       // controla el número de items producidos
         contador_cons = 0;         // controla el número de items consumidos

unsigned  cont_prod[total_items] = {0}, // contadores de verificación: producidos
          cont_cons[total_items] = {0}; // contadores de verificación: consumidos

Semaphore ocupadas = 0,               // Semáforo que controla las posiciones ocupadas
          libres   = tam_vec;         // Semáforo que controla las posiciones libres

int vec[tam_vec];                     // Buffer

mutex mtx, mtx2, mtx3;                            // Candado

#if LIFO==1
unsigned primera_libre = 0;           // controla buffer LIFO
#endif

//**********************************************************************
// plantilla de función para generar un entero aleatorio uniformemente
// distribuido entre dos valores enteros, ambos incluidos
// (ambos tienen que ser dos constantes, conocidas en tiempo de compilación)
//----------------------------------------------------------------------

template< int min, int max > int aleatorio()
{
  static default_random_engine generador( (random_device())() );
  static uniform_int_distribution<int> distribucion_uniforme( min, max ) ;
  return distribucion_uniforme( generador );
}

//**********************************************************************
// funciones para manipular el buffer
//----------------------------------------------------------------------

void insertar_dato(int dato)
{
  mtx.lock();
#if LIFO==1
  vec[primera_libre++] = dato;
#else
  static unsigned primera_libre = 0; // controla escritura en FIFO
  vec[primera_libre] = dato;
  primera_libre = (primera_libre+1) % tam_vec;
#endif
  mtx.unlock();

  cout << "Insertado: " << dato << endl;
}

//----------------------------------------------------------------------

void extraer_dato(int & dato)
{
  mtx.lock();
#if LIFO==1
  dato = vec[--primera_libre];
#else
  static unsigned primera_ocupada = 0;  // controla lectura en FIFO
  dato = vec[primera_ocupada];
  primera_ocupada = (primera_ocupada+1) % tam_vec;
#endif
  mtx.unlock();

  cout << "                  Extraido: " << dato << endl;
}

//**********************************************************************
// funciones comunes a las dos soluciones (FIFO y LIFO)
//----------------------------------------------------------------------

int producir_dato(unsigned i)
{
   static unsigned valor = 0;
   mtx2.lock();
   unsigned dato = valor++;
   mtx2.unlock();
   this_thread::sleep_for( chrono::milliseconds( aleatorio<20,100>() ));
   cout << "Producido (por hebra " << i << "): " << dato << endl << flush ;
   cont_prod[dato]++;
   return dato;
}
//----------------------------------------------------------------------

void consumir_dato( unsigned dato, int i )
{
   assert( dato < total_items );
   cont_cons[dato]++ ;
   this_thread::sleep_for( chrono::milliseconds( aleatorio<20,100>() ));
   cout << "                  Consumido (por hebra " << i << "): " << dato << endl ;
}


//----------------------------------------------------------------------

bool test_contadores()
{
   bool ok = true ;
   cout << "Comprobando contadores ...." ;
   for( unsigned i = 0 ; i < total_items ; i++ )
   {  if ( cont_prod[i] != 1 )
      {  cout << "error: valor " << i << " producido " << cont_prod[i] << " veces." << endl ;
         ok = false ;
      }
      if ( cont_cons[i] != 1 )
      {  cout << "error: valor " << i << " consumido " << cont_cons[i] << " veces" << endl ;
         ok = false ;
      }
   }
   if (ok)
      cout << endl << flush << "Solución (aparentemente) correcta." << endl << flush ;
   return ok;
}

// comprueba si el contador correspondiente ha llegado al final
// y después lo incrementa
bool test_and_inc(bool is_contador_prods)
{
  bool ultimo;
  mtx3.lock();
  if (is_contador_prods){
    ultimo = contador_prods++ >= total_items;
  }
  else {
    ultimo = contador_cons++ >= total_items;
  }
  mtx3.unlock();

  return ultimo;
}

//----------------------------------------------------------------------

void  funcion_hebra_productora( unsigned i )
{
   while (! test_and_inc(true))
   {
      int dato = producir_dato(i);
      sem_wait(libres);
      insertar_dato(dato);
      sem_signal(ocupadas);
   }
}

//----------------------------------------------------------------------

void funcion_hebra_consumidora( unsigned i )
{
   while(! test_and_inc(false))
   {
      int dato ;
      sem_wait(ocupadas);
      extraer_dato(dato);
      sem_signal(libres);
      consumir_dato( dato, i ) ;
    }
}

//**********************************************************************
// Programa principal
//----------------------------------------------------------------------

int main()
{
  int i;

  cout << "--------------------------------------------------------" << endl
       << "Problema de los productores-consumidores (solución "
       << (LIFO ? "LIFO" : "FIFO") << ")" << endl
       << "--------------------------------------------------------" << endl
       << flush ;

  thread hebras_productoras[num_prods],
         hebras_consumidoras[num_cons];

  for(i = 0; i < num_prods; i++)
    hebras_productoras[i] = thread(funcion_hebra_productora, i);
  for(i = 0; i < num_cons; i++)
    hebras_consumidoras[i] = thread(funcion_hebra_consumidora, i+num_prods);

  for(i = 0; i < num_prods; i++)
    hebras_productoras[i].join();
  for(i = 0; i < num_cons; i++)
    hebras_consumidoras[i].join();

  cout << endl;
  if (test_contadores())
    cout << endl << "------------- FIN -------------" << endl;
  else {
    cout << endl << "!! Hay algún error." << endl;
    return 1;
  }
}
