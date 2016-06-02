/***
CLIENT of information of dogs
Created by: Andres Mauricio Rondon Patiño and Jaime Andres Vargas Arevalo
on 18 october 2015
 */
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <netinet/in.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <strings.h>
# include <netdb.h>

/** ip of the server*/
char * ip;

/**Socket*/
int client_socket;

/***Struct that holds the information about dogs
this includes its name, age, race, height, weight
and gender
 */
struct Dog{
	char name[32];  //The name of the dog
	int age;        //The age in years
	char race[32];  //The race of the dog
	int height;     //the height in centimeters
	float weight;   //The weight in kilograms
	char gender;    //The gender M for male, F for female
};

/***Functions headers*/

/*Asks for a dog infromation and sends the comands to the server*/
void create_registry();

/**Ask the server for the list of all the dogs in the system, chooses one
and send the server which dog the user want to see in detail*/
int list_registry();

/**Send a name to search to the server and ask it to
search and shows all the registers that match with the
given name. Then, asks the user which of those registers
he want to see in detail.
PARAMETERS
char * name: pointer to the string 'name' */
void search_registry(char * name);

/**Lists all the dogs in the system and asks the user for a register to chooses
then send the comand to the server*/
void erase_registry();

/**Pause the execution*/
void a_pause();

/**A function which shows the main menu and asks the user for a comand
RETURN VALUE
int the choice of the user o -1 if the choice doesn't exit*/
int show_menu();

/**Aks the user if they are sure of doing some changes
RETURN VALUE
a char 'S' if the user is sure a 'N' if is not*/
char are_you_sure();

/**Emptys the stdin buffer*/
void empty_buffer();

/**Sends data to the server specified by 'ip' (ip of the server) global variable
PARAMETERS
void * buf : A pointer to the data to be sent
int bytes  : How many bytes of that data will be sent */
void send_to_server(void * buf, int bytes);

/**Shows on stdout the information of the dog pointed
by the parameter pointer
PARAMETERS
struct Dog * dog : a pointer to a dog*/
void show_dog(struct Dog * dog);

/**__________________________________________________________MAIN ____________________________________________________*/
int main(int argc, char * argv[] ){
	if(argc==1){
		ip="127.0.0.1";
	}else{
		ip = argv[1];
	}

	//Option choosen
	int op;
	while( (op = show_menu()) !=5){
		switch(op){
		case 1:
			create_registry();
			break;
		case 2:
			list_registry();
			break;
		case 3:
			erase_registry();
			break;
		case 4:
			printf("Ingrese el nombre a buscar: ");
			char name[32];
			scanf("%s",name);
			empty_buffer();
			search_registry(name);
			break;
		}
	}
	a_pause();
	return 0;
}

void create_registry(){

	struct Dog dog;

	printf("Ingrese el nombre : ");
	scanf("%s",dog.name);
	empty_buffer();

	printf("Ingrese la edad: ");
	scanf("%d",&dog.age);
	empty_buffer();

	printf("Ingrese la raza: ");
	scanf("%s",dog.race);
	empty_buffer();

	printf("Ingrese la altura: ");
	scanf("%d",&dog.height);
	empty_buffer();

	printf("Ingrese el peso: ");
	scanf("%f",&dog.weight);
	empty_buffer();

	printf("Ingrese el genero Masculino [M] o femenino [F]: ");
	scanf("\n%c",&dog.gender);
	empty_buffer();

	if(dog.gender!='M'){
		if(dog.gender == 'm')
			dog.gender='M';
		else
			dog.gender = 'F';
	}
	if(are_you_sure()=='N') return;

	//Creating the package to send (numberOfBytes,comand,struct)
	void * buf = malloc( sizeof(int)*2 + sizeof(struct Dog) );
	int howMany = sizeof(int) + sizeof(struct Dog) ;
	int comand = 1;

	memcpy(buf,&howMany,sizeof(howMany));
	memcpy(buf+4,&comand,sizeof(comand));
	memcpy(buf+8, &dog ,sizeof(struct Dog));

	send_to_server( buf , sizeof(int)*2 + sizeof(struct Dog)  );

	a_pause();
}

int list_registry(){
	//Creating the package (numberOfBytes,comand)
	int op[2];
	op[0] = 4;
	op[1] = 2;

	//Send package
	send_to_server(op,8);

	//Recieving the number of bytes to read next
	int s;
	int r = recv(client_socket ,&s,sizeof(int),0);
	if(r==-1) perror("Error enviado infromacion\n"), exit(-1);

	int i=-1;
	if(s!=0){
		//Reading the package (name1,name2,name3...nameN)
		void * buf = malloc(s);
		r = recv(client_socket ,buf,s,0);
		if(r==-1) perror("Error linea 249\n"), exit(-1);

		printf("Numero de registro             Nombre\n\n");
		for(i=0 ; i< s/32 ; i++){
			//Printing the ith name
			printf("%18d             %s\n",i,(char*)(buf+i*32) );
		}

		printf("\nSeleccione un numero de registro: ");
		scanf("%d",&i);
		empty_buffer();

		if(i<0 || i>=(s/32)) {
			printf("No es un registro valido\n");
			i = -1;
		}

		//Send number of register -1 if it is not a valid registry
		r = send(client_socket ,&i,4,0);

		if(i!=-1){
			struct Dog dog;
			//Recieving the dog wanted
			r = recv(client_socket ,&dog,sizeof(struct Dog),0);

			show_dog(&dog);
		}
	}else{
		printf("No se encontraron registros\n");
	}

	return i;
}

