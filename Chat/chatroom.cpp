
/*
 * chatroom.cpp - Sala de chat multiusuario basada en memoria compartida
 *
 * Rafael Herrero Alvarez
 *
 */

#include <condition_variable>
#include <mutex>
#include <vector>
#include <thread>
#include <cstring>
#include <cstdlib>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fstream>
#include <iostream>
#include <vector>

#include "chatroom.h"

using namespace std;

/* En nuestra estructura compartida guardamos todo lo necesario para que lean y escriban todos los usuarios.
 * Tenemos nuestro mensaje, su tamaño, el usuario que lo mando y la cantidad de mensajes. Esos son los necesarios,
 * ademas del mutex y la condicion variable. Las demas variables las utilizo como ampliacion del chat. Tipomensaje
 * me permite saber si es un mensaje normal, de entrada o salida de un usuario del chat, emoticono de cara feliz o
 * triste, o si es un comando. El numero de usuario junto con el color de usuario me permite que se conecten varios
 * usuarios con el mismo nombre, y asignar hasta cuatro colores diferentes (en caso de que hayan mas se repiten).
 * Las variables de varios mensajes permiten que no se imprima de nuevo el nombre de usuario si este ha mandado mas
 * de un mensaje.
 */

struct ChatRoom::SharedMessage
{
    char mensajito[1048576];
    char usuariomensaje[1024];
    long int tamanomensaje=0;
    unsigned nummensaje=0;
    unsigned tipomensaje=0;
    unsigned numusuario;
    unsigned colorusuario=0;
//    vector<string> todosusuarios;
    bool vmensajes=false;
    bool variosmensajes=false;
    mutex letstop;
    condition_variable joselito;

    SharedMessage();
};

/* Inicializamos nuestra estructura.
 */

ChatRoom::SharedMessage::SharedMessage()
    :tamanomensaje(0),
    nummensaje(0),
    tipomensaje(0),
    colorusuario(0),
    vmensajes(false),
    variosmensajes(false)
{

}

/* Inicializamos variables y punteros de nuestra clase con el constructor.
*/

ChatRoom::ChatRoom()
    : sharedMessage_(nullptr),
      messageReceiveCounter_(0),      
      isSharedMemoryObjectOwner_(false),
      numusuar(0),
      colorusuar(0)
{

}

/* Eliminamos lo que hemos hecho en la memoria compartida, teniendo en cuenta que si nuestro puntero ya esta apuntando a NULL,
 * no ponemos eliminarlo. Tambien vemos que siempre debemos deshacer el mapeo de memoria, pero solo si somos el propietario, lo
 * destruimos.
 */

ChatRoom::~ChatRoom()
{
if (sharedMessage_ != nullptr){
    munmap(sharedMessage_,sizeof(sharedMessage_));
    sharedMessage_ = nullptr;
}
    if (isSharedMemoryObjectOwner_){
    shm_unlink(sala.c_str());
    }
}

/* La funcion connectTo nos crea nuestra zona de memoria segun el nombre de sala que hayamos pasado previamente en el main.
 * Asignamos ademas a las variables globales, el nombre de usuario y la sala.
 * Tenemos en cuenta que si somos el propietario, tenemos que darle tamaño con ftruncate y mapear la memoria, ademas de crear
 * nuestro puntero a dicha direccion de memoria. Ponemos la variable de si somos el propietario a verdadero.
 * En caso de ser un invitado, abrimos la sala y mapeamos, inicializando el puntero a dicha direccion de memoria. Ponemos
 * la variable de si somos propietarios a falso.
 */

