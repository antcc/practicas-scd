// -----------------------------------------------------------------------------
//
// Sistemas concurrentes y Distribuidos.
// Práctica 3. Implementación de algoritmos distribuidos con MPI
//
// Archivo: prodcons_varios.cpp
// Implementación del problema del productor-consumidor con
// un proceso intermedio que gestiona un buffer finito y recibe peticiones
// en orden arbitrario.
// (versión con varios productores y varios consumidores)
//
// Antonio Coín Castro
//
// -----------------------------------------------------------------------------


#include <iostream>
#include <thread>
#include <random>
#include <chrono>
#include <mpi.h>

using namespace std;
using namespace std::this_thread;
using namespace std::chrono;


//**********************************************************************
// Variables globales
//----------------------------------------------------------------------

// Parámetros del programa

const int
   np                    = 4,
   nc                    = 5,
   num_procesos_esperado = np + nc + 1,
   num_items             = 100,
   tam_vector            = 40,
   items_por_productor   = num_items / np,
   items_por_consumidor  = num_items / nc;

// Etiquetas MPI

const int
   etiq_productor  = 0,
   etiq_consumidor = 1;


//**********************************************************************
// Plantilla de función para generar un entero aleatorio uniformemente
// distribuido en el rango [min,max]
//----------------------------------------------------------------------

template< int min, int max > int aleatorio()
{
  static default_random_engine generador( (random_device())() );
  static uniform_int_distribution<int> distribucion_uniforme( min, max ) ;
  return distribucion_uniforme( generador );
}


//**********************************************************************
// Produce los números en secuencia (1,2,3,...), y conlleva retraso
// aleatorio
//----------------------------------------------------------------------

int producir( int num_productor )
{
   static int contador = num_productor * items_por_productor;
   sleep_for( milliseconds( aleatorio<10,100>()) );
   contador++;
   cout << "Productor " << num_productor << " ha producido: " << contador
        << endl << flush;
   return contador;
}


//**********************************************************************
// Consume un dato (lo imprime), y conlleva retraso aleatorio
//----------------------------------------------------------------------

void consumir( int valor_cons, int num_consumidor )
{
   sleep_for( milliseconds( aleatorio<110,200>()) );
   cout << "\t\tConsumidor " << num_consumidor << " ha consumido: " << valor_cons
        << endl << flush;
}


//**********************************************************************
// Funciones que implementan el paso de mensajes
//----------------------------------------------------------------------

void funcion_productor( int num_productor )
{
   for ( unsigned i = 0 ; i < items_por_productor ; i++ )
   {
      // producir valor
      int valor_prod = producir( num_productor );
      // enviar valor (id_buffer = np)
      cout << "Productor " << num_productor << " va a enviar: " << valor_prod
           << endl << flush;
      MPI_Ssend( &valor_prod, 1, MPI_INT, np, etiq_productor, MPI_COMM_WORLD );
   }
}

// ---------------------------------------------------------------------

void funcion_consumidor( int num_consumidor )
{
   int         peticion,
               valor_rec;
   MPI_Status  estado;

   for( unsigned i = 0 ; i < items_por_consumidor ; i++ )
   {
      // enviar petición (id_buffer = np)
      MPI_Ssend( &peticion, 1, MPI_INT, np, etiq_consumidor, MPI_COMM_WORLD );
      // recibir y consumir dato
      MPI_Recv ( &valor_rec, 1, MPI_INT, np, etiq_productor, MPI_COMM_WORLD, &estado );
      cout << "\t\tConsumidor " << num_consumidor << " ha recibido: " << valor_rec
           << endl << flush;
      consumir( valor_rec, num_consumidor );
   }
}

// ---------------------------------------------------------------------

void funcion_buffer()
{
   int        buffer[tam_vector],      // buffer con celdas ocupadas y vacías
              valor,                   // valor recibido o enviado
              primera_libre       = 0, // índice de primera celda libre
              primera_ocupada     = 0, // índice de primera celda ocupada
              num_celdas_ocupadas = 0, // número de celdas ocupadas
              etiqueta_aceptable;      // identificador de etiqueta aceptable
   MPI_Status estado;                  // metadatos del mensaje recibido

   for( unsigned i = 0 ; i < num_items * 2 ; i++ )
   {
      // 1. determinar si puede enviar solo productores, solo consumidores, o todos

      if ( num_celdas_ocupadas == 0 )               // si buffer vacío
         etiqueta_aceptable = etiq_productor;       // $~~~$ solo prod.
      else if ( num_celdas_ocupadas == tam_vector ) // si buffer lleno
         etiqueta_aceptable = etiq_consumidor;      // $~~~$ solo cons.
      else                                          // si no vacío ni lleno
         etiqueta_aceptable = MPI_ANY_TAG;          // $~~~$ cualquiera

      // 2. recibir un mensaje con etiqueta aceptable

      MPI_Recv( &valor, 1, MPI_INT, MPI_ANY_SOURCE, etiqueta_aceptable, MPI_COMM_WORLD, &estado );

      // 3. procesar el mensaje recibido

      switch( estado.MPI_TAG ) // leer etiqueta del mensaje en metadatos
      {
         case etiq_productor: // si ha sido un productor: insertar en buffer
            buffer[primera_libre] = valor;
            primera_libre = (primera_libre + 1) % tam_vector;
            num_celdas_ocupadas++;
            cout << "\tBuffer ha recibido: " << valor << endl;
            break;

         case etiq_consumidor: // si ha sido un consumidor: extraer y enviarle
            valor = buffer[primera_ocupada];
            primera_ocupada = (primera_ocupada + 1) % tam_vector;
            num_celdas_ocupadas--;
            cout << "\tBuffer va a enviar: " << valor << endl;
            MPI_Ssend( &valor, 1, MPI_INT, estado.MPI_SOURCE, etiq_productor, MPI_COMM_WORLD );
            break;
      }
   }
}


//**********************************************************************
// Main
//----------------------------------------------------------------------

int main( int argc, char *argv[] )
{
   int id_propio, num_procesos_actual;

   if (num_items % nc != 0 || num_items % np != 0)
   {
     cout << "error: num_items debe ser múltiplo de nc y de np " << endl;
     return 1;
   }

   // inicializar MPI, leer identificador de proceso y número de procesos
   MPI_Init( &argc, &argv );
   MPI_Comm_rank( MPI_COMM_WORLD, &id_propio );
   MPI_Comm_size( MPI_COMM_WORLD, &num_procesos_actual );

   if ( num_procesos_esperado == num_procesos_actual )
   {
     /**
      * Productores: 0,1,...,np - 1
      * Buffer: np
      * Consumidores: np + 1, np + 2, ..., np + nc - 1
      */

      if ( id_propio < np )
         funcion_productor( id_propio );
      else if ( id_propio == np )
         funcion_buffer();
      else
         funcion_consumidor( id_propio );
   }

   else
   {
      if ( id_propio == 0 )
      {
        cout << "error: el número de procesos esperados es " << num_procesos_esperado
             << ", pero el número de procesos en ejecución es " << num_procesos_actual << endl;
        return 1;
      }
   }

   MPI_Finalize();
   return 0;
}
