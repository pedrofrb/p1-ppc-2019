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
#define NCLIENTESDEFAULT 100

typedef struct{
    int idMuda;
    int idAparelhoAnterior;
} muda_t;


typedef struct{
    int numClientes;
    sem_t maquinas[NMAQUINAS];
    sem_t secadoras[NSECADORAS];
    sem_t mpassar[NMESASPASSAR];
	sem_t funcMutex;
    sem_t filaMutex;
    sem_t roupasLavadasMutex;
    sem_t roupasSecasMutex;
    sem_t totalMudasLavadasMutex;
    fifo_t fila;
    fifo_t roupasLavadas;
    fifo_t roupasSecas;
    int totalMudasLavadas;

	
} shared_data_t;

typedef struct{
    int identificacao;
    shared_data_t *shared_data_p;
} thread_data_t;

void *MaquinaLavar(void *);
void *Secadora(void *);
void *MesaPassar(void *);
void *GeradorClientes(void *);

int main(int argc, char *argv[]){
   
    

    thread_data_t dataMaquinas[NMAQUINAS];
    thread_data_t dataSecadoras[NSECADORAS];
    thread_data_t dataMesasPassar[NMESASPASSAR];
    thread_data_t dataGeradorCliente;
    shared_data_t shared_data;
    int i;

    if(argc>1){
        shared_data.numClientes = atoi(argv[1]); 
    }else{
        shared_data.numClientes =NCLIENTESDEFAULT;
    }

    shared_data.fila = fifo_create(shared_data.numClientes,sizeof(muda_t));
    shared_data.roupasLavadas = fifo_create(shared_data.numClientes,sizeof(muda_t));
    shared_data.roupasSecas = fifo_create(shared_data.numClientes,sizeof(muda_t));
    shared_data.totalMudasLavadas=0;

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

    sem_init(&shared_data.totalMudasLavadasMutex,0,1);


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
    sem_destroy(&shared_data.totalMudasLavadasMutex);


    for (i=0; i<NSECADORAS; i++){
        sem_destroy(&shared_data.secadoras[i]);
    }

    for (i=0; i<NMESASPASSAR; i++){
        sem_destroy(&shared_data.mpassar[i]);
    }

    for (i=0; i<NMAQUINAS; i++){
        sem_destroy(&shared_data.maquinas[i]);
    }
    
    printf("Fim da simulacao\n");
}

void *GeradorClientes(void* thread_data){
    int identificacao=((thread_data_t*)thread_data)->identificacao;
    shared_data_t *data=((thread_data_t*)thread_data)->shared_data_p;
    printf("Inicio da simulacao\n");
    int i;
    
    for(i=0;i<data->numClientes;i++){
        muda_t novaMuda;
        novaMuda.idAparelhoAnterior=-1;
        novaMuda.idMuda=i;
        sem_wait(&data->filaMutex);
        fifo_add(data->fila,&novaMuda);
        printf("Um cliente  chegou com uma muda : %d (Tamanho da fila=%d)\n",i,fifo_size(data->fila));
        sem_post(&data->filaMutex);
        usleep(1000);
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
            if(data->totalMudasLavadas==data->numClientes){
                sem_post(&data->filaMutex);
                sem_post(&data->maquinas[identificacao]);
                pthread_exit(NULL);
            }
            sem_post(&data->filaMutex);
            sem_post(&data->maquinas[identificacao]);
            continue;
        }
        sem_wait(&data->funcMutex);
        
        fifo_get(data->fila,&mudaAtual);
        printf("Funcionario: coloquei a MUDA %d na MAQUINA %d\n",mudaAtual.idMuda,identificacao);
        sem_post(&data->filaMutex);
        sem_post(&data->funcMutex);
        usleep(1000);
        printf("MAQUINA %d LIVRE\n",identificacao);
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
            if(data->totalMudasLavadas==data->numClientes){
                sem_post(&data->roupasLavadasMutex);
                sem_post(&data->secadoras[identificacao]);
                pthread_exit(NULL);
            }
            sem_post(&data->roupasLavadasMutex);
            sem_post(&data->secadoras[identificacao]);
            continue;
        }
        sem_wait(&data->funcMutex);
        
        fifo_get(data->roupasLavadas,&mudaAtual);
        sem_post(&data->maquinas[mudaAtual.idAparelhoAnterior]);
         printf("Funcionario: coloquei a MUDA %d na SECADORA %d\n",mudaAtual.idMuda,identificacao);
        sem_post(&data->roupasLavadasMutex);
        sem_post(&data->funcMutex);
        usleep(1000);
        printf("SECADORA %d LIVRE\n",identificacao);
        mudaAtual.idAparelhoAnterior=identificacao;
        sem_wait(&data->roupasSecasMutex);
        fifo_add(data->roupasSecas,&mudaAtual);
        sem_post(&data->roupasSecasMutex);
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
            if(data->totalMudasLavadas==data->numClientes){
                sem_post(&data->roupasSecasMutex);
                sem_post(&data->mpassar[identificacao]);
                pthread_exit(NULL);
            }
            sem_post(&data->roupasSecasMutex);
            sem_post(&data->mpassar[identificacao]);
            continue;
        }
        sem_wait(&data->funcMutex);
        fifo_get(data->roupasSecas,&mudaAtual);
        sem_post(&data->secadoras[mudaAtual.idAparelhoAnterior]);
        printf("Funcionario: estou passando a MUDA %d na TABUA %d\n",mudaAtual.idMuda,identificacao);
        sem_post(&data->roupasSecasMutex);
        usleep(1000);
        sem_post(&data->funcMutex);
        sem_wait(&data->totalMudasLavadasMutex);
        data->totalMudasLavadas++;
        sem_post(&data->totalMudasLavadasMutex);
        sem_post(&data->mpassar[identificacao]);
    }
}