int ChatRoom::connectTo(const std::string& chatRoomId, const std::string& userchat)
{
    void* mapeo;
    usuario=userchat;
    sala=chatRoomId;
    int salaexiste=shm_open(sala.c_str(),O_CREAT|O_EXCL|O_RDWR,0777);

    if(salaexiste != -1){
        if(ftruncate(salaexiste,sizeof(ChatRoom::SharedMessage))==-1)
           return -1;
         mapeo=mmap(NULL,sizeof(ChatRoom::SharedMessage),PROT_READ|PROT_WRITE,MAP_SHARED,salaexiste,0);
         if (mapeo == MAP_FAILED)
             return -2;
         cout << "Esta creando la sala: " << sala << endl;
         cout << "Su usuario es: " << usuario << endl;
         cout << "Escriba '--help'' en cualquier momento para mostrar la ayuda" << endl << endl;
         sharedMessage_ = (ChatRoom::SharedMessage*) mapeo;
         new(sharedMessage_) ChatRoom::SharedMessage;
         isSharedMemoryObjectOwner_ = true;
    }
    else if (errno==EEXIST){
        salaexiste=shm_open(sala.c_str(),O_RDWR,0777);
        if (salaexiste == -1)
            return -3;
        mapeo=mmap(NULL,sizeof(ChatRoom::SharedMessage),PROT_READ|PROT_WRITE,MAP_SHARED,salaexiste,0);
        if (mapeo == MAP_FAILED)
            return -4;
        cout << "Se esta conectando a la sala: " << sala << endl;
        cout << "Su usuario es: " << usuario << endl;
        cout << "Escriba '--help'' en cualquier momento para mostrar la ayuda" << endl << endl;
        sharedMessage_ = (ChatRoom::SharedMessage*) mapeo;
        isSharedMemoryObjectOwner_ = false;
    }
    else{
        return -5;
    }
    return 0;

}

/* Nuestro chat empezara si no se ha detectado ningun error previamente. Creamos dos hilos y cuando terminen, los unimos.
 * Uno envia es el encargado de enviar y el otro de recibir.
 */

void ChatRoom::run()
{
    if ( sharedMessage_ == nullptr ) return;

    thread envia (&ChatRoom::runSender,this);
    thread lee (&ChatRoom::runReceiver,this);

    lee.join();
    envia.join();

}

/* Cuando enviamos un mensaje, nos quedamos esperando infinitamente a un mensaje mediante un while. Antes de esto, inicializamos
 * el numero del usuario a uno mas que el usuario anterior, bloqueando el mutex, e igualmente con el numero de su color, que no
 * puede pasar de 3.
 * Tenemos en cuenta que si se ejecuta esta funcion, es porque ha entrado alguien, asi que de ser asi, ejecutamos el send con un
 * mensaje en blanco, y poniendo el tipo de mensaje a 1, que significa que alguien ha entrado. Si se termina el bucle, significa
 * que alguien ha salido, asi que llamamos nuevamente a send, pero ahora con el tipo de mensaje a 2. En nuestro bucle infinito
 * detectamos si lo que se ha escrito es ':quit', de manera que abandonamos el bucle y con ello el programa, enviando en tipo de
 * mensaje un 2, '--help' para mostrar la ayuda solo al usuario que lo pide, ':)' que imprime una cara feliz en grande con el tipo
 * de mensaje a 5, ':(' que imprime una cara triste en grande con tipo de mensaje a 6, '!...' con un comando en los puntos
 * suspensivos e imprime la ejecucion del comando que llama directamente a execandsend, o cualquier otra cosa, que se interpreta
 * como un mensaje normal y corriente de ese usuario.
 */

