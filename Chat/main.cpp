/*
 * main.cpp - Chat multiusuario basado en memoria compartida
 *
 * Rafael Herrero Alvarez
 *
 */

#include "chatroom.h"
using namespace std;

int main(int argc, char *argv[])
{

    string chatRoomId;
    string userchat;
    bool ejecutoayuda=false;

/* En el main, miramos si nos han pasado algun argumento al ejecutar el programa.
 * En el caso de que no, el predeterminado, utilizamos como nombre de usuario y de sala, el nombre de usuario del sistema.
 * Tambien puede ser que nos den solo el usuario y utilizamos el predeterminado para la sala y viceversa.
 * O tambien que pasen ambos argumentos en el orden que quieran.
 * En cualquier otro caso, o caso de error, se ejecuta la ayuda.
 */

    if (argc == 1){
    chatRoomId=getenv("USER");
    userchat=getenv("USER");
    }

    else if (argc == 2){
        ejecutoayuda=true;
        }
    else if (argc == 3){
        if ((strcmp (argv[1], "-u")==0)||strcmp (argv[1], "--user")==0){
            userchat=argv[2];
            chatRoomId=getenv("USER");
        }
        else if ((strcmp (argv[1], "-r")==0)||strcmp (argv[1], "--room")==0){
            userchat=getenv("USER");
            chatRoomId=argv[2];
        }
        else{
            ejecutoayuda=true;
        }
    }
    else if (argc == 5){
        if (((strcmp (argv[1], "-u")==0)||strcmp (argv[1], "--user")==0)&&((strcmp (argv[3], "-r")==0)||(strcmp (argv[3], "--room")==0))){
            userchat=argv[2];
            chatRoomId=argv[4];
        }
        else if (((strcmp (argv[3], "-u")==0)||strcmp (argv[3], "--user")==0)&&((strcmp (argv[1], "-r")==0)||(strcmp (argv[1], "--room")==0))){
            userchat=argv[4];
            chatRoomId=argv[2];
        }
        else{
            ejecutoayuda=true;

    }
    }
    else{
        ejecutoayuda=true;
    }

    if (ejecutoayuda){
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
        return 0;

    }

/* Tras leer los argumentos, creamos nuestro chatRoom, y ejecutamos la funcion connectTo que realizara todo el mapeo de memoria
 * y nos avisara en caso de algun fallo. Si nuestro programa fallase, saldria por pantalla dicho error, y ademas no se ejecutaria
 * nuestro. En caso de que no de error, el programa continuara perfectamente.
 */

    ChatRoom chatRoom;

    int result = chatRoom.connectTo(chatRoomId,userchat);

    if (result == -1){
        cout << "ftruncate: error realizando la definición del tamaño de memoria (truncando)." << endl << "Error: " << strerror(errno) << endl;
    }
    else if (result == -2){
        cout << "mmap: error realizando el mapeo de memoria (propietario)." << endl << "Error: " << strerror(errno) << endl;
    }
    else if (result == -3){
        cout << "shm_open: error conectando a la memoria (invitado)." << endl << "Error: " << strerror(errno) << endl;
    }
    else if (result == -4){
        cout << "mmap: error realizando el mapeo de memoria (invitado)." << endl << "Error: " << strerror(errno) << endl;
    }
    else if (result == -5){
        cout << "shm_open: error conectando a la memoria (propietario)." << endl << "Error: " << strerror(errno) << endl;
    }
    else{
        chatRoom.run();
    }

    return 0;
}

