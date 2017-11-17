// -----------------------------------------------------------------------------
//
// Sistemas concurrentes y Distribuidos.
// Práctica 2. Casos prácticos de monitores en C++11.
// Antonio Coín Castro.
//
// Archivo: barbero_SU.cpp
//
// Monitor en C++11 con semántica SU, para el problema
// del barbero. Se contemplan las siguientes modificaciones:
//
//    * Hay un número variable de barberos.
//    * Si un cliente llega y la sala de espera está llena, deberá
//      salir de la barbería, esperar fuera un tiempo, y volver a entrar.
//    * Los barberos pueden pelar como máximo a un número determinado de
//      clientes antes de tener que descansar durante un tiempo.
//
// Para eliminar alguna de las dos últimas restricciones, basta con poner
// los parámetros correspondientes a un valor negativo, por ejemplo -1.
// -----------------------------------------------------------------------------

#include <iostream>
#include <cassert>
#include <thread>
#include <random>
#include <mutex>
#include "HoareMonitor.hpp"

using namespace std;
using namespace HM;


//**********************************************************************
// Variables globales
//----------------------------------------------------------------------

// Elección de parámetros del problema

constexpr
int NC = 8,            // Número de clientes
    NB = 2,            // Número de barberos
    MAX_ESPERA = 5,    // Número de asientos en la sala de espera
    MAX_PELADOS = 10;  // Máximo de clientes a los que se puede pelar sin descansar

// Otros

mutex mtx;                  // Exclusión mutua para la salida por pantalla


//**********************************************************************
// Funciones auxiliares
//----------------------------------------------------------------------

// Generar número pseudoaleatorio en el rango [min,max]
template<int min, int max> int aleatorio()
{
  static default_random_engine generador( (random_device())() );
  static uniform_int_distribution<int> distribucion_uniforme(min, max);
  return distribucion_uniforme(generador);
}

//----------------------------------------------------------------------

// Simular la acción de cortar el pelo a un cliente
void cortarPeloACliente(int num_barbero)
{
  chrono::milliseconds duracion_corte( aleatorio<20,200>() );
  mtx.lock();
  cout << "Barbero " << num_barbero << " cortando el pelo a un cliente..."
       << endl;
  mtx.unlock();

  this_thread::sleep_for(duracion_corte);

  mtx.lock();
  cout << "Barbero " << num_barbero << " termina de pelar al cliente."
       << endl;
  mtx.unlock();
}

//----------------------------------------------------------------------

// Simula la acción a esperar fuera de la barbería
void esperarFueraBarberia(int num_cliente)
{
  chrono::milliseconds duracion_espera( aleatorio<20,200>() );

  mtx.lock();
  cout << "\t\tCliente " << num_cliente
       << " espera fuera de la barbería (" << duracion_espera.count() << " milisegundos)..." << endl;
  mtx.unlock();

  this_thread::sleep_for(duracion_espera);
}

//----------------------------------------------------------------------

// Simula la acción de un barbero descansando
void descansarBarbero(int num_barbero)
{
  chrono::milliseconds duracion_descanso( aleatorio<20,200>() );

  mtx.lock();
  cout << "Barbero " << num_barbero << " ha pelado a " << MAX_PELADOS << " clientes"
       << " seguidos y necesita descansar (" << duracion_descanso.count()
       << " milisegundos)..." << endl;
  mtx.unlock();

  this_thread::sleep_for(duracion_descanso);

  mtx.lock();
  cout << "Barbero " << num_barbero << " vuelve a estar disponible." << endl;
  mtx.unlock();
}


//**********************************************************************
// Monitor para gestionar la barbería
//----------------------------------------------------------------------

class Barberia : public HoareMonitor {
  private:
    unsigned clientes_pelados[NB];
    int siguiente_barbero;    // Indica el barbero que va a pelar al siguiente cliente
    CondVar vacio;
    CondVar esperando;
    CondVar pelandose[NB];
  public:
    Barberia();
    void cortarPelo(int num_cliente);
    void siguienteCliente(int num_barbero);
    bool finCliente(int num_barbero);
};


//**********************************************************************
// Implementación de métodos del monitor
//----------------------------------------------------------------------

