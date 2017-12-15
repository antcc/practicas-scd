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
// Además, se impone una restricción adicional en la que los productores
// se dividen en dos grupos, atendiendo a la paridad de su id.
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
   items_por_consumidor  = num_items / nc,
   id_buffer             = np;

// Etiquetas MPI

const int
   etiq_productor_m1  = 0,
   etiq_productor_m2  = 1,
   etiq_consumidor    = 2;

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
   int etiq = num_productor % 2 == 0 ? etiq_productor_m2 : etiq_productor_m1;
   for ( unsigned i = 0 ; i < items_por_productor ; i++ )
   {
      // producir valor
      int valor_prod = producir( num_productor );
      // enviar valor
      cout << "Productor " << num_productor << " va a enviar: " << valor_prod
           << endl << flush;
      MPI_Ssend( &valor_prod, 1, MPI_INT, id_buffer, etiq, MPI_COMM_WORLD );
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
      // enviar petición
      MPI_Ssend( &peticion, 1, MPI_INT, id_buffer, etiq_consumidor, MPI_COMM_WORLD );
      // recibir y consumir dato (la etiqueta es irrelevante)
      MPI_Recv ( &valor_rec, 1, MPI_INT, id_buffer, 0, MPI_COMM_WORLD, &estado );
      cout << "\t\tConsumidor " << num_consumidor << " ha recibido: " << valor_rec
           << endl << flush;
      consumir( valor_rec, num_consumidor );
   }
}

// ---------------------------------------------------------------------

void funcion_buffer()
{
   int        buffer[tam_vector],          // buffer con celdas ocupadas y vacías
              valor,                       // valor recibido o enviado
              primera_libre       = 0,     // índice de primera celda libre
              primera_ocupada     = 0,     // índice de primera celda ocupada
              num_celdas_ocupadas = 0,     // número de celdas ocupadas
              etiqueta_aceptable,          // identificador de etiqueta aceptable
              etiq_prod,                   // etiqueta de productor aceptable
              etiq_proxima;                 // etiqueta del siguiente mensaje que se recibiría
   bool       modo1               = true;  // true -> modo1, false -> modo2
   MPI_Status estado;                      // metadatos del mensaje recibido

   for( unsigned i = 0 ; i < num_items * 2 ; i++ )
   {
      // 1. Determinar la etiqueta de productor que debemos aceptar, según el modo

      etiq_prod = modo1 ? etiq_productor_m1 : etiq_productor_m2;

      // 2. Determinar si pueden enviar solo productores, solo consumidores, o todos

      if ( num_celdas_ocupadas == 0 )               // si buffer vacío, solo prod. adecuado
         etiqueta_aceptable = etiq_prod;
      else if ( num_celdas_ocupadas == tam_vector ) // si buffer lleno
         etiqueta_aceptable = etiq_consumidor;      // solo cons.
      else                                          // si no vacío ni lleno
         etiqueta_aceptable = MPI_ANY_TAG;          // cualquiera

      if (etiqueta_aceptable == MPI_ANY_TAG) {
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &estado);
        etiq_proxima = estado.MPI_TAG;

        if ((!modo1 && etiq_proxima == etiq_productor_m1) ||
            (modo1 && etiq_proxima == etiq_productor_m2)) {
          etiqueta_aceptable = etiq_prod;
        }
        else {
          etiqueta_aceptable = etiq_proxima;
        }
      }

      // 3. Recibir un mensaje con etiqueta aceptable

      MPI_Recv( &valor, 1, MPI_INT, MPI_ANY_SOURCE, etiqueta_aceptable, MPI_COMM_WORLD, &estado );

      // 4. Procesar el mensaje recibido

      switch( estado.MPI_TAG ) // leer etiqueta del mensaje en metadatos
      {
         case etiq_productor_m1: // si ha sido un productor: insertar en buffer
         case etiq_productor_m2:
            buffer[primera_libre] = valor;
            primera_libre = (primera_libre + 1) % tam_vector;
            num_celdas_ocupadas++;
            modo1 = !modo1;  // cambiamos de modo
            cout << "\tBuffer ha recibido: " << valor << endl;
            break;

         case etiq_consumidor: // si ha sido un consumidor: extraer y enviarle
            valor = buffer[primera_ocupada];
            primera_ocupada = (primera_ocupada + 1) % tam_vector;
            num_celdas_ocupadas--;
            cout << "\tBuffer va a enviar: " << valor << endl;
            // La etiqueta es irrelevante
            MPI_Ssend( &valor, 1, MPI_INT, estado.MPI_SOURCE, 0, MPI_COMM_WORLD );
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
      * Productores: 0,1,...,id_buffer - 1
      * Buffer: id_buffer
      * Consumidores: id_buffer + 1, id_buffer + 2, ..., id_buffer + nc - 1
      */

      if ( id_propio < id_buffer )
         funcion_productor( id_propio );
      else if ( id_propio == id_buffer )
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
      }
   }

   MPI_Finalize();
   return 0;
}
