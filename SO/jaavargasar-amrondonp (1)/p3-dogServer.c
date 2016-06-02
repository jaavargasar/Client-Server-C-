/***
SERVER of information of dogs
Created by: Andres Mauricio Rondon Patiño and Jaime Andres Vargas Arevalo
on 18 october 2015
 */
# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <arpa/inet.h>
# include <netinet/in.h>
# include <netdb.h>
# include <strings.h>
# include <time.h>

# include <fcntl.h>
# include <sys/stat.h>
# include <semaphore.h>

//Maximum number of dogs supported
# define MAX_SIZE 1000000

//Maximum number of clients supported
# define MAX_CLIENTS 32

//Constants the choose the desired method to synchronize the access to the system's log file
# define PIP 0
# define MUT 1
# define SEM 0

/***The actual number of dogs in the system*/
int number_of_dogs;

/***The descriptor of the server_socket*/
int server_socket;

/**Boolean variable for knowing the state of a thread asociated with a client*/
int isFree[MAX_CLIENTS];

/**Pipe to control the access of the proceses to the datastructure*/
int pipefd[2];

/**Array with the information of the socket for all the clients*/
struct sockaddr_in client[MAX_CLIENTS];

/**Semaphore to manage the access for the writers*/
sem_t *writer_semaphore;

/**Semaphore to manage the access for the readers*/
sem_t *reader_semaphore;

/**Semaphore to manage the access for the writers in the system log*/
sem_t *log_semaphore;

/**pipe to manage the access for the writers in the system log*/
int pipefd[2];

/**mutex to manage the access for the writers in the system log*/
pthread_mutex_t mutex;

/**Number of readers in a instant of time this is used in order to manage the readers-writers problem*/
int number_of_readers=0;

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

/***
Pointer to an array which holds the information of all the dogs.
The maximun capacity of the system is MAX_SIZE
 */
struct Dog * dogs;

/***Functions headers*/

/**Append to the underlying datastructure a new dog which has been read from a client
PARAMETERS
void * buf : a buffer containing an integer in the first 4 bytes and the dog in the next bytes
 */
void create_registry(void * buf);

/**Send the list of all the names to the client, wait for a client choise and send the desired dog
PARAMETERS
int sock_cli : the descriptor of the socket of the client
 */
void list_registry(int sock_cli);

/**Send all the names of the dosg to the client, wait for a client choise
and finally erases the dog choosed by the client
PARAMETERS
int sock_cli : the descriptor of the socket of the client
RETURN VALUE
int : the number of registry erased
 */
int erase_registry(int sock_cli);

/**Recieves a string from the client, search for matches and send all the matches
then, wait for a choise from the client and finally send the dog choosed
PARAMETERS
int sock_cli : the descriptor of the socket of the client
RETURN VALUE
char * : apointer to the name searched
 */
char * search_registry(int sock_cli);

/**init the system. it searchs for information in the file 'dataDogs.dat'
if the file is not present, it initalize in blank the system*/
void charge_system();

/**Saves the current state of the system, in the file 'dataDogs.dat'
if the file exists, it overwrites it*/
void save_system();

/**Creates the server_socket and sets the descriptor*/
void  configurate_socket();

/**A thread function which manages the conection of a single client
Recieves and executes the comands from the client, calls the necessary functions
and writes the log of the server
PARAMETERS
void * args : a buffer with 2 4-bytes integers , the descriptor of
de client's socket and the number of the thread respectively
 */
void *manage_client(void * args);

/**Writes to the log of the system
PARAMETERS
int client_index: the client that made the request
int op: the option the client choose
char * args: aditional information for writing the log*/
void sys_log(int client_index, int op, char * args);

/**Converts an integer to an string
PARAMETERS
int x: the number to be converted
RETURN VALUE
char * : a ponter to the string*/
char * toString(int x);


/**Routine which manages the semaphores in order to GET access array dogs. this routine is only
valid for a read-only thread*/
void getDogResource();

/**Routine which manages the semaphores in order to GIVE a writer access array dogs. this routine is only
valid for a read-only thread*/
void freeDogResource();

