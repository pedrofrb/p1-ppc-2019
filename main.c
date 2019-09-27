#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <sched.h>
#include <unistd.h>

#include "lib/FIFO.c"

#define NMAQUINAS 10
#define NSECADORAS 10
#define NMESASPASSAR 5
#define NCLIENTESDEFAULT 5

typedef struct{
    int idMuda;
    int idAparelhoAnterior;
} muda_t;


typedef struct{
    sem_t maquinas[NMAQUINAS];
    sem_t secadoras[NSECADORAS];
    sem_t mpassar[NMESASPASSAR];
	sem_t funcMutex;
    sem_t filaMutex;
    sem_t roupasLavadasMutex;
    sem_t roupasSecasMutex;
    fifo_t fila;
    fifo_t roupasLavadas;
    fifo_t roupasSecas;

	
} shared_data_t;

typedef struct{
    int identificacao;
    shared_data_t *shared_data_p;
} thread_data_t;

void *MaquinaLavar(void *);
void *Secadora(void *);
void *MesaPassar(void *);
void *GeradorClientes(void *);

int main(){
    thread_data_t dataMaquinas[NMAQUINAS];
    thread_data_t dataSecadoras[NSECADORAS];
    thread_data_t dataMesasPassar[NMESASPASSAR];
    thread_data_t dataGeradorCliente;
    shared_data_t shared_data;
    int i;

    shared_data.fila = fifo_create(NCLIENTESDEFAULT,sizeof(muda_t));
    shared_data.roupasLavadas = fifo_create(NCLIENTESDEFAULT,sizeof(muda_t));
    shared_data.roupasSecas = fifo_create(NCLIENTESDEFAULT,sizeof(muda_t));

    //Criar os semaforos

    for (i=0; i<NSECADORAS; i++){
        sem_init(&shared_data.secadoras[i],0,1);
    }

    for (i=0; i<NMAQUINAS; i++){
        sem_init(&shared_data.maquinas[i],0,1);
    }

    for (i=0; i<NMESASPASSAR; i++){
        sem_init(&shared_data.mpassar[i],0,1);
    }

    sem_init(&shared_data.funcMutex,0,1);

    sem_init(&shared_data.filaMutex,0,1);

    sem_init(&shared_data.roupasLavadasMutex,0,1);

    sem_init(&shared_data.roupasSecasMutex,0,1);


    //Criar as threads

	pthread_t threadsMaquinas[NMAQUINAS];
    pthread_t threadsSecadoras[NSECADORAS];
    pthread_t threadsMesasPassar[NMESASPASSAR];

    pthread_t threadGeradorClientes;  

	for (i=0; i<NMAQUINAS; i++){
        dataMaquinas[i].identificacao=i;
        dataMaquinas[i].shared_data_p=&shared_data;

        pthread_create(&threadsMaquinas[i],NULL,MaquinaLavar, &dataMaquinas[i]);
    }

    
	for (i=0; i<NSECADORAS; i++){
        dataSecadoras[i].identificacao=i;
        dataSecadoras[i].shared_data_p=&shared_data;

        pthread_create(&threadsSecadoras[i],NULL,Secadora, &dataSecadoras[i]);
    }


	for (i=0; i<NMESASPASSAR; i++){
        dataMesasPassar[i].identificacao=i;
        dataMesasPassar[i].shared_data_p=&shared_data;

        pthread_create(&threadsMesasPassar[i],NULL,MesaPassar, &dataMesasPassar[i]);
    }

    dataGeradorCliente.identificacao=1;
    dataGeradorCliente.shared_data_p=&shared_data;
    pthread_create(&threadGeradorClientes,NULL,GeradorClientes,&dataGeradorCliente);


    //Inicializar as threads

    for (i=0; i<NMAQUINAS; i++){
        pthread_join(threadsMaquinas[i],NULL);
    }

    for (i=0; i<NSECADORAS; i++){
        pthread_join(threadsSecadoras[i],NULL);
    }

    for (i=0; i<NMESASPASSAR; i++){
        pthread_join(threadsMesasPassar[i],NULL);
    }

    pthread_join(threadGeradorClientes,NULL);

    //Destruir semaforos

    sem_destroy(&shared_data.funcMutex);
    sem_destroy(&shared_data.filaMutex);
    sem_destroy(&shared_data.roupasLavadasMutex);
    sem_destroy(&shared_data.roupasSecasMutex);


    for (i=0; i<NSECADORAS; i++){
        sem_destroy(&shared_data.secadoras[i]);
    }

    for (i=0; i<NMESASPASSAR; i++){
        sem_destroy(&shared_data.mpassar[i]);
    }

    for (i=0; i<NMAQUINAS; i++){
        sem_destroy(&shared_data.maquinas[i]);
    }
    
    // fifo_t fila;
    // fila =fifo_create(NCLIENTESDEFAULT,sizeof(int));
    // int valor=2;
    // fifo_add(fila,&valor);
    // valor=0;
    // printf("%d\n",&valor);
    // fifo_get(fila,&valor);
    // printf("%d\n\n\n\n",&valor);
}

