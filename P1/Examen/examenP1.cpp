/**
 * Sistemas Concurrentes y Distribuidos.
 * Examen Práctica 1: el problema de los fumadores
 *
 * Antonio Coín Castro
 * 77191012E
 *
 */

#include <iostream>
#include <cassert>
#include <thread>
#include <random>
#include <chrono>
#include "Semaphore.h"

using namespace std;
using namespace SEM;

//**********************************************************************
// variables globales
//----------------------------------------------------------------------

const int N = 2,     // Número de fumadores
          ITER = 10; // Número de iteraciones antes de necesitar suministros

Semaphore mostr_vacio  = 1,
          puede_suministrar = 1,
          suministra = 0,
          ingr_disp[N] = {0,0}; // Inicializar manualmente

//**********************************************************************
// plantilla de función para generar un entero aleatorio uniformemente
// distribuido entre dos valores enteros, ambos incluidos
// (ambos tienen que ser dos constantes, conocidas en tiempo de compilación)
//----------------------------------------------------------------------

template<int min, int max> int aleatorio()
{
  static default_random_engine generador((random_device())());
  static uniform_int_distribution<int> distribucion_uniforme(min, max) ;
  return distribucion_uniforme(generador);
}

//----------------------------------------------------------------------
// función que produce un ingrediente aleatorio
int producirIngrediente()
{
  // Espera bloqueada con retardo aleatorio
  this_thread::sleep_for(chrono::milliseconds(aleatorio<20, 200>()));
  return aleatorio<0, N-1>();  // Devuelve ingrediente producido
}

//-------------------------------------------------------------------------
// función que simula la acción de fumar, como un retardo aleatoria de la hebra
void fumar(int num_fumador)
{
   // calcular milisegundos aleatorios de duración de la acción de fumar)
   chrono::milliseconds duracion_fumar(aleatorio<20, 200>());

   // informa de que comienza a fumar
    cout << "\tFumador " << num_fumador << "  :"
          << " empieza a fumar (" << duracion_fumar.count() << " milisegundos)" << endl;

   // espera bloqueada un tiempo igual a ''duracion_fumar' milisegundos
   this_thread::sleep_for( duracion_fumar );

   // informa de que ha terminado de fumar
    cout << "\tFumador " << num_fumador << "  : termina de fumar, comienza espera de ingrediente "
         << num_fumador << "." << endl;
}

//----------------------------------------------------------------------
// función que ejecuta la hebra del fumador
void  funcion_hebra_fumador(int num_fumador)
{
   while(true)
   {
     sem_wait( ingr_disp[num_fumador] ) ;
     cout << "\t\tRetirado ingrediente: " << num_fumador << endl;
     sem_signal( mostr_vacio );
     fumar(num_fumador);
   }
}

//----------------------------------------------------------------------
// función que ejecuta la hebra del estanquero
void funcion_hebra_estanquero()
{
  while(true)
  {
    sem_wait(suministra);
    for (int j = 0; j < ITER; j++)
    {
      int i = producirIngrediente();
      sem_wait(mostr_vacio);
      cout << "Disponible ingrediente: " << (i == 0 ? "papel" : "tabaco") << " (" << i  << ")" << endl;
      sem_signal(ingr_disp[i]);
    }
    sem_signal(puede_suministrar);
  }
}

//----------------------------------------------------------------------
// función que ejecuta la hebra suministradora
void funcion_hebra_suministradora()
{
  while (true)
  {
    sem_wait(puede_suministrar);
    cout << "\n*** Ha llegado un nuevo lote de suministros para el estanquero. ***\n" << endl;
    // simula el acto de suministrar
    this_thread::sleep_for( chrono::milliseconds(aleatorio<20, 200>()));
    sem_signal(suministra);
  }
}

//**********************************************************************
// programa principal
//----------------------------------------------------------------------

int main()
{
  cout << "--------------------------------------------------------" << endl
       << "Problema de los fumadores"                                << endl
       << "--------------------------------------------------------" << endl
       << flush ;

  thread hebra_estanquero(funcion_hebra_estanquero),
         hebra_suministradora(funcion_hebra_suministradora),
         hebra_fumador[N];

  for (int i = 0; i < N; i++)
    hebra_fumador[i] = thread(funcion_hebra_fumador, i);

  /*
   * Es necesario esperar el join de al menos una hebra,
   * pues en otro caso la hebra que ejecutaba main finaliza, y el programa
   * lanza un runtime error.
   */

  hebra_estanquero.join() ;

  /*
   * No es necesario esperar al resto de hebras, pues ninguna de las N+1 va a terminar
   * naturalmente.
   */

  //for (int i = 0; i < N; i++)
    //hebra_fumador[i].join() ;

  //hebra_suministradora.join();
}