/**__________________________________________________________MAIN ____________________________________________________*/
int main(){
	charge_system();
	pthread_t threads[MAX_CLIENTS];
	configurate_socket();

	int args[32][2];
	int errorHandle;

	int i=-1;
	for(i=0 ; i<MAX_CLIENTS ; i++){
		isFree[i]=1;
	}

	char * writer_semaphore_name = "writer_semaphore";
	writer_semaphore=sem_open(writer_semaphore_name , O_CREAT , 0666 , 1 );
	if(writer_semaphore==SEM_FAILED) perror("failed to sem open RW"), exit(-1);

	char * reader_semaphore_name = "reader_semaphore";
	reader_semaphore=sem_open(reader_semaphore_name , O_CREAT , 0666 , MAX_CLIENTS);
	if(reader_semaphore==SEM_FAILED) perror("failed to sem open LL"), exit(-1);

	char * log_semaphore_name = "log_semaphore";

	if(SEM){
		log_semaphore = sem_open(log_semaphore_name, O_CREAT , 0666 , 1 );
		if(log_semaphore==SEM_FAILED) perror("Unable to crate the semaphore for the log of the system\n") , exit(-1);
	}else if(PIP){
		errorHandle = pipe(pipefd);
		if(errorHandle==-1) perror("Error while creating the pipe\n "), exit(-1);

		char witness = 't';
		write(pipefd[1],&witness,sizeof(witness));
	}else{
		pthread_mutex_init(&mutex,NULL);
	}



	while(1){

		//Find free space for a client
		i = -1;
		int j;
		for(j=0 ; j<MAX_CLIENTS ; j++){
			if(isFree[j]==1){
				isFree[j]=0;
				i=j;
				break;
			}
		}

		if(i==-1) continue;

		int sock_cli;
		socklen_t socklen=sizeof(struct sockaddr_in);

		int handle_error;

		sock_cli = accept(server_socket,(struct sockaddr * )&client[i],&socklen);
		if(sock_cli==-1) perror("Error conectando con el cliente\n"), exit(-1);

		args[i][0] = sock_cli;
		args[i][1] = i;

		int r = pthread_create(&threads[i], NULL, &manage_client, &args[i]);
		if(r != 0)
			perror("no se pudo crear el hilo");
	}

	//Closing and unlinking the semaphores
	int fl;

	fl=sem_close(writer_semaphore);
	if(fl==-1) perror("failed to close RW"), exit(-1);
	fl=sem_unlink(writer_semaphore_name);
	if(fl==-1) perror("failed to unlink RW"), exit(-1);

	fl=sem_close(reader_semaphore);
	if(fl==-1) perror("failed to close LL"), exit(-1);
	fl=sem_unlink(reader_semaphore_name);
	if(fl==-1) perror("failed to unlink LL"), exit(-1);

	if(SEM){
		fl = sem_close(log_semaphore);
		if(fl==-1) perror("failed to close log_semaphore"), exit(-1);
		fl=sem_unlink(log_semaphore_name);
		if(fl==-1) perror("failed to unlink log_semaphore"), exit(-1);
	}else if(PIP){
		close(pipefd[0]);
		close(pipefd[1]);
	}else{
		pthread_mutex_destroy(&mutex);
	}

	return 0;
}


void create_registry(void * buf){
	struct Dog * dog = (struct Dog *)malloc(sizeof(struct Dog));

	//The 4 first bytes contains an integer because of how the protocol was defined (comand,struct)
	memcpy(dog,buf+4,sizeof(struct Dog));

	//Reading data to synchronize the threads
	int fl;
	fl=sem_wait(writer_semaphore);
	if(fl==-1)  perror("failed to wait writer_semaphore"), exit(-1);

	//Start critic region
	dogs[number_of_dogs] = *dog;
	number_of_dogs++;
	save_system();
	//End critic region

	//Writing data to synchronize the threads
	fl=sem_post(writer_semaphore);
	if(fl==-1)  perror("failed to post writer_semaphore"), exit(-1);

	free(dog);
}