void *GeradorClientes(void* thread_data){
    int identificacao=((thread_data_t*)thread_data)->identificacao;
    shared_data_t *data=((thread_data_t*)thread_data)->shared_data_p;
    printf("Iniciando geracao...");
    int i;
    
    for(i=0;i<NCLIENTESDEFAULT;i++){
        muda_t novaMuda;
        novaMuda.idAparelhoAnterior=NULL;
        novaMuda.idMuda=i;
        sem_wait(&data->filaMutex);
        fifo_add(data->fila,&novaMuda);
        printf("\nCliente %d chegou\n",i);
        sem_post(&data->filaMutex);
        usleep(1000); //1 seg ou usleep para microsegundos
    }

}

void *MaquinaLavar(void* thread_data){
    int identificacao=((thread_data_t*)thread_data)->identificacao;
    shared_data_t *data=((thread_data_t*)thread_data)->shared_data_p;
    muda_t mudaAtual;
    while(1){
        sem_wait(&data->maquinas[identificacao]);
        sem_wait(&data->filaMutex);
        if(fifo_is_empty(data->fila)){
            sem_post(&data->filaMutex);
            sem_post(&data->maquinas[identificacao]);
            continue;
        }
        sem_wait(&data->funcMutex);
        
        fifo_get(data->fila,&mudaAtual);
        printf("\nFuncionario: Colocando muda %d na maquina %d\n",mudaAtual.idMuda,identificacao);
        sem_post(&data->filaMutex);
        sem_post(&data->funcMutex);
        usleep(1000);
        printf("\nMáquina %d acabou o processo da muda %d\n",identificacao,mudaAtual);
        mudaAtual.idAparelhoAnterior=identificacao;
        sem_wait(&data->roupasLavadasMutex);
        fifo_add(data->roupasLavadas,&mudaAtual);
        sem_post(&data->roupasLavadasMutex);
        
        
    }
}

void *Secadora(void* thread_data){
    int identificacao=((thread_data_t*)thread_data)->identificacao;
    shared_data_t *data=((thread_data_t*)thread_data)->shared_data_p;
    muda_t mudaAtual;
    while(1){
        sem_wait(&data->secadoras[identificacao]);
        sem_wait(&data->roupasLavadasMutex);
        if(fifo_is_empty(data->roupasLavadas)){
            sem_post(&data->roupasLavadasMutex);
            sem_post(&data->secadoras[identificacao]);
            continue;
        }
        sem_wait(&data->funcMutex);
        
        fifo_get(data->roupasLavadas,&mudaAtual);
        sem_post(&data->maquinas[mudaAtual.idAparelhoAnterior]);
        printf("\nFuncionario: Colocando muda %d na secadora %d\n",mudaAtual.idMuda,identificacao);
        sem_post(&data->roupasLavadasMutex);
        sem_post(&data->funcMutex);
        usleep(1000);
        printf("\nSecadora %d acabou o processo da muda %d\n",identificacao,mudaAtual);
        mudaAtual.idAparelhoAnterior=identificacao;
        sem_wait(&data->roupasSecasMutex);
        fifo_add(data->roupasSecas,&mudaAtual);
        sem_post(&data->roupasSecasMutex);
        // sem_post(&data->secadoras[identificacao]); //tirar dps q isso é de resp da mesa de passar
    }

}