void ChatRoom::runSender()
{

    string msg;
    string ayuda = ("--help");
    string salida = (":quit");
    string comando = ("!");
    string carafeliz = (":)");
    string caratriste = (":(");
    bool finrun;
    bool noejecutarsend=false;

    sharedMessage_->letstop.lock();
    sharedMessage_->numusuario++;
    numusuar = sharedMessage_->numusuario;
    if (sharedMessage_->colorusuario>2){
        sharedMessage_->colorusuario=0;
        colorusuar=sharedMessage_->colorusuario;
    }
    else{
        sharedMessage_->colorusuario++;
        colorusuar=sharedMessage_->colorusuario;
    }
    sharedMessage_->letstop.unlock();

send (msg,1);

 while (!finrun){
    getline(cin,msg);
    noejecutarsend=false;
    if (strcmp (msg.c_str(),salida.c_str())==0){
        finrun=true;
        noejecutarsend=true;
    }
    else if (strncmp (msg.c_str(),comando.c_str(), 1)==0){
            execAndSend(msg);
            noejecutarsend=true;
        }
        else if(strcmp (msg.c_str(),carafeliz.c_str())==0){
        send (msg,5);
        noejecutarsend=true;
    }
        else if(strcmp (msg.c_str(),caratriste.c_str())==0){
        send (msg,6);
        noejecutarsend=true;
    }
        else if (strcmp (msg.c_str(),ayuda.c_str())==0){
        cout << "\nChat multiusuario basado en memoria compartida por Rafael Herrero Álvarez. Versión 7.0.3" << endl;
        cout << "-h | --help -> Muestra esta ayuda." << endl;
        cout << "-u | --user -> Permite definir el usuario del chat." << endl;
        cout << "-r | --room -> Permite definir la sala del chat." << endl;
        cout << "Si no especifica argumentos, se tomará su nombre de usuario.\n" << endl;
        cout << "Los mensajes tienen los siguientes colores: " << endl;
        cout << " - Blanco: mensaje del propio usuario." << endl;
        cout << " - Azul, verde, violeta o rojo: mensaje de otro usuario." << endl;
        cout << " - Marrón: comando ejecutado por otro usuario." << endl;
        cout << " - Amarillo: mensaje avisando de la entrada o salida de un usuario del chat.\n" << endl;
        noejecutarsend=true;
    }
        if (!noejecutarsend){
        send (msg,3);
        }
    }

finbucle=true;
send(msg,2);

}

/* En la funcion send bloqueamos el mutex, ya que vamos a escribir en la estructura. Lo primero que hacemos es establecer
 * que tipo de mensaje es el que se envia, despues analizamos si el mismo usuario ha mandado justo antes un mensaje normal
 * y ponemos en verdadero nuestra variable. Guardamos el mensaje, el usuario que lo envia, el tamaño del mensaje, la
 * cantidad de mensajes enviados, el numero de usuario y su color, y por ultimo mandamos un notify, que nos permite
 * avisar a las demas funciones que esten esperando un cambio y desbloqueamos el mutex.
 */

void ChatRoom::send(const std::string& message, unsigned tipomens)
{

    sharedMessage_->letstop.lock();

    if (tipomens==1)
        sharedMessage_-> tipomensaje=1;
    else if (tipomens==2)
        sharedMessage_-> tipomensaje=2;
    else if (tipomens==3)
        sharedMessage_-> tipomensaje=3;
    else if (tipomens==4)
        sharedMessage_-> tipomensaje=4;
    else if (tipomens==5)
        sharedMessage_-> tipomensaje=5;
    else if (tipomens==6)
        sharedMessage_-> tipomensaje=6;
    if (sharedMessage_->numusuario==numusuar&& sharedMessage_-> vmensajes==true){
        sharedMessage_->variosmensajes=true;
        sharedMessage_-> vmensajes=false;
    }
    else{
        sharedMessage_->variosmensajes=false;
    }

    if (sharedMessage_->tipomensaje==3||sharedMessage_->tipomensaje==5||sharedMessage_->tipomensaje==6||sharedMessage_->tipomensaje==7){
        sharedMessage_-> vmensajes=true;
    }

   strcpy (sharedMessage_-> mensajito, message.c_str());
   strcpy (sharedMessage_->usuariomensaje, usuario.c_str());
   sharedMessage_->tamanomensaje=message.size();
   sharedMessage_->nummensaje++;
   sharedMessage_->numusuario = numusuar;
   sharedMessage_->colorusuario=colorusuar;
   sharedMessage_->joselito.notify_all();
   sharedMessage_->letstop.unlock();

}

/* Ahora, nuestra funcion imprimira el mensaje que otro usuario haya mandado. Principalmente, analizamos si el mismo usuario ha
 * mandado el mensaje para no imprimirlo por pantalla, o si ha ejecutado un comando, o cara triste o feliz. En caso contrario,
 * analizamos que tipo de mensaje es, y que color tienen asignado ese usuario. Todo ello en un bucle infinito que para cuando el
 * usuario abandone la sala.
 */

