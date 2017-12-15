// -----------------------------------------------------------------------------
//
// Sistemas concurrentes y Distribuidos.
// Examen práctica 2. 17 de Noviembre de 2017.
// Antonio Coín Castro. 77191012E.
//
// Archivo: barberia2.cpp
//
// Monitor en C++11 con semántica SU, para el problema
// del barbero. Se realizan las modificaciones necesarias para
// ajustarse al enunciado del examen.
//
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

constexpr unsigned NC = 3;  // Número de clientes

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
void cortarPeloACliente()
{
  chrono::milliseconds duracion_corte( aleatorio<20,200>() );
  mtx.lock();
  cout << "Barbero pelando a un cliente..." << endl;
  mtx.unlock();

  this_thread::sleep_for(duracion_corte);

  mtx.lock();
  cout << "Barbero termina de pelar al cliente." << endl;
  mtx.unlock();
}

//----------------------------------------------------------------------

// Simula la acción a esperar fuera de la barbería
void esperarFueraBarberia(int num_cliente)
{
  chrono::milliseconds duracion_espera( aleatorio<20,200>() );

  mtx.lock();
  cout << "\t\tCliente " << num_cliente
       << " espera fuera de la barbería (" << duracion_espera.count() << " milisegundos)..."
       << endl;
  mtx.unlock();

  this_thread::sleep_for(duracion_espera);
}


//**********************************************************************
// Monitor para gestionar la barbería
//----------------------------------------------------------------------

class Barberia : public HoareMonitor {
  private:
    CondVar vacio;
    CondVar esperando;
    CondVar pelandose;
  public:
    Barberia();
    void cortarPelo(int num_cliente);
    void siguienteCliente();
    void finCliente();
};


//**********************************************************************
// Implementación de métodos del monitor
//----------------------------------------------------------------------

Barberia::Barberia()
{
  vacio     = newCondVar();
  esperando = newCondVar();
  pelandose = newCondVar();
}

//----------------------------------------------------------------------

void Barberia::cortarPelo(int num_cliente)
{
  mtx.lock();
  cout << "\t\tCliente " << num_cliente << " entra en la barbería." << endl;
  mtx.unlock();

  // Si el barbero está dormido y al menos hay un cliente esperando, despierta al barbero
  if (vacio.get_nwt() > 0 && esperando.get_nwt() > 0) {
    vacio.signal();
  }

  // En cualquier caso, entra en la sala de espera
  esperando.wait();

  mtx.lock();
  cout << "\t\tCliente " << num_cliente << " comienza a pelarse." << endl;
  mtx.unlock();

  pelandose.wait();
}

//----------------------------------------------------------------------

void Barberia::siguienteCliente()
{
  // Si no hay clientes esperando, se duerme
  if (esperando.get_nwt() == 0) {
    mtx.lock();
    cout << "Barbero se duerme porque no hay clientes ... ZZZZZZ ..."
         << endl;
    mtx.unlock();

    vacio.wait();

    mtx.lock();
    cout << "Barbero se despierta." << endl;
    mtx.unlock();
  }

  // Avisamos al siguiente cliente para ser pelado
  esperando.signal();

}

//----------------------------------------------------------------------

void Barberia::finCliente()
{
  pelandose.signal();
}


// *****************************************************************************
// Funciones de hebras
// -----------------------------------------------------------------------------

void funcion_hebra_barbero(MRef<Barberia> monitor)
{
  while (true)
  {
    monitor->siguienteCliente();
    cortarPeloACliente();
    monitor->finCliente();
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
  cout << endl << "--------------------------------------------------------" << endl
               << " Problema del barbero" << endl
               << "--------------------------------------------------------" << endl
               << endl << flush ;

  cout << "NÚMERO DE CLIENTES: " << NC << endl << endl << flush;

  auto monitor = Create<Barberia>();

  thread hebra_barbero(funcion_hebra_barbero, monitor),
         hebra_cliente[NC];

  for (int i = 0; i < NC; i++)
    hebra_cliente[i] = thread(funcion_hebra_cliente, monitor, i);

  // Hay que esperar al menos a una hebra
  hebra_barbero.join();
}