void *MesaPassar(void* thread_data){
    int identificacao=((thread_data_t*)thread_data)->identificacao;
    shared_data_t *data=((thread_data_t*)thread_data)->shared_data_p;
    muda_t mudaAtual;
    while(1){
        sem_wait(&data->mpassar[identificacao]);
        sem_wait(&data->roupasSecasMutex);
        if(fifo_is_empty(data->roupasSecas)){
            sem_post(&data->roupasSecasMutex);
            sem_post(&data->mpassar[identificacao]);
            continue;
        }
        sem_wait(&data->funcMutex);
        fifo_get(data->roupasSecas,&mudaAtual);
        sem_post(&data->secadoras[mudaAtual.idAparelhoAnterior]);
        printf("\nFuncionario: Passando muda %d na mesa de passar %d\n",mudaAtual.idMuda,identificacao);
        sem_post(&data->roupasSecasMutex);
        // sem_post(&data->funcMutex);
        usleep(1000);
        printf("\nFuncionario acabou na mesa %d  o processo de passar a muda %d\n",identificacao,mudaAtual);
        sem_post(&data->funcMutex);
        // mudaAtual.idAparelhoAnterior=identificacao;
        // sem_wait(&data->roupasSecasMutex);
        // fifo_add(data->roupasSecas,&mudaAtual);
        // sem_post(&data->roupasSecasMutex);
        sem_post(&data->mpassar[identificacao]);
    }
}

// void *Filosofo(void* thread_data){
//     int filosofo=((thread_data_t*)thread_data)->filosofo;

//     sem_t *garfos=((thread_data_t*)thread_data)->shared_data_p->garfos;
//     sem_t *mutex=&((thread_data_t*)thread_data)->shared_data_p->mutex;

//     printf("Eu o Filosofo %d\n",filosofo);

//     int esquerda=filosofo;
//     int direita=((filosofo+1)%NFILOSOFOS);

//     //Pegar o garfo da esquerda:
//     int n,value;
//     for (n=0 ;n<1000 ; n++){

//         // Mutex antes de entrar na RC
//         sem_wait(mutex);

//         /*
//         Testa o valor do garfo a esquerda
//         se for 0, liberta o mutex
//         senao, pega o garfo a esquerda
//         */
//         sem_getvalue(&garfos[esquerda],&value);
//         if (value==1){
//             sem_wait(&garfos[esquerda]);
//         }else{
//             sem_post(mutex);
//             sched_yield();
//         }

//         /*
//         Testa o valor do garfo a direita
//         se for 0, liberta o mutex e o garfo a esquerda
//         senao, pega o garfo a direita
//         */
//         sem_getvalue(&garfos[direita],&value);
//         if (value==1){
//             sem_wait(&garfos[direita]);
//         }else{
//             sem_post(&garfos[esquerda]);
//             sem_post(mutex);
//             sched_yield();
//         }

//         usleep(1000);

//         printf("%d: O filosofo %d estah comendo!\n",n++,filosofo);

//         sem_post(&garfos[esquerda]);
//         sem_post(&garfos[direita]);

//         sem_post(mutex);
//     }


    //sem_wait(data->semaphore);
	//printf("Sou a thread %d. Dormindo ... \n",data->rank);
	//sleep(5);

