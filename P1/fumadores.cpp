/**
 * Sistemas concurrentes y distribuidos.
 * Práctica 1: el problema de los fumadores
 *
 * Antonio Coín Castro.
 */

#include <iostream>
#include <cassert>
#include <thread>
#include <mutex>
#include <random> // dispositivos, generadores y distribuciones aleatorias
#include <chrono> // duraciones (duration), unidades de tiempo
#include "Semaphore.h"

using namespace std;
using namespace SEM;

//**********************************************************************
// variables globales
//----------------------------------------------------------------------

const int N = 3;   // Número de fumadores

Semaphore mostr_vacio  = 1,
          ingr_disp[N] = {0,0,0}; // Hay que inicializar manualmente

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

//----------------------------------------------------------------------
// función que produce un ingrediente aleatorio
int producirIngrediente()
{
  // Espera bloqueada con retardo aleatorio
  this_thread::sleep_for(chrono::milliseconds(aleatorio<20,200>() ));
  return aleatorio<0,N-1>();  // Devuelve ingrediente producido
}

//----------------------------------------------------------------------
// función que ejecuta la hebra del estanquero
void funcion_hebra_estanquero(  )
{
  while(true)
  {
    int i = producirIngrediente();
    assert(i<N);
    sem_wait(mostr_vacio);
    cout << "Disponible ingrediente: " << i << endl;
    sem_signal(ingr_disp[i]);
  }
}

//-------------------------------------------------------------------------
// función que simula la acción de fumar, como un retardo aleatoria de la hebra
void fumar( int num_fumador )
{
   // calcular milisegundos aleatorios de duración de la acción de fumar)
   chrono::milliseconds duracion_fumar( aleatorio<20,200>() );

   // informa de que comienza a fumar
    cout << "Fumador " << num_fumador << "  :"
          << " empieza a fumar (" << duracion_fumar.count() << " milisegundos)" << endl;

   // espera bloqueada un tiempo igual a ''duracion_fumar' milisegundos
   this_thread::sleep_for( duracion_fumar );

   // informa de que ha terminado de fumar
    cout << "Fumador " << num_fumador << "  : termina de fumar, comienza espera de ingrediente "
         << num_fumador << "." << endl;

}

//----------------------------------------------------------------------
// función que ejecuta la hebra del fumador
void  funcion_hebra_fumador( int num_fumador )
{
   while( true )
   {
     sem_wait( ingr_disp[num_fumador] ) ;
     cout << "Retirado ingrediente: " << num_fumador << endl;
     sem_signal( mostr_vacio );
     fumar(num_fumador);
   }
}

//**********************************************************************
// programa principal
//----------------------------------------------------------------------

int main()
{
  int i;
  cout << "--------------------------------------------------------" << endl
       << "Problema de los fumadores"                                << endl
       << "--------------------------------------------------------" << endl
       << flush ;

  thread hebra_estanquero ( funcion_hebra_estanquero ),
         hebra_fumador[N];

  for (i=0; i< N; i++)
    hebra_fumador[i] = thread(funcion_hebra_fumador, i);

  /*
   * Es necesario esperar el join de al menos una hebra,
   * pues en otro caso la hebra que ejecutaba main finaliza, y el programa
   * lanza un runtime error.
   */

  hebra_estanquero.join() ;

  /*
   * No es necesario esperar al resto de hebras, pues ninguna de las 4 va a terminar
   * naturalmente.
   */

  //for (int i = 0; i < N; i++)
    //hebra_fumador[i].join() ;
}
