/* COMMAND LINE
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
// --apache      put logs in Apache format (should be the default)
//
// --version     show current version number
*/

#include "websnarf.h"

int main (int argc, char *argv[]) {
  for(int i = 0; i < argc; i++){
    printf("param num %d : %s\t", i, argv[i]);
  }
  printf("\n");

  int debug = 0;
  char* logfile = "";
  int port = 80; //TCP seulement
  int alarmtime = 5; //secondes
  int maxline = 666; //longueur max d'une ligne
  int apache = 0; // option --apache
  int iis = 0; // option --iis
  int isDaemon = 0;
  int isMultiPort = 0;
  int nbPorts = 1;
  int* ports_tab;

  if(argc > 1){ // cas ou il y'a des paramètres saisis.
    for(int i=1; i<argc; i++){
      if( starts_with(argv[i],"--help") ){ // --help
        printf("usage: %s [options]\n\n\t--timeout=<n>   wait at most <n> seconds on a read (default $alarmtime)\n\t--log=FILE      append output to FILE (default stdout only)\n\t--port=<n>      listen on TCP port <n> (default $port/tcp)\n\t--max=<n>       save at most <n> chars of request (default $maxline chars)\n\t--debug         turn on a bit of debugging (mainly for developers)\n\t--apache        logs are in Apache style\n\t--iis\t\tlogs are in IIS style\n\t--daemon\trun the program as daemon(in the same directory)\n\t--version       show version info\n\n\t--help          show this listing\n",__FILE__);
        exit(0);
      }
      else if( starts_with(argv[i],"--log=")  ){ // --log=FILE
          logfile = get_param(argv[i],"--log=");
      }
      else if( starts_with(argv[i],"--port=")  ){ // --port=##
          port = atoi(get_param(argv[i],"--port="));
      }
      else if( starts_with(argv[i],"--multiport=")  ){ // --multiport=##,...,##
          isMultiPort = 1;
          char* str_ports = get_param(argv[i],"--multiport=");
          for(int j = 1; j < strlen(str_ports); j++){
            if(str_ports[j] == 44) nbPorts++; // 44 : code ASCII de la virgule (séparateur des numéros de ports)
          }
          ports_tab = getPorts(str_ports, nbPorts);
      }
      else if( starts_with(argv[i],"--timeout=")  ){ // --timeout=##
          alarmtime = atoi(get_param(argv[i],"--timeout="));
      }
      else if( starts_with(argv[i],"--max=")  ){ // --max=##
          maxline = atoi(get_param(argv[i],"--max="));
      }
      else if( starts_with(argv[i],"--debug")  ){ // --debug
          debug = 1;
      }
      else if( starts_with(argv[i],"--apache")  ){ // --apache
          apache = 1;
      }
      else if( starts_with(argv[i],"--iis")  ){ // --iis
          iis = 1;
      }
      else if( starts_with(argv[i],"--daemon")  ){ // --daemon
          isDaemon = 1;
      }
      else if( starts_with(argv[i],"--version")  ){ // --version
          printf("websnarf v%s -- http://www.unixwiz.net/tools/websnarf.html\n",VERSION);
          exit(0);
      }
      else{
        if(! starts_with(argv[0],"websnarf")  ){
          printf("Erreur : tapez --help pour afficher l'aide.\n");
          exit(-1);
        } else {
          // On est dans un fils
          printf("On est dans le fils : %s\n",argv[0]);
        }
      }
    }
  }

  if(iis == 1 && apache == 1){
    perror("Format de log IIS et apache incompatibles");
    exit(-1);
  }

  if(isMultiPort){
    // TODO : faire un string par paramètre, même si le paramètre n'est pas utilisé.
    int ret[nbPorts];
    char process_name[20];
    char param_unique[1024];
    char parametres[1024];
    char str_port_param[20];
    int j;
    int status = 0;
    // Pour chaque port, on créer un fils a port unique
    for(int i = 0; i < nbPorts; i++){
      // Copie des parametres
      for(j = 1; j < argc; j++){
        if(! starts_with(argv[j],"--multiport=")){
          strcpy(param_unique, argv[j]);
          printf("param_unique : %s\n",param_unique);
          strcat(parametres, param_unique);
        }
      }
      // Ajout du port a la liste des paramètres
      sprintf(process_name,"websnarf_%d",ports_tab[i]);
      sprintf(str_port_param, "--port=%d\n",ports_tab[i]);

      strcat(parametres, str_port_param);
      printf("parametres : %s\n", parametres);
      fflush(stdout);

      // Création du fils
      switch (ret[i] = fork()) {
        case(pid_t) -1 :
                        perror("création impossible");
                        exit(-1);
                        break;
        case(pid_t) 0 : // On est dans le fils
                        // La ligne ci-dessous permet de vérifier (entre autres) que les numéros de pipe sont bien disctinct pour chaques fils
                        //printf("read_tube : %s\twrite_tube : %s\tfichier_finale : %s\ttaille_enetete : %s\tidFils : %s\n", read_tube, write_tube, fichier_finale, taille_entete, idFils);
                        execl("./websnarf", process_name, parametres, NULL);
                        perror("execl fail");
                        exit(-1);
                        break;

        default : // On est dans le pere
                        for(int i = 0; i < nbPorts; i++){
                          waitpid(ret[i], &status, 0);
                        }
                        exit(1);
      }
    }
  }

  if(isDaemon){
    daemon(1, 0);
  }

  // -----------------------------------------------------------------------
  // Create the socket to listen on. It's a fatal error if we cannot listen
  // on port 80, the most common reasons being (a) we're not root or
  // (b) something else is already listening on that. Is Apache running?
  //

  int newsockfd, sock, s;

  sock = creersock (port, alarmtime);

  if (sock<0) {
    perror ("ERREUR OUVERTURE");
    exit(-1);
  }

  // -----------------------------------------------------------------------
  // CREATE LOGFILE
  //
  // ... but only if requested with --log=FILE
  //
  FILE* file;
  int mustLog = 0;
  char str_affiche[BUFSIZ];
  char string_to_log[BUFSIZ];
  if(strlen(logfile) > 0){
    file = fopen(logfile, "rb+");
    if(file == NULL){
        file = fopen(logfile, "wb");
    }
    mustLog = 1;
    file = fopen(logfile, "a+");

    sprintf(str_affiche, "# Now listening on port %d\n",port);
    print_or_log(str_affiche, mustLog, file);
    fflush(stdout);
  }

  sprintf(str_affiche, "%s v%s listening on port %d (timeout=%d secs)\n", argv[0], VERSION, port, alarmtime);
  print_or_log(str_affiche, mustLog, file);
  fflush(stdout);

  char our_ip [512];
  char their_ip [512];
  char request [2048];

  char printed_requete[512];

  struct sockaddr_in client_addr;
  socklen_t clen = sizeof(client_addr);
  struct sockaddr_in server_addr;
  socklen_t slen = sizeof(server_addr);
  struct hostent *client_name;

  while(1){
    listen (sock,5);

    // Accepte une connexion TCP. Si après (alarmtime) secondes aucunes connexion n'est établie, retourne -1.
    newsockfd = accept(sock, (struct sockaddr *)&client_addr, &clen);

    if(newsockfd != -1){
      getsockname(newsockfd, (struct sockaddr *)&server_addr, &slen);
      getpeername(newsockfd, (struct sockaddr *)&client_addr, &clen);

      client_name = gethostbyaddr(&client_addr, sizeof(client_addr), AF_INET);

      if(client_name != NULL) {
        printf("Client name : %s\n", client_name->h_name);
      }

      // Calcul du string de notre IP
      sprintf(our_ip, "%s", inet_ntoa(server_addr.sin_addr));
      // Calcul du string de l'IP distante
      sprintf(their_ip, "%s", inet_ntoa(client_addr.sin_addr));


      if(debug){
        sprintf(str_affiche, "--> accepted connection from %s\n",their_ip);
        print_or_log(str_affiche, mustLog, file);
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
              sprintf(str_affiche, "  client ready to read, now reading\n");
              print_or_log(str_affiche, mustLog, file);
              fflush(stdout);
            }

            request[s] = 0;

            if(debug){
              sprintf(str_affiche, "  got read #%d of [%ld]\n", 1,strlen(request) );
              print_or_log(str_affiche, mustLog, file);
              sprintf(str_affiche, "[%s]\n",request);
              print_or_log(str_affiche, mustLog, file);
              fflush(stdout);
            }

            // On ne prend que la première ligne de la requete
            if(!iis){
              get_request(request, printed_requete);
            }

            // Récupération de l'heure de la requête
            if(apache) {
              apache_display_log(their_ip, printed_requete, string_to_log);
            } else if(iis) {
              iis_display_log(our_ip, their_ip, printed_requete, string_to_log);
            } else {
              display_log(our_ip, their_ip, printed_requete, string_to_log);
            }
            sprintf(str_affiche,"%s", string_to_log);
            print_or_log(str_affiche, mustLog, file);
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