void ChatRoom::runReceiver()
{

while(!finbucle){

    string mensajetemporal;

 receive(mensajetemporal);

if (numusuar==sharedMessage_->numusuario){

    if (sharedMessage_-> tipomensaje==4){
            //execAndSend(mensajetemporal);
            cout << "\n\e[1;34mHas ejecutado un comando.\e[0m\n";
            cout << "\n\e[1;34m" << sharedMessage_->mensajito << "\e[0m" << endl;
            }
    if (sharedMessage_-> tipomensaje==5){
            cout << "\n ----\n|    |     |\n|    |      |\n ----        |\n              |\n         ---- |\n              |\n ----        |\n|    |      |\n|    |     |\n ----\n\n";
            }
    if (sharedMessage_-> tipomensaje==6){
            cout << "\n ----\n|    |           |\n|    |          |\n ----          |\n              |\n         ---- |\n              |\n ----          |\n|    |          |\n|    |           |\n ----\e[0m\n\n";
            }


}
else{
        if (sharedMessage_-> tipomensaje==1){
        cout << "\n\e[1;33mEl usuario " << sharedMessage_->usuariomensaje << " ha entrado en la sala.\e[0m\n" << endl;
        }
        else if (sharedMessage_-> tipomensaje==2){
        cout << "\n\e[1;33mEl usuario " << sharedMessage_->usuariomensaje << " ha abandonado la sala.\e[0m\n" << endl;
        }

        else if (sharedMessage_-> tipomensaje==3){
            if (mensajetemporal==""){
            }
            else{
                if (sharedMessage_->variosmensajes){
                    if(sharedMessage_->colorusuario==0){
                    cout << "\e[0;31m" << mensajetemporal << "\e[0m" << endl;
                    }
                    if(sharedMessage_->colorusuario==1){
                    cout << "\e[0;32m" << mensajetemporal << "\e[0m" << endl;
                    }
                    if(sharedMessage_->colorusuario==2){
                    cout << "\e[0;36m" << mensajetemporal << "\e[0m" << endl;
                    }
                    if(sharedMessage_->colorusuario==3){
                    cout << "\e[0;35m" << mensajetemporal << "\e[0m" << endl;
                    }
                }
                else{
                if(sharedMessage_->colorusuario==0){
                cout << "\e[1;31m" << sharedMessage_->usuariomensaje << ": " << "\e[0;31m" << mensajetemporal << "\e[0m" << endl;
                }
                if(sharedMessage_->colorusuario==1){
                cout << "\e[1;32m" << sharedMessage_->usuariomensaje << ": " << "\e[0;32m" << mensajetemporal << "\e[0m" << endl;
                }
                if(sharedMessage_->colorusuario==2){
                cout << "\e[1;36m" << sharedMessage_->usuariomensaje << ": " << "\e[0;36m" << mensajetemporal << "\e[0m" << endl;
                }
                if(sharedMessage_->colorusuario==3){
                cout << "\e[1;35m" << sharedMessage_->usuariomensaje << ": " << "\e[0;35m" << mensajetemporal << "\e[0m" << endl;
                }
            }
            }
        }
        else if (sharedMessage_-> tipomensaje==4){
        //execAndSend(mensajetemporal);
        cout << "\n\e[0;33mEl usuario " << sharedMessage_->usuariomensaje << " ha ejecutado un comando.\e[0m\n";
        cout << "\n\e[0;33m" << sharedMessage_->mensajito << "\e[0m" << endl;
        }
        else if (sharedMessage_-> tipomensaje==5){
            if (sharedMessage_->variosmensajes){
                if(sharedMessage_->colorusuario==0){
                   cout << "\n\e[0;31m ----\n|    |     |\n|    |      |\n ----        |\n              |\n         ---- |\n              |\n ----        |\n|    |      |\n|    |     |\n ----\e[0m\n";
                }
                if(sharedMessage_->colorusuario==1){
                   cout << "\n\e[0;32m ----\n|    |     |\n|    |      |\n ----        |\n              |\n         ---- |\n              |\n ----        |\n|    |      |\n|    |     |\n ----\e[0m\n";
                }
                if(sharedMessage_->colorusuario==2){
                   cout << "\n\e[0;36m ----\n|    |     |\n|    |      |\n ----        |\n              |\n         ---- |\n              |\n ----        |\n|    |      |\n|    |     |\n ----\e[0m\n";
                }
                if(sharedMessage_->colorusuario==3){
                   cout << "\n\e[0;35m ----\n|    |     |\n|    |      |\n ----        |\n              |\n         ---- |\n              |\n ----        |\n|    |      |\n|    |     |\n ----\e[0m\n";
                }
            }
            else{
                if(sharedMessage_->colorusuario==0){
                    cout << "\e[1;31m" << sharedMessage_->usuariomensaje << ": ";
                    cout << "\n\e[0;31m ----\n|    |     |\n|    |      |\n ----        |\n              |\n         ---- |\n              |\n ----        |\n|    |      |\n|    |     |\n ----\e[0m\n";
                }
                if(sharedMessage_->colorusuario==1){
                    cout << "\e[1;32m" << sharedMessage_->usuariomensaje << ": ";
                    cout << "\n\e[0;32m ----\n|    |     |\n|    |      |\n ----        |\n              |\n         ---- |\n              |\n ----        |\n|    |      |\n|    |     |\n ----\e[0m\n";
                }
                if(sharedMessage_->colorusuario==2){
                    cout << "\e[1;36m" << sharedMessage_->usuariomensaje << ": ";
                    cout << "\n\e[0;36m ----\n|    |     |\n|    |      |\n ----        |\n              |\n         ---- |\n              |\n ----        |\n|    |      |\n|    |     |\n ----\e[0m\n";
                }
                if(sharedMessage_->colorusuario==3){
                    cout << "\e[1;32m" << sharedMessage_->usuariomensaje << ": ";
                    cout << "\n\e[0;32m ----\n|    |     |\n|    |      |\n ----        |\n              |\n         ---- |\n              |\n ----        |\n|    |      |\n|    |     |\n ----\e[0m\n";
                }
            }
       }
        else if (sharedMessage_-> tipomensaje==6){
            if (sharedMessage_->variosmensajes){
                if(sharedMessage_->colorusuario==0){
                    cout << "\n\e[0;31m ----\n|    |           |\n|    |          |\n ----          |\n              |\n         ---- |\n              |\n ----          |\n|    |          |\n|    |           |\n ----\e[0m\n\n";
                }
                if(sharedMessage_->colorusuario==1){
                    cout << "\n\e[0;32m ----\n|    |           |\n|    |          |\n ----          |\n              |\n         ---- |\n              |\n ----          |\n|    |          |\n|    |           |\n ----\e[0m\n\n";
                }
                if(sharedMessage_->colorusuario==2){
                    cout << "\n\e[0;36m ----\n|    |           |\n|    |          |\n ----          |\n              |\n         ---- |\n              |\n ----          |\n|    |          |\n|    |           |\n ----\e[0m\n\n";
                }
                if(sharedMessage_->colorusuario==3){
                    cout << "\n\e[0;35m ----\n|    |           |\n|    |          |\n ----          |\n              |\n         ---- |\n              |\n ----          |\n|    |          |\n|    |           |\n ----\e[0m\n\n";
                }
            }
            else{
                if(sharedMessage_->colorusuario==0){
                    cout << "\e[1;31m" << sharedMessage_->usuariomensaje << ": ";
                    cout << "\n\e[0;31m ----\n|    |           |\n|    |          |\n ----          |\n              |\n         ---- |\n              |\n ----          |\n|    |          |\n|    |           |\n ----\e[0m\n\n";
                }
                if(sharedMessage_->colorusuario==1){
                    cout << "\e[1;32m" << sharedMessage_->usuariomensaje << ": ";
                    cout << "\n\e[0;32m ----\n|    |           |\n|    |          |\n ----          |\n              |\n         ---- |\n              |\n ----          |\n|    |          |\n|    |           |\n ----\e[0m\n\n";
                }
                if(sharedMessage_->colorusuario==2){
                    cout << "\e[1;36m" << sharedMessage_->usuariomensaje << ": ";
                    cout << "\n\e[0;36m ----\n|    |           |\n|    |          |\n ----          |\n              |\n         ---- |\n              |\n ----          |\n|    |          |\n|    |           |\n ----\e[0m\n\n";
                }
                if(sharedMessage_->colorusuario==3){
                    cout << "\e[1;35m" << sharedMessage_->usuariomensaje << ": ";
                    cout << "\n\e[0;35m ----\n|    |           |\n|    |          |\n ----          |\n              |\n         ---- |\n              |\n ----          |\n|    |          |\n|    |           |\n ----\e[0m\n\n";
                }
            }
        }
}
}
}