Barberia::Barberia()
{
  vacio = newCondVar();
  esperando = newCondVar();
  siguiente_barbero = -1;  // En realidad no importa el valor inicial

  for (int i = 0; i < NB; i++) {
    clientes_pelados[i] = 0;
    pelandose[i] = newCondVar();
  }
}

//----------------------------------------------------------------------

void Barberia::cortarPelo(int num_cliente)
{
  mtx.lock();
  cout << "\t\tCliente " << num_cliente << " entra en la barbería." << endl;
  mtx.unlock();

  // Si hay algún barbero dormido, lo despierta
  if (vacio.get_nwt() != 0) {
    vacio.signal();
  }
  // Si la barbería está llena, sale
  else if (esperando.get_nwt() == MAX_ESPERA) {
    mtx.lock();
    cout << "\t\tCliente " << num_cliente << " sale de la barbería porque está llena."
         << endl;
    mtx.unlock();

    return;  // ¡Barbería llena!

    /*
      Otra opción sería tener una segunda cola (sin limitación de tamaño)
      que simbolice la espera fuera de la barbería cuando está llena. De esta forma,
      se mantendría el orden también fuera de la barbería, y un cliente no intentaría
      entrar hasta que no fuera su turno de entrar a la barbería.
     */
  }
  // En otro caso, se queda esperando su turno
  else {
    esperando.wait();
  }

  mtx.lock();
  cout << "\t\tCliente " << num_cliente << " entra a pelarse con el barbero "
       << siguiente_barbero << "." << endl;
  mtx.unlock();

  pelandose[siguiente_barbero].wait();
}

//----------------------------------------------------------------------

void Barberia::siguienteCliente(int num_barbero)
{
  if (esperando.get_nwt() == 0) {
    mtx.lock();
    cout << "Barbero " << num_barbero << " se duerme porque no hay clientes ... ZZZZZZ ..."
         << endl;
    mtx.unlock();

    vacio.wait();

    mtx.lock();
    cout << "Barbero " << num_barbero << " se despierta." << endl;
    mtx.unlock();

    siguiente_barbero = num_barbero;
  }
  else {
    siguiente_barbero = num_barbero;
    esperando.signal();
  }
}

//----------------------------------------------------------------------

bool Barberia::finCliente(int num_barbero)
{
  bool descansar = false;

  clientes_pelados[num_barbero]++;
  pelandose[num_barbero].signal();

  if (clientes_pelados[num_barbero] == MAX_PELADOS) {
    clientes_pelados[num_barbero] = 0;
    descansar = true;
  }

  return descansar;
}


// *****************************************************************************
// Funciones de hebras
// -----------------------------------------------------------------------------

void funcion_hebra_barbero(MRef<Barberia> monitor, int i)
{
  bool descansar;

  while (true)
  {
    monitor->siguienteCliente(i);
    cortarPeloACliente(i);
    descansar = monitor->finCliente(i);

    if (descansar)
      descansarBarbero(i);
  }
}

// -----------------------------------------------------------------------------

void funcion_hebra_cliente(MRef<Barberia> monitor, int i)
{
  while (true)
  {
    monitor->cortarPelo(i);
    esperarFueraBarberia(i);
  }
}


// *****************************************************************************
// Programa principal
// -----------------------------------------------------------------------------

int main()
{
  int i;

  cout << endl << "--------------------------------------------------------" << endl
               << " Problema del barbero" << endl
               << "--------------------------------------------------------" << endl
               << endl << flush ;

  cout << "NÚMERO DE BARBEROS: " << NB << endl;
  cout << "NÚMERO DE CLIENTES: " << NC << endl;
  cout << "ASIENTOS EN LA SALA DE ESPERA: " << MAX_ESPERA << endl;
  cout << "MÁXIMO DE PELADOS SEGUIDOS: " << MAX_PELADOS << endl << endl << flush;

  auto monitor = Create<Barberia>();

  thread hebra_barbero[NB],
         hebra_cliente[NC];

  for (i = 0; i < NB; i++)
    hebra_barbero[i] = thread(funcion_hebra_barbero, monitor, i);
  for (i = 0; i < NC; i++)
    hebra_cliente[i] = thread(funcion_hebra_cliente, monitor, i);

  // Hay que esperar al menos a una hebra
  hebra_barbero[0].join();
}
