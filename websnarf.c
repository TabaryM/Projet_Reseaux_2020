// COMMAND LINE
// ------------
//
// --port=##	Listen on port ## instead of 80. This is mainly only
//		good for developers making sure we've not broken something
//		while testing on a "live" system.
//
// --timeout=##	Wait for at most ## seconds for input from the remote end.
//		Longer timeouts will capture a bit more traffic over slow
//		links, but it will hold up the rest of the program.
//
// --log=FILE	Append to the given file. The info saved there is also
//		*always* written to the standard output, but this insures
//		that we have a record even if the program is restarted.
//
// --max=##	max length of captured request is ## characters. Most URL
//		fetches are small ("GET / HTTP/1.0") but the Code Red
//		ones are quite large. We don't care to record much more
//		than one line's worth.
//
// --save=DIR	save /all/ headers into separate files in DIR. This is not
//		a very good mechanism (but was easy to implement). It fills
//		up a directory quickly: don't use --save=.  !
//
// --apache      put logs in Apache format (should be the default)
//
// --version     show current version number

#include <stdio.h>      // Fichier contenant les en-têtes des fonctions standard d'entrées/sorties
#include <stdlib.h>     // Fichier contenant les en-têtes de fonctions standard telles que malloc()
#include <string.h>     // Fichier contenant les en-têtes de fonctions standard de gestion de chaînes de caractères
#include <unistd.h>     // Fichier d'en-têtes de fonctions de la norme POSIX (dont gestion des fichiers : write(), close(), ...)
#include <sys/types.h>      // Fichier d'en-têtes contenant la définition de plusieurs types et de structures primitifs (système)
#include <sys/socket.h>     // Fichier d'en-têtes des fonctions de gestion de sockets
#include <netinet/in.h>     // Fichier contenant différentes macros et constantes facilitant l'utilisation du protocole IP
#include <netdb.h>      // Fichier d'en-têtes contenant la définition de fonctions et de structures permettant d'obtenir des informations sur le réseau (gethostbyname(), struct hostent, ...)
#include <memory.h>     // Contient l'inclusion de string.h (s'il n'est pas déjà inclus) et de features.h
//include <errno.h>      // Fichier d'en-têtes pour la gestion des erreurs (notamment perror())
#include <arpa/inet.h>

// constantes :
#define VERSION "1.04"

int debug = 0;
char* logfile = "";
int port = 80; //TCP seulement
int alarmtime = 5; //secondes
int maxline = 40; //longueur max d'une ligne
char* savedir = "";
int apache = 0; // option --apache

// Fonction qui renvoie un booléen selon si le string commence par le prefix.
//(utilisée ici pour verifier les entrées utilisateur)
_Bool starts_with(const char *restrict string, const char *restrict prefix)
{
    while(*prefix)
    {
        if(*prefix++ != *string++)
            return 0;
    }

    return 1;
}