void search_registry(char * name){
	//Creating the option package (numberOfBytes,comand)
	int op[2];
	op[0]=4;
	op[1]=4;
	//Send the 'option package'
	send_to_server(op,8);

	int sss = strlen(name);

	//Send the length of the name
	int r = send(client_socket,&sss,4,0);
	if(r==-1) perror("Error al mandar size\n "), exit(-1);

	//Send the name
	r = send(client_socket,name,sss,0);
	if(r==-1) perror("Error al mandar nombre\n"),exit(-1);

	int found;

	//Recieve how many coincidences were found
	r = recv(client_socket,&found,4,0);
	if(r==-1) perror("Error al recibir archivos encontrados\n"), exit(-1);

	if(found==0){
		printf("\nNo se encontraron resultados\n");
		a_pause();
	}
	else{

		void * buf= malloc(found*36);
		//Recieve the package from the server (id,name,id2,name2,...,idN,nameN)
		r = recv(client_socket,buf,found*36,0);

		int aux;
		char nn[32];
		int i;

		printf("\nNumero de registro             Nombre\n\n");

		for(i=0 ; i<found ; i++){
			memcpy(&aux,buf+i*36,4);
			memcpy(nn,buf+i*36+4,32);

			printf("%18d             %s\n",aux,nn);
		}


		printf("\n%d concidencias encontradas\n\n",found);
		printf("Seleccione el registro a consultar: ");
		int reg;
		scanf("%d",&reg);
		empty_buffer();

		//Send the id of dog wanted to be seen
		r = send(client_socket,&reg,4,0);
		if(r==-1) perror("Error al enviar el registro\n"),exit(-1);

		//Recieve the dog
		struct Dog dog;
		r = recv(client_socket,&dog,sizeof(struct Dog),0);
		if(r==-1)perror("Error al recibir el registro\n"),exit(-1);
		show_dog(&dog);
	}
}

void erase_registry(){
	//Data to be sent to the server following the protocol (numberOfBytes,comand)
	int op[2];
	op[0] = 4;
	op[1] = 3;

	send_to_server(op,8);

	int s;

	//Recieving the number of bytes to read next
	int r = recv(client_socket ,&s,sizeof(int),0);
	if(r==-1) perror("Error recieving data\n"), exit(-1);

	void * buf = malloc(s);

	if(s!=0){
		//Reading the package (name1,name2,name3...nameN)
		r = recv(client_socket ,buf,s,0);
		if(r==-1) perror("Error recieving data\n"), exit(-1);

		int i;
		printf("Numero de registro             Nombre\n\n");
		for(i=0 ; i< s/32 ; i++){
			printf("%18d             %s\n",i,(char*)(buf+i*32) );
		}

		printf("\nSeleccione un numero de registro: ");
		scanf("%d",&i);
		empty_buffer();

		if(i<0 || i>=(s/32)) {
			printf("No es un registro valido\n");
			i=-1;
		}
		if(i!=-1){
			char x = are_you_sure();
			if(x=='N') i = -1;
		}
		//Send the number of registry choosen to the server
		r = send(client_socket ,&i,4,0);
		if(i!=-1 && r!=-1) printf("\n\nRegistro borrado\n");
	}else{
		printf("No se encontraron registros\n");
	}
	free(buf);
}

void show_dog(struct Dog * dog){
	printf("\nNombre: %s\n",dog->name);
	printf("Edad: %d\n",dog->age);
	printf("Raza: %s\n",dog->race);
	printf("Altura: %dcm\n",dog->height);
	printf("Peso: %.2fkg\n",dog->weight);
	printf("Genero: %c\n\n",dog->gender);
	a_pause();
}

int show_menu(){
	int op;
	printf("***************SELECCIONE UNA OPCION***************\n");
	printf("1. Ingresar registro\n");
	printf("2. Ver registro\n");
	printf("3. Borrar registro\n");
	printf("4. Buscar registro\n");
	printf("5. Salir\n");

	scanf("%d",&op);
	empty_buffer();

	if(op<1 || op>5) op=-1;
	return op;
}

char are_you_sure(){
	printf("Continuar y guardar cambios? [S,N]: ");
	char s;
	scanf("\n%c",&s);
	empty_buffer();

	if(s=='S' || s=='s'){
		printf("\nCambios guardados\n");
		return 'S';
	}
	printf("\nCambios no guradados\n");
	return 'N';
}

void empty_buffer(){
	char ch;
	while ((ch = getchar()) != '\n' && ch != EOF);
}

void a_pause(){
	printf("Presione una tecla para continuar...\n");
	getchar();
}

void send_to_server(void * buf, int bytes){
	socklen_t socklen;
	struct hostent * x;
	struct sockaddr_in server;
	int socket_id;

	x = gethostbyname(ip);

	socket_id = socket(AF_INET,SOCK_STREAM,0);
	if(socket_id == -1) perror("Error creating the socket\n") , exit(-1);

	int enable = 1;
	if (setsockopt(socket_id, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		error("setsockopt(SO_REUSEADDR) failed");

	server.sin_family = AF_INET;
	server.sin_port = htons(6789);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	bzero(server.sin_zero,0);

	int r = connect(socket_id, (struct sockaddr *) & server, sizeof(server));
	if(r==-1) perror("Error creando conexion con servidor\n") ,exit(-1);

	send(socket_id, buf , bytes ,0);
	client_socket = socket_id;
}