/* En la funcion receive, no hacemos nada hasta que el contador de mensaje cambie. En caso afirmativo, nos encargamos de copiar
 * en nuestra variable global del mensaje el que esta ubicado en la memoria compartida, asi como aumentar el numero de mensajes
 * recibidos.
 */

void ChatRoom::receive(std::string& message)
{
  std::unique_lock<std::mutex> lck(sharedMessage_->letstop);
    while (sharedMessage_->nummensaje==messageReceiveCounter_){
        sharedMessage_->joselito.wait(lck);
    }

    message=sharedMessage_->mensajito;
    messageReceiveCounter_= sharedMessage_->nummensaje;
    sharedMessage_->joselito.notify_all();

}

/* Con esta funcion podremos ejecutar comandos como si de una terminal se tratase. Analizamos el comando recibido y mandamos a dev/null
 * los errores que puedan ocurrir, principalmente, que no exista dicho comando. Utilizando popen ejecutamos el comando que queramos,
 * gracias a las tuberias. Mandamos la salida a un string que posteriormente mandaremos a send. Si nuestro string esta en blanco,
 * significa que el comando no existe, por lo que avisamos al usuario, pero no mandamos nada al resto.
 */

void ChatRoom::execAndSend(const std::string& command)
{

    int comandonoexiste;
    string comandito(command);
    string salidaerror(command.begin()+1,command.end());
    comandito += " 2> /dev/null";
    char* comandoejecutar = new char [comandito.length()];
    strcpy(comandoejecutar, comandito.c_str()+1);

    string result;
    FILE* archivocomando=NULL;

    archivocomando = popen(comandoejecutar, "r");

    if (archivocomando){
        char buffer[1024];
        while(!feof(archivocomando)) {
            if(fgets(buffer, 1024, archivocomando) != NULL) {
                result += buffer;
            }
        }
    comandonoexiste = pclose(archivocomando);
    if (comandonoexiste==0)
    send(result,4);
    if (result==""){
        cout << "No existe el comando " << salidaerror << endl;
    }
    }

}

