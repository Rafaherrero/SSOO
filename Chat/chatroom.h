/*
 * chatroom.h - Sala de chat multiusuario basada en memoria compartida
 *
 * Rafael Herrero Alvarez
 *
 */

//#ifndef CHATROOM_H
//#define CHATROOM_H

#include <string>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

using namespace std;

class ChatRoom
{
public:

    ChatRoom();
    ~ChatRoom();

    // Conectar a la sala de chat indicada
    int connectTo(const std::string& chatRoomId,const std::string& userchat);

    // Ejecutar el chat
    void run();

private:
    struct SharedMessage;

    // Buffer en memoria compartida para el intercambio de mensajes
    SharedMessage* sharedMessage_;
    // Número de secuencia del último mensaje leido con receive()
    unsigned messageReceiveCounter_;

    // Indicador de si el objeto es el propietario del objeto de memoria
    // compartida. El propietario es el responsable de su destrucción
    bool isSharedMemoryObjectOwner_;

    // Leer mensajes desde la entrada estándar y enviarlos a la sala de chat
    void runSender();
    // Recibir mensajes de la sala de chat y mostrarlos por la salida estándar
    void runReceiver();

    // Enviar um mensaje a la sala de chat
    void send(const std::string& message, unsigned tipomens);
    // Recibir un mensaje de la sala de chat
    void receive(std::string& message);

    // Ejecutar el comando indicado y enviar su salida estándar
    void execAndSend(const std::string& command);

    //Variable booleana que nos permite terminar nuestro send
    bool finbucle = false;

    //Variables que almacenan el nombre de usuario y de la sala
    string sala;
    string usuario;

    //Variables que nos indica que usuario unico es y el color que tiene asociado
    unsigned numusuar;
    unsigned colorusuar;

};

//#endif // CHATROOM_H