int main (int argc, char *argv[]) {

  if(argc > 1){ // cas ou il y'a des paramètres saisis.
    for(int i=1; i<argc; i++){
      if( starts_with(argv[i],"--help") ){ // --help
        printf("usage: %s [options]\n\n\t--timeout=<n>   wait at most <n> seconds on a read (default $alarmtime)\n\t--log=FILE      append output to FILE (default stdout only)\n\t--port=<n>      listen on TCP port <n> (default $port/tcp)\n\t--max=<n>       save at most <n> chars of request (default $maxline chars)\n\t--save=DIR      save all incoming headers into DIR\n\t--debug         turn on a bit of debugging (mainly for developers)\n\t--apache        logs are in Apache style\n\t--version       show version info\n\n\t--help          show this listing\n",__FILE__);
        exit(0);
      }
      else if( starts_with(argv[i],"--log=")  ){ // --log=FILE
          logfile = strtok(argv[i],"--log=");
      }
      else if( starts_with(argv[i],"--port=")  ){ // --port=##
          port = atoi(strtok(argv[i],"--port="));
      }
      else if( starts_with(argv[i],"--timeout=")  ){ // --timeout=##
          alarmtime = atoi(strtok(argv[i],"--timeout="));
      }
      else if( starts_with(argv[i],"--max=")  ){ // --max=##
          maxline = atoi(strtok(argv[i],"--max="));
      }
      else if( starts_with(argv[i],"--save=")  ){ // --save=DIR
          savedir = strtok(argv[i],"--save=");
      }
      else if( starts_with(argv[i],"--debug")  ){ // --debug
          debug = 1;
      }
      else if( starts_with(argv[i],"--apache")  ){ // --apache
          apache = 1;
      }
      else if( starts_with(argv[i],"--version")  ){ // --version
          printf("websnarf v%s -- http://www.unixwiz.net/tools/websnarf.html\n",VERSION);
          exit(0);
      }
      else{
        printf("Erreur : tapez --help pour afficher l'aide.\n");
        exit(-1);
      }
    }
  }

  // -----------------------------------------------------------------------
  // CREATE LOGFILE
  //
  // ... but only if requested with --log=FILE
  //
  FILE* file;
  if(strlen(logfile) > 0){
    file = fopen(logfile, "rb+");
    if(file == NULL){
        file = fopen(logfile, "wb");
    }
    //Ajoute la sortie standard dans le fichier de log.
    file = freopen(logfile, "a+", stdout);

    printf("# Now listening on port %d\n",port);
    fflush(stdout);
  }


  // -----------------------------------------------------------------------
  // Create the socket to listen on. It's a fatal error if we cannot listen
  // on port 80, the most common reasons being (a) we're not root or
  // (b) something else is already listening on that. Is Apache running?
  //

  int newsockfd, sock, retour, s;
  struct sockaddr_in adresse;

  sock = socket(AF_INET,SOCK_STREAM,0);

  if (sock<0) {
    perror ("ERREUR OUVERTURE");
    exit(-1);
  }

  adresse.sin_family = AF_INET;

  adresse.sin_port = htons(port);

  adresse.sin_addr.s_addr=INADDR_ANY;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
    perror("setsockopt(SO_REUSEADDR) failed");
  }

  retour = bind (sock,(struct sockaddr *)&adresse,sizeof(adresse));

  if (retour<0) {
    perror ("ERROR: cannot listen on port !\n");
    return(-1);
  }

  printf("websnarf v%s listening on port %d (timeout=%d secs)\n",VERSION,port,alarmtime);
  fflush(stdout);

  char msg [BUFSIZ];
  struct sockaddr_in client_addr;
  socklen_t clen = sizeof(client_addr);
  struct sockaddr_in server_addr;
  socklen_t slen = sizeof(server_addr);
  while(1){

    listen (sock,5);

    newsockfd = accept (sock, (struct sockaddr *)&client_addr, &clen);

    getsockname(newsockfd, (struct sockaddr *)&server_addr, &slen);
    getpeername(newsockfd, (struct sockaddr *)&client_addr, &clen);
    // TODO verifier que c'est correct
    printf("ip serveur : %s\n", inet_ntoa(server_addr.sin_addr));
    printf("ip client : %s\n", inet_ntoa(client_addr.sin_addr));
    fflush(stdout);
    // c'est l'equivalent de la ligne 186 du .pl

    //----------------------------------------------------------------
    // We immediately need to note who the local and remote IP addresses
    // are, because once we start the read we may find that our socket
    // gets closed by a timeout. Save them now for easier reference.
    //

/*
    if ( fork() == 0 ) {
      close ( sock );
      // On lit le message envoy? par la socket de communication.
    //  msg contiendra la chaine de caract?res envoy?e par le r?seau,
      // s le code d'erreur de la fonction. -1 si pb et sinon c'est le nombre de caract?res lus
      s = read(newsockfd, msg, 1024);
      if (s == -1)
        perror("Problemes");
      else {
        // Si le code d'erreur est bon, on affiche le message.
        msg[s] = 0;
        printf("Msg: %s\n", msg);
        printf("Recept reussie, emission msg: ");

        // On demande ? l'utilisateur de rentrer un message qui va ?tre exp?di? sur le r?seau
        scanf(" %[^\n]", msg);

        // On va ?crire sur la socket, en testant le code d'erreur de la fonction write.
        s = write(newsockfd, msg, strlen(msg));
        if (s == -1) {
          perror("Erreur write");
          return(-1);
        }
        else
          printf("Ecriture reussie, msg: %s\n", msg);
      }
*/

      close ( newsockfd );
      fclose(file); //TODO a enlever plus tard
      exit (1); //TODO a enlever plus tard
  }

  return 0;
}
