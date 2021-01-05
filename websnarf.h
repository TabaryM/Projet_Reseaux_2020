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


int creersock( u_short port) {

  // On cr?e deux variables enti?res
  int sock, retour;
  // On cr?e une variable adresse selon la structure sockaddr_in (la structure est d?crite dans sys/socket.h)
  struct sockaddr_in adresse;

  /*
  La ligne suivante d?crit la cr?ation de la socket en tant que telle.
  La fonction socket prend 3 param?tres :
  - la famille du socket : la plupart du temps, les d?veloppeurs utilisent AF_INET pour l'Internet (TCP/IP, adresses IP sur 4 octets)
    Il existe aussi la famille AF_UNIX, dans ce mode, on ne passe pas des num?ros de port mais des noms de fichiers.
  - le protocole de niveau 4 (couche transport) utilis? : SOCK_STREAM pour TCP, SOCK_DGRAM pour UDP, ou enfin SOCK_RAW pour g?n?rer
    des trames directement g?r?es par les couches inf?rieures.
  - un num?ro d?signant le protocole qui fournit le service d?sir?. Dans le cas de socket TCP/IP, on place toujours ce param?tre a 0 si on utilise le protocole par d?faut.
  */


  sock = socket(AF_INET, SOCK_STREAM,0);

  // Si le code retourn? n'est pas un identifiant valide (la cr?ation s'est mal pass?e), on affiche un message sur la sortie d'erreur, et on renvoie -1
  if (sock<0) {
    perror ("ERREUR OUVERTURE");
    return(-1);
  }

  // On compl?te les champs de la structure sockaddr_in :
  // La famille du socket, AF_INET, comme cit? pr?c?dement
  adresse.sin_family = AF_INET;

  /* Le port auquel va se lier la socket afin d'attendre les connexions clientes. La fonction htonl()
  convertit  un  entier  long  "htons" signifie "host to network long" conversion depuis  l'ordre des bits de l'h?te vers celui du r?seau.
  */
  adresse.sin_port = htons(port);

  /* Ce champ d?signe l'adresse locale auquel un client pourra se connecter. Dans le cas d'une socket utilis?e
  par un serveur, ce champ est initialis? avec la valeur INADDR_ANY. La constante INADDR_ANY utilis?e comme
  adresse pour le serveur a pour valeur 0.0.0.0 ce qui correspond ? une ?coute sur toutes les interfaces locales disponibles.

  */
  adresse.sin_addr.s_addr = INADDR_ANY;

  int optval = 1;

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof optval) < 0){
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

  /*
  bind est utilis? pour lier la socket : on va attacher la socket cr?e au d?but avec les informations rentr?es dans
  la structure sockaddr_in (donc une adresse et un num?ro de port).
   Ce bind affecte une identit? ? la socket, la socket repr?sent?e par le descripteur pass? en premier argument est associ?e
    ? l'adresse pass?e en seconde position. Le dernier argument repr?sente la longueur de l'adresse.
    Ce qui a pour but de  rendre la socket accessible de l'ext?rieur (par getsockbyaddr)
  */
  retour = bind (sock,(struct sockaddr *)&adresse,sizeof(adresse));

  // En cas d'erreur lors du bind, on affiche un message d'erreur et on renvoie -1
  if (retour<0) {
    perror ("IMPOSSIBLE DE NOMMER LA SOCKET");
    return(-1);
  }

  // Au final, on renvoie sock, qui contient l'identifiant ? la socket cr?e et attach?e.
  return (sock);
}

void print_or_log(char* message, bool mustLog, FILE* file){
  if(mustLog) fputs(message, file);
  printf("%s", message);
}