void list_registry(int sock_cli){
	//A buffer which will contain all the names of the dogs registered (name1,name2,...,nameN)
	void * buf = malloc(32*number_of_dogs);
	int i;

	//Try to get the resource for ALL the readers
	getDogResource();

	//Start reading region..............

	//Filling the buffer
	for(i=0 ; i<number_of_dogs ; i++){
		memcpy(buf+(i*32),dogs[i].name,32);
	}

	//Number of bytes that will be sent
	int si = sizeof(dogs[0].name)*number_of_dogs;
	int r = send(sock_cli,&si,sizeof(int),0);
	if(r==-1) perror("Error enviado tamaño del buffer\n") , exit(-1);

	if(si!=0){
		//Sending the buffer
		r = send(sock_cli,buf,si,0);
		if(r==-1) perror("Error enviado datos de busqueda\n") , exit(-1);

		//Waiting for the choise from the client
		r = recv(sock_cli,&i,4,0);
		if(r==-1) perror("Error recibiendo los datos") , exit(-1);

		if(i>=0 && i<number_of_dogs){
			//Sending the dog choosed to the client
			r = send(sock_cli,&dogs[i],sizeof(struct Dog),0);
			if(r==-1) perror("Error al enviar la consulta\n") , exit(-1);
		}
	}
	//End of the critic region.............


	//Make resource available for writers if posible
	freeDogResource();

	free(buf);
}

char * search_registry(int sock_cli){
	//Space for name to be searched
	char * name = malloc(32);
	memset(name,0,32);

	//Size of the name
	int si;

	//Read the size of the name (how many bytes have to be read) (number_of_bytes,)
	int r = recv(sock_cli,&si,4,0);
	if(r==-1) perror("Error recibiendo el tamanio\n"), exit(-1);

	//Reading the name
	r = recv(sock_cli,name,si,0);
	if(r==-1 )perror("Error recibiendo el nombre\n"),exit(-1);

	//Buffer to send to the client, will contain is size (first 4 bytes) and the index and name of the dog
	//(36 bytes each)
	void * buf = malloc(4+(4+32)*number_of_dogs);


	int i,j;		//Iterators
	int size2;		//Size of a name matched
	int found=0;	//how many matched
	int size = strlen(name); //Size of the name recieved

	//Lower case the name
	for(i=0 ; i<size ; i++){
		if(name[i]>='A' && name[i]<='Z')
			name[i]+=32;
	}

	//To store the copy of a name because it needs to be lower case
	char aux[32];

	getDogResource();

	//Start of the critict region...
	for(i=0 ; i<number_of_dogs ; i++){

		memset(aux,0,sizeof(aux));
		size2 = strlen(dogs[i].name);

		if(size!=size2) continue;

		//Lower case the name
		for(j=0 ; j<size2 ; j++){
			if(dogs[i].name[j]>='A' && dogs[i].name[j]<='Z')
				aux[j]=dogs[i].name[j]+32;
			else
				aux[j]=dogs[i].name[j];
		}

		//Matched?
		if(strncmp(name,aux,size)==0){
			//Putting its identifier and name in the buffer
			memcpy(buf+(4+found*36),&i,4);
			memcpy(buf+(4+(found*36)+4),dogs[i].name,32);
			found++;
		}
	}

	//Put in the buffer how many matches found
	memcpy(buf,&found,4);

	//Send the buffer
	r = send(sock_cli,buf,4+found*36,0);
	if(r==-1) perror("Error enviado los datos\n"), exit(-1);

	if(found!=0){
		//Waiting a choise from the client
		r = recv(sock_cli,&i,4,0);
		if(r==-1) perror("Error recibiendo los datos") , exit(-1);

		if(i>=0 && i<number_of_dogs){
			//Sending the dog to the client.
			r = send(sock_cli,&dogs[i],sizeof(struct Dog),0);
			if(r==-1) perror("Error al enviar la consulta\n") , exit(-1);
		}
	}
	//End of the critic region

	//Make resource available for writers if posible
	freeDogResource();


	return name;
}