/* Intento de utilizar un string para almacenar el nombre de todos los usuarios conectados. Problemas al leer el vector si no se es
 * el propietario. Posible solucion, utilizar la salida y lectura de un fichero temporal.
 */

//string listausuarios =("--usuarios");
//string eliminarusu = ("estenombredeusuarionuncasevaadar");

//string strusu = ("estenombredeusuarionuncasevaadar");

//if (isSharedMemoryObjectOwner_)
//    sharedMessage_->todosusuarios.push_back(usuario);

//sharedMessage_->letstop.lock();
//if (isSharedMemoryObjectOwner_){
//    sharedMessage_->todosusuarios.push_back(sharedMessage_->usuariomensaje);
//}
//sharedMessage->letstop.unlock();

//sharedMessage_->letstop.lock();
//if (isSharedMemoryObjectOwner_){
//    sharedMessage_->todosusuarios.erase(sharedMessage_->todosusuarios.begin()+sharedMessage_->numusuario-1);
//    sharedMessage_->todosusuarios.insert(sharedMessage_->todosusuarios.begin()+sharedMessage_->numusuario-1,strusu);
//}
//sharedMessage_->letstop.unlock();

//else if(strcmp(msg.c_str(),listausuarios.c_str())==0){

//for (int i=0; i<sharedMessage_->todosusuarios.size();i++){
//    if (strcmp(sharedMessage_->todosusuarios[i].c_str(),eliminarusu.c_str())==0){
//    }
//    else{
//    cout << sharedMessage_->todosusuarios[i]<< endl;
//    }
//}
//noejecutarsend=true;
//}
