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
#include <stdbool.h>
#include <time.h>
#include <signal.h>

// constantes :
#define VERSION "1.04"

int debug = 0;
char* logfile = "";
int port = 80; //TCP seulement
int alarmtime = 5; //secondes
int maxline = 666; //longueur max d'une ligne
char* savedir = "";
int apache = 0; // option --apache

// Fonction qui renvoie un booléen selon si le string commence par le prefix.
//(utilisée ici pour verifier les entrées utilisateur)
_Bool starts_with(const char *restrict string, const char *restrict prefix) {
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
  bool mustLog = false;
  char str_affiche[BUFSIZ];
  if(strlen(logfile) > 0){
    file = fopen(logfile, "rb+");
    if(file == NULL){
        file = fopen(logfile, "wb");
    }
    //Ajoute la sortie standard dans le fichier de log.
    mustLog = true;
    file = fopen(logfile, "a+");
    //file = freopen(logfile, "a+", stdout);

    if(mustLog){
      sprintf(str_affiche, "# Now listening on port %d\n",port);
      fputs(str_affiche, file);
    }
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

  struct timeval tv;
  tv.tv_sec = alarmtime;
  tv.tv_usec = 0;

  if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv)){
    perror("setsockopt(SO_RCVTIMEO) failed");
  }

  if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv)){
    perror("setsockopt(SO_SNDTIMEO) failed");
  }

  retour = bind (sock,(struct sockaddr *)&adresse,sizeof(adresse));

  if (retour<0) {
    perror ("ERROR: cannot listen on port !\n");
    return(-1);
  }

  if(mustLog){
    sprintf(str_affiche, "websnarf v%s listening on port %d (timeout=%d secs)\n"
      ,VERSION, port, alarmtime);
    fputs(str_affiche, file);
  }
  printf("websnarf v%s listening on port %d (timeout=%d secs)\n",VERSION,port,alarmtime);
  fflush(stdout);

  time_t timestamp;
  struct tm * pTime;

  char our_ip [BUFSIZ];
  char their_ip [BUFSIZ];
  char time_request [BUFSIZ];

  char request [BUFSIZ];

  struct sockaddr_in client_addr;
  socklen_t clen = sizeof(client_addr);
  struct sockaddr_in server_addr;
  socklen_t slen = sizeof(server_addr);

  while(1){
    listen (sock,5);

    newsockfd = accept(sock, (struct sockaddr *)&client_addr, &clen);

    if(newsockfd != -1){
      getsockname(newsockfd, (struct sockaddr *)&server_addr, &slen);
      getpeername(newsockfd, (struct sockaddr *)&client_addr, &clen);

      // Calcul du string de notre IP
      sprintf(our_ip, "%s", inet_ntoa(server_addr.sin_addr));
      // Calcul du string de l'IP distante
      sprintf(their_ip, "%s", inet_ntoa(client_addr.sin_addr));


      if(debug){
        sprintf(str_affiche, "--> accepted connection from %s\n",their_ip);
        if(mustLog){
          fputs(str_affiche, file);
        }
        printf("%s",str_affiche);
        fflush(stdout);
      }

      if ( fork() == 0 ) {
        close ( sock );

        time_t start, end;
        double elapsed;
        time(&start);
        // récupération de la requête effectuée par le "client"
        do {
          time(&end);
          elapsed = difftime(end, start);
          s = read(newsockfd, request, maxline);

          if(s >= 1){
            if(debug){
              if(mustLog){
                sprintf(str_affiche, "  client ready to read, now reading\n");
                fputs(str_affiche, file);
              }
              printf("  client ready to read, now reading\n");
              fflush(stdout);
            }

            request[s] = 0;

            if(debug){
              if(mustLog){
                sprintf(str_affiche, "  got read #%d of [%ld]\n", 1,strlen(request) );
                fputs(str_affiche, file);
                sprintf(str_affiche, "[%s]\n",request);
                fputs(str_affiche, file);
              }
              printf("  got read #%d of [%ld]\n", 1,strlen(request));
              printf("[%s]\n",request);
              fflush(stdout);
            }

            char* req = strtok(request, "\n"); // ne recupere que l'url de la requete

            // Récupération de l'heure de la requête
            time(&timestamp);
            pTime = localtime( & timestamp);
            strftime(time_request, BUFSIZ, "%m/%d %H:%M:%S", pTime );

            sprintf(str_affiche, "%s %s \t-> %s \t: %s\n", time_request, our_ip, their_ip, req);
            if(mustLog){
              // ajout du message dans le fichier
              fputs(str_affiche, file);
            }
            // affichage du message dans le stdout
            printf("%s",str_affiche);
            fflush(stdout);
          }
        } while(s > 1 || elapsed < alarmtime);

        // On a fini de lire la socket
        close (newsockfd);
        exit(1);

        //fclose(file); //TODO a enlever plus tard
        //exit (1); //TODO a enlever plus tard
      }
      close(newsockfd);
    }
  }
  close(sock);
  return 0;
}