void charge_system(){
	FILE * file = fopen("dataDog.dat","r");
	dogs = (struct Dog * ) malloc( (sizeof(struct Dog) * MAX_SIZE) );
	if(file==0){
		printf("No se encontro archivo, iniciando sistema desde cero...\n");
		number_of_dogs=0;
	}else{
		fread(&number_of_dogs,sizeof(int),1,file);
		fread(dogs,sizeof(struct Dog),number_of_dogs,file);
		fclose(file);
	}
}

void save_system(){
	FILE * file = fopen("dataDog.dat","w");
	if(file==0){
		return;
	}else{
		fwrite(&number_of_dogs,sizeof(int),1,file);
		fwrite(dogs,sizeof(struct Dog),number_of_dogs,file);
		fclose(file);
	}
}

void configurate_socket(){
	struct sockaddr_in server,client;
	int sock_id,sock_cli;
	socklen_t socklen;

	sock_id = socket(AF_INET,SOCK_STREAM,0);

	int enable = 1;
	if (setsockopt(sock_id, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
		error("setsockopt(SO_REUSEADDR) failed");

	server.sin_family = AF_INET;
	server.sin_port = htons(6789);
	server.sin_addr.s_addr = htonl(INADDR_ANY);
	bzero(server.sin_zero,8);

	bind(sock_id,(struct sockaddr * )&server, sizeof(struct sockaddr_in));
	listen(sock_id,MAX_CLIENTS);

	server_socket = sock_id;
}


int erase_registry(int sock_cli){
	//Buffer which will contain all the dogs names
	void * buf = malloc(sizeof(dogs[0].name)*number_of_dogs);
	int i;

	//Filling the buffer
	int fl;
	fl=sem_wait(writer_semaphore);
	if(fl==-1) perror("failed to wait RW"),exit(-1);

	//Here starts the acces to the critic region


	for(i=0 ; i<number_of_dogs ; i++){
		memcpy(buf+(i*32),dogs[i].name,32);
	}

	//Number of bytes that will be sent (size of the buffer)
	int si = sizeof(dogs[0].name)*number_of_dogs;

	//Sending how many bytes will be sent next
	int r = send(sock_cli,&si,sizeof(int),0);
	if(r==-1) perror("Error enviado datos de busqueda\n") , exit(-1);

	i=-1;

	if(si!=0){
		//Sending the buffer
		r = send(sock_cli,buf,si,0);
		if(r==-1) perror("Error enviado datos de busqueda\n") , exit(-1);

		//Waiting for the client's choise
		r = recv(sock_cli,&i,4,0);
		if(r==-1) perror("Error recibiendo los datos") , exit(-1);

		//Erasing the dog
		if(i>=0 && i<number_of_dogs){
			int j=i;
			for(j=i ; j<number_of_dogs-1 ; j++){
				dogs[j]=dogs[j+1];
			}
			number_of_dogs--;
			save_system();
		}else{
			i = -1;
		}
	}

	//Here starts the acces to the critic region
	fl=sem_post(writer_semaphore);
	if(fl==-1)  perror("failed to post RW"), exit(-1);


	free(buf);
	return i;
}

void *manage_client(void * args){
	int num[2];
	memcpy(num,args,sizeof(num));

	int sock_cli = num[0];

	//How many bytes has the client sent
	int howManyBytesAfter;

	int r = recv(sock_cli,&howManyBytesAfter,4,0);
	if(r==-1) perror("Error recibiendo datos") , exit(-1);

	//Buffer for reading the client's message
	void * buf = malloc(howManyBytesAfter);
	r = recv(sock_cli,buf,howManyBytesAfter,0);
	if( r==-1) perror("error recibiendo datos 2\n"),exit(-1);

	//The option choosed by the client corresponds to the first 4 bytes of the message
	int op,erased_registry;
	char * name_query;
	memcpy(&op,buf,4);

	switch(op){
	case 1:
		create_registry(buf);
		sys_log(num[1],op,toString(number_of_dogs-1));
		break;
	case 2:
		list_registry(sock_cli);
		sys_log(num[1],op,NULL);
		break;
	case 3:
		erased_registry = erase_registry(sock_cli);
		if(erased_registry!=-1) sys_log(num[1],op,toString(erased_registry));
		break;
	case 4:
		name_query = search_registry(sock_cli);
		sys_log(num[1],op,name_query);
		break;
	default:

		break;
	}
	free(buf);
	close(sock_cli);
	isFree[num[1]]=1;
}

void sys_log(int client_index, int op, char * args){
	//Get Access to the critic region
	int errorFlag;
	if(SEM){
		errorFlag = sem_wait(log_semaphore);
		if(errorFlag==-1)  perror("failed to wait log_semaphore"), exit(-1);
	}else if(PIP){
		char w;
		read(pipefd[0],&w,sizeof(w));
	}else{
		pthread_mutex_lock(&mutex);
	}


	FILE * log;
	log = fopen("serverDogs.log","a+");
	char date[100];
	time_t now = time (0);
	strftime (date, 100, "%Y-%m-%d %H:%M:%S.000", localtime (&now));

	char *ip;
	ip=inet_ntoa(client[client_index].sin_addr);

	fwrite(date,sizeof(char),strlen(date),log);
	fwrite(" cliente ",sizeof(char),9,log);
	fwrite(ip,sizeof(char),strlen(ip),log);

	switch (op){
	case 1:
		fwrite(" insercion ",sizeof(char),11,log);
		fwrite(args,sizeof(char),strlen(args),log);
		fwrite("\n",1,1,log);
		break;
	case 2:
		fwrite(" lectura\n",sizeof(char),9,log);
		break;
	case 3:
		fwrite(" borrado ",sizeof(char),9,log);
		fwrite(args,sizeof(char),strlen(args),log);
		fwrite("\n",1,1,log);
		break;
	case 4:
		fwrite(" busqueda ",sizeof(char),10,log);
		fwrite(args,sizeof(char),strlen(args),log);
		fwrite("\n",1,1,log);
		break;
	default:
		fwrite(" non valid command\n",sizeof(char),19,log);
		break;
	}
	fclose(log);

	if(SEM){
		errorFlag = sem_post(log_semaphore);
		if(errorFlag==-1)  perror("failed to post log_semaphore"), exit(-1);
	}else if(PIP){
		char w = 't';
		write(pipefd[1],&w,sizeof(w));
	}else{
		pthread_mutex_unlock(&mutex);
	}
}

char * toString(int x){
	char *ans=malloc(8);
	memset(ans,0,8);
	char *aux=malloc(8);

	int c = 0;
	while(x>=10){
		aux[c] = (x%10)+'0';
		c++;
		x/=10;
	}
	aux[c]=x+'0';
	c++;

	int i=0;
	for(i = 0 ; i<c ; i++){
		ans[i] = aux[c-i-1];
	}
	free(aux);
	return ans;
}

void getDogResource(){
	int fl;
	fl=sem_wait(reader_semaphore);

	if(fl==-1)  perror("failed to wait reader_semaphore"), exit(-1);
	number_of_readers++;

	//If it is the first reader, compete for the resource! if it is not. it already has the resource
	if(number_of_readers==1){
		fl=sem_wait(writer_semaphore);
		if(fl==-1)  perror("failed to wait writer_semaphore"), exit(-1);
	}

	fl=sem_post(reader_semaphore);
	if(fl==-1)  perror("failed to post reader_semaphore"), exit(-1);
}

void freeDogResource(){
	int fl;
	fl=sem_wait(reader_semaphore);

	if(fl==-1)  perror("failed to wait reader_semaphore"), exit(-1);
	number_of_readers--;

	//If it is the last reader, make free the resource
	if(number_of_readers==0){
		fl=sem_post(writer_semaphore);
		if(fl==-1)  perror("failed to wait writer_semaphore"), exit(-1);
	}

	fl=sem_post(reader_semaphore);
	if(fl==-1)  perror("failed to post reader_semaphore"), exit(-1);
}






