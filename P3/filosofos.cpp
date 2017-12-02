// -----------------------------------------------------------------------------
//
// Sistemas concurrentes y Distribuidos.
// Práctica 3. Implementación de algoritmos distribuidos con MPI
//
// Archivo: filosofos.cpp
// Implementación del problema de los filósofos.
//
// En esta versión se garantiza que no habrá interbloqueo. Para ello,
// basta con que un filósofo coja los tenedores en el orden inverso
// al que lo hacen los demás.
//
// Antonio Coín Castro
//
// -----------------------------------------------------------------------------

#include <mpi.h>
#include <thread>
#include <random>
#include <chrono>
#include <iostream>

using namespace std;
using namespace std::this_thread;
using namespace std::chrono;

//**********************************************************************
// Variables globales
//----------------------------------------------------------------------

const int
   num_filosofos = 5,
   num_procesos  = 2 * num_filosofos;  // Un tenedor por cada filósofo

//**********************************************************************
// Plantilla de función para generar un entero aleatorio uniformemente
// distribuido en el rango [min, max]
//----------------------------------------------------------------------

template< int min, int max > int aleatorio()
{
  static default_random_engine generador( (random_device())() );
  static uniform_int_distribution<int> distribucion_uniforme( min, max );
  return distribucion_uniforme( generador );
}

//**********************************************************************
// Función que simula la acción de comer o pensar
//----------------------------------------------------------------------

void retraso_aleatorio( string mensaje, int id )
{
  cout << "--- Filósofo " << id << " " << mensaje << " ---" << endl;
  sleep_for( milliseconds( aleatorio<10,100>() ) );
}

//**********************************************************************
// Funciones que implementan el paso de mensajes
//----------------------------------------------------------------------

void funcion_filosofos( int id )
{
  int id_ten_izq = (id + 1) % num_procesos,
      id_ten_der = (id + num_procesos - 1) % num_procesos,
      peticion;

  while ( true )
  {
    if (id == 0) // uno de los filósofos debe comenzar al revés
    {
      cout << "Filósofo " << id << " solicita tenedor derecho (" << id_ten_der << ")" << endl;
      MPI_Ssend( &peticion, 1, MPI_INT, id_ten_der, 0, MPI_COMM_WORLD );

      cout << "Filósofo " << id << " solicita tenedor izquierdo (" << id_ten_izq << ")" << endl;
      MPI_Ssend( &peticion, 1, MPI_INT, id_ten_izq, 0, MPI_COMM_WORLD );
    }
    else
    {
      cout << "Filósofo " << id << " solicita tenedor izquierdo (" << id_ten_izq << ")" << endl;
      MPI_Ssend( &peticion, 1, MPI_INT, id_ten_izq, 0, MPI_COMM_WORLD );

      cout << "Filósofo " << id << " solicita tenedor derecho (" << id_ten_der << ")" << endl;
      MPI_Ssend( &peticion, 1, MPI_INT, id_ten_der, 0, MPI_COMM_WORLD );
    }

    retraso_aleatorio("comienza a comer", id);

    cout << "Filósofo " << id << " suelta tenedor izquierdo (" << id_ten_izq << ")" << endl;
    MPI_Ssend( &peticion, 1, MPI_INT, id_ten_izq, 0, MPI_COMM_WORLD );

    cout << "Filósofo " << id <<" suelta tenedor derecho (" << id_ten_der << ")" << endl;
    MPI_Ssend( &peticion, 1, MPI_INT, id_ten_der, 0, MPI_COMM_WORLD );

    retraso_aleatorio("comienza a pensar", id);
  }
}

// ---------------------------------------------------------------------

void funcion_tenedores( int id )
{
  int        valor,
             id_filosofo ;
  MPI_Status estado ;

  while ( true )
  {
     // Recibir petición de cualquier filósofo
     MPI_Recv( &valor, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &estado );
     id_filosofo = estado.MPI_SOURCE;
     cout << "\t\tTenedor " << id <<" cogido por filósofo " << id_filosofo << endl;

     // Recibir liberación de filósofo 'id_filosofo'
     MPI_Recv( &valor, 1, MPI_INT, id_filosofo, 0, MPI_COMM_WORLD, &estado );
     cout << "\t\tTenedor " << id << " liberado por filósofo " << id_filosofo << endl;
  }
}

//**********************************************************************
// Main
//----------------------------------------------------------------------

int main( int argc, char** argv )
{
   int id_propio, num_procesos_actual;

   MPI_Init( &argc, &argv );
   MPI_Comm_rank( MPI_COMM_WORLD, &id_propio );
   MPI_Comm_size( MPI_COMM_WORLD, &num_procesos_actual );

   if ( num_procesos == num_procesos_actual )
   {
      if ( id_propio % 2 == 0 )          // si es par
         funcion_filosofos( id_propio ); // es un filósofo
      else                               // si es impar
         funcion_tenedores( id_propio ); // es un tenedor
   }
   else
   {
      if ( id_propio == 0 )
      {
        cout << "error: el número de procesos esperados es " << num_procesos
             << ", pero el número de procesos en ejecución es: " << num_procesos_actual << endl;
      }
   }

   MPI_Finalize();
   return 0;
}